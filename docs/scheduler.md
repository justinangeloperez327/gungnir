# Scheduler

Gungnir's scheduler supports deterministic interval and cron schedules without hiding distributed coordination behind in-process APIs.

## Tasks

`Scheduler::every()` registers a named action with a positive interval.

`Scheduler::cron()` registers a standard five-field cron schedule:

`minute hour day-of-month month day-of-week`

Supported cron syntax includes wildcards, lists, ascending ranges, steps, month names (`JAN`-`DEC`), weekday names (`SUN`-`SAT`), and Sunday as either `0` or `7`. When both day-of-month and day-of-week are restricted, standard cron OR semantics are used.

Convenience registrations are available through `hourly()`, `daily()`, `weekly()`, and `monthly()`.

Names must be unique within a scheduler.

## Execution

`run_due()` evaluates all tasks against the injected clock and runs those that are due. A task is marked as run only after its action completes successfully. Exceptions propagate to the caller.

Cron tasks execute at most once for each matching minute, even when `run_due()` is called repeatedly during that minute.

`next_due()` exposes the earliest next task deadline for scheduler runtimes that want deadline-aware sleeping.

## Clock and timezones

Time is provided through the `Clock` interface. Production applications can use `SystemClock`; tests can provide deterministic clocks.

A scheduler has an explicit default `TimeZone`, defaulting to UTC. Cron tasks inherit that timezone when registered and may override it fluently with `.timezone(...)`.

`FixedOffsetTimeZone` covers zones without daylight-saving transitions. `RecurringTimeZone` models named zones whose daylight transitions follow recurring month / nth-weekday / local-time rules. Transition rules are evaluated without modifying process-global timezone state.

During a spring-forward gap, nonexistent wall-clock minutes do not run. During fall-back, the repeated wall-clock minute represents one scheduled slot: once a cron task has run for that local year/month/day/hour/minute, the repeated occurrence is suppressed. This prevents accidental duplicate business actions during DST rollback.

Recurring transition rules define the start time in pre-transition standard time and the end time in pre-transition daylight time.

## Production runner

`Scheduler::run()` provides the long-running production loop. It executes due work, computes the next scheduled deadline, and sleeps until that deadline or `RunnerOptions::maximum_sleep`, whichever is earlier. The bounded maximum sleep lets the runner re-evaluate wall-clock changes without aggressive polling.

The runner accepts a Gungnir `CancellationToken` and also exposes `request_stop()`. Both wake an idle runner immediately.

Shutdown is cooperative. If stop/cancellation arrives while a synchronous scheduled action is running, that action is allowed to finish. Before the runner considers the next task, it re-checks shutdown state and exits without starting additional scheduled work.

`reset_stop()` allows explicit reuse after a requested stop. Scheduler configuration and manual `run_due()` are rejected while the production runner is active.

Interval schedules support millisecond resolution; cron schedules remain minute-based by definition.

Scheduled tasks are stored with stable references, so the `Task&` returned by registration remains valid when additional tasks are registered.

## Overlap and distributed execution

Scheduler locking is explicit. Configure a `LockStore` on the scheduler with `.locks(...)`, then opt individual tasks into the desired policy.

`without_overlapping(ttl)` acquires a task-name lease before the action starts. If another process already owns that lease, the occurrence is skipped. The lease is released after successful or failed execution; the TTL is crash recovery protection.

`on_one_server(ttl)` acquires a schedule-occurrence-specific lease. The occurrence key includes the cron wall-clock slot (or the interval bucket), so only one application instance can execute that occurrence. The lease is intentionally not released after the action; it expires by TTL so a second server cannot replay the same slot immediately after the first finishes.

The two policies may be combined: `on_one_server()` elects one instance for the occurrence, while `without_overlapping()` prevents the elected occurrence from starting while a previous occurrence is still active.

`MemoryLockStore` is deterministic and useful for tests or single-process coordination.

When Gungnir is built with `GUNGNIR_WITH_REDIS=ON`, `RedisLockStore` provides distributed leases using Redis `SET NX PX`. Release and renewal use owner-token comparisons, preventing a stale process from releasing or extending a lock that was reacquired by another process.
