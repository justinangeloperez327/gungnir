# Scheduler

Define the application's schedule in `routes/console.gnr` using `function void schedule(Scheduler schedule)`. Import job modules used by that function. Generated applications invoke it once during boot after listener/job registration and the native bootstrap hook.

```gnr
job RefreshSummary {
    inject Cache cache;
    handle() { cache.put('summary', {ready: true}); }
}
function void schedule(Scheduler schedule) {
    schedule.every('refresh-summary', 60000, RefreshSummary());
    schedule.cron('daily-summary', '0 0 * * *', RefreshSummary())
        .timezone('UTC').withoutOverlapping().onOneServer();
}
```

Each task needs a unique non-empty name. `every` takes a positive interval in milliseconds. `cron` takes a five-field cron expression. `hourly`, `daily`, `weekly` and `monthly` use the existing calendar scheduler. Scheduled jobs publish to the queue; a worker executes their handlers.

Callbacks must be synchronous, accept no parameters and return `void`. They can capture application service values; they cannot capture their own scheduler or task handle. Keep callbacks short, because the scheduler runs them sequentially.

```gnr
function void registerCallbacks(Scheduler schedule, Cache cache) {
    schedule.hourly('mark-hour', () => { cache.put('hourly', true); });
}
```

Calendar schedules use UTC by default. `.timezone('America/New_York')` loads the named IANA zone; provision zoneinfo data and set `TZDIR` when the system has no zoneinfo directory, including Windows. UTC needs no external files. Calendar scheduling follows local wall-clock slots, including the native scheduler's daylight-saving behavior.

`.withoutOverlapping(ttlMilliseconds = 86400000)` locks the scheduled action while it executes. For a queued job, that covers publication, not the job's handler. `.onOneServer(ttlMilliseconds = 86400000)` locks an occurrence across schedulers using the same lock store. TTLs must be positive; choose a TTL long enough for the protected action. Configure tasks before starting the runner.

The development bootstrap configures `MemoryLockStore` for process-local locks. Set `ServiceOptions.scheduler_locks` to a shared store such as `RedisLockStore` in `bootstrap/app.hpp` for multiple processes or servers. An unconfigured lock store is an error when enabling either lock option.

```sh
gungnir schedule:run
gungnir schedule:work --release
```

`schedule:run` boots the application, runs currently due actions once and shuts down. Interval tasks run on the first check. Because one-shot processes start with fresh interval state, use `schedule:work` to preserve intervals between checks, or invoke `schedule:run` once per minute for calendar schedules. Continuous scheduling stops on SIGINT/SIGTERM after the current action finishes. No HTTP port is opened by these commands.

HTTP startup registers schedules but starts no background thread automatically. Start the scheduler and workers explicitly, or attach the existing native scheduler and queue worker to `RuntimeHost` in an embedded application. Use a shared queue when the scheduler and workers run in separate processes.
