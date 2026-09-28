# Queues and Jobs

Gungnir queues separate job data, transport and execution.

## Job envelope

A queued job is represented by an `Envelope` containing an opaque identifier, stable job name, serialized payload and retry counters. Serialization is explicit; the queue does not persist arbitrary C++ object memory.

## Drivers

`queue::Driver` defines push, pop, acknowledge, release and failure operations. `MemoryDriver` is a process-local development and test implementation. It is not durable and must not be presented as a production queue.

## Worker

`Worker` maps stable job names to handlers. `run_one()` remains available for tests and explicit single-job execution, while `run()` provides the long-running production loop.

`WorkerOptions` controls idle polling, an optional maximum number of jobs, and an optional maximum runtime. `request_stop()` wakes an idle worker immediately and prevents any new reservation after the current iteration. If shutdown is requested while a handler is active, that handler is allowed to finish and its job is acknowledged or released before the worker exits.

`run()` also accepts a Gungnir `CancellationToken`. Process supervisors and application signal handlers should cancel that token rather than installing queue-specific global signal state inside the framework.

## Delivery semantics

The generic contract does not promise exactly-once delivery. Production adapters must document their acknowledgement, visibility timeout, redelivery and crash-recovery semantics.

## Worker lifecycle

Background execution is provided by queue workers, not by pretending synchronous application code is asynchronous. The worker loop now has cooperative production shutdown semantics: stop polling first, finish the active job, then exit. Operating-system signal ownership remains with the hosting executable or service manager so applications can coordinate HTTP, queue, scheduler and other shutdown participants through one cancellation source.

Multi-worker supervision, process restarts and deployment-level concurrency remain host concerns.

## Serialization and compatibility

Job names and payload schemas are externalized data contracts once persisted. Applications should version payloads when deployments may process jobs produced by older code.


## Redis production driver

When Gungnir is built with `GUNGNIR_WITH_REDIS=ON`, `gungnir::redis` also provides `queue::RedisDriver`.

```cpp
gungnir::queue::RedisSettings settings;
settings.host = "127.0.0.1";
settings.queue = "emails";
settings.visibility_timeout =
    std::chrono::seconds{60};

gungnir::queue::RedisDriver driver{
    settings
};

gungnir::queue::Worker worker{
    driver
};
```

The Redis driver does not treat a queue as a disposable list. Each active job is stored separately from its ready-state entry. `pop()` atomically assigns a visibility lease and a driver-owned reservation token. `acknowledge()`, `release()`, `release_after()`, `renew()`, and `fail()` only mutate the job when that reservation token still owns the lease.

If a worker exits without acknowledging a job, a later `pop()` recovers expired leases back to the ready queue. A worker that finishes after its old lease expired cannot acknowledge a job that has already been leased again to another worker.

The queue uses Redis server time when calculating visibility and delayed-delivery deadlines, avoiding correctness dependence on worker-machine clock synchronization.

### Delayed jobs

`push_later(job, delay)` stores the job immediately but keeps it out of the ready list until its Redis-server-time deadline. `release_after(job, delay)` applies the same mechanism when a worker wants to retry later. Due delayed jobs are promoted atomically during reservation.

### Lease renewal

Long-running handlers can enable `WorkerOptions::lease_renewal_interval`. While the handler is running, the worker periodically calls `Driver::renew()`. Redis renews the visibility deadline only when the reservation token still owns the lease, so a stale worker cannot extend a lease after another worker has recovered the job.

The renewal interval should be comfortably shorter than `RedisSettings::visibility_timeout`.

Current Redis queue guarantees:

- durable ready-job state as far as the configured Redis persistence policy provides;
- atomic reservation;
- configurable visibility timeout;
- delayed enqueue and delayed release;
- expired-lease recovery;
- stale-reservation protection;
- lease renewal for long-running handlers;
- retry state persisted through `release()` / `release_after()`;
- failed-job retention;
- binary-safe payload storage; and
- duplicate active/failed job-ID rejection.

Retry backoff policy, failed-job administration, Redis Cluster/Sentinel/TLS support, and multi-process worker supervision remain separate runtime work.
