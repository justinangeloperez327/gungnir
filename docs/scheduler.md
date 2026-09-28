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

## Clock

Time is provided through the `Clock` interface. Production applications can use `SystemClock`; tests can provide deterministic clocks.

Cron evaluation in this group is UTC. Named timezones and daylight-saving-time policy are handled separately by the timezone scheduling layer.

## Process lifecycle

The scheduler does not yet create its own production loop in this stage. A command, worker, service manager or application runtime can call `run_due()` and use `next_due()` to avoid aggressive polling.

## Distributed deployments

Cron syntax does not imply distributed exclusivity. Multiple application instances may run the same due task until an explicit shared locking policy is configured.
