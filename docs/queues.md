# Queues and Jobs

Queues move work outside the immediate request path. Jobs are typed units of work that can be serialized, dispatched, retried, and executed by workers.

## Defining a job

```gnr
job GenerateReport {
    int report_id;

    handle() {
        // Generate the report.
    }
}
```

Job fields form the serialized payload.

## Dependency injection

Jobs can inject services required by their handler. Services are resolved by the worker when the payload is reconstructed.

## Dispatching

Jobs are dispatched through the queue dispatcher. Applications choose the configured queue connection/driver.

## After-commit dispatch

Database-dependent jobs can be published after the active transaction commits so workers do not observe data that later rolls back.

## Workers

Workers reserve jobs from a queue, reconstruct the typed job, execute `handle`, and apply retry/failure policy.

## Retries

Jobs must be designed for retry-safe execution. Queue delivery should not be treated as exactly-once execution.

## Drivers

Gungnir provides an in-memory queue for development/testing and Redis-backed queues for persistent shared workers.

## Cancellation and shutdown

Workers support controlled stop requests and cancellation-aware execution so applications can shut down without abandoning process state unnecessarily.

## Scheduling jobs

The scheduler can dispatch queued jobs on a recurring schedule.

See [Scheduler](scheduler.md).
