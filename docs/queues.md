# Queues and Jobs

Jobs carry immutable data to a worker. Injected services are excluded from the payload and resolved in the worker's application when it reconstructs the job.

```gnr
job GenerateReport {
    inject Storage storage;
    int report_id;
    handle() {
        storage.put('reports/latest.txt', 'Report ready');
    }
}
```

Payload fields must have a reconstruction codec: JSON values, scalar data, lists, maps and models are supported. Use lists for payload sequences, and store identifiers instead of request/response handles, events or query collections.

Construct a job with its data fields, then inject `Queue` to dispatch it. The producer's job object has no resolved services; call its handler through a worker.

```gnr
controller ReportController {
    inject Queue queue;
    store(int id) {
        const jobId = queue.dispatch(GenerateReport(id), 3);
        return json({jobId: jobId}, 202);
    }
    later(int id) {
        return text(queue.later(GenerateReport(id), 5000, 3));
    }
}
```

`dispatch(job, attempts = 1, afterCommit = true)` returns the job ID. `later(job, delayMilliseconds, attempts = 1, afterCommit = true)` delays availability; zero dispatches immediately. Attempts must be positive and fit the native unsigned range. Delays must be non-negative and fit the native clock.

After-commit dispatch publishes only when the active database transaction commits. A rollback discards the pending publication. A delayed job's delay begins when it is published. Pass `false` as the final argument for immediate publication inside a transaction. This applies to the active Gungnir database connection, not an external transaction or a transactional outbox.

Run a worker from the project directory:

```sh
gungnir queue:work
gungnir queue:work --once
gungnir queue:work --release
```

`--once` reserves and processes at most one available job, then exits, including when the queue is empty. Generated applications register all job handlers during boot. Continuous workers finish the current handler and stop on SIGINT/SIGTERM; handlers must cooperate with cancellation for prompt shutdown.

The generated bootstrap uses an in-memory queue for development. Its contents are process-local: a separate worker cannot consume jobs from an HTTP process's memory queue. Configure the same Redis queue in both processes in `bootstrap/app.hpp` for persistent shared workers. Set `ServiceOptions.worker_options` there for idle sleep, retry backoff, lease renewal, job count and runtime limits. Choose reservation and renewal settings suitable for the longest handler.

Failures retry up to the configured attempt count. Handlers must tolerate duplicate execution; delivery is not exactly-once. Inspect or retry failures through the configured driver:

```gnr
controller FailedJobsController {
    inject Queue queue;
    index() { return json(queue.failed()); }
    retry(string id) { return json(queue.retry(id)); }
    forget(string id) { return json(queue.forget(id)); }
}
```

Failure listings contain ID, name, attempts and maximum attempts, excluding payload data. `retry` resets attempts and returns the failed job to the queue; `forget` removes its failure record. Protect these administrative routes with authentication and authorization.

The [scheduler](scheduler.md) can publish jobs on a recurring schedule.
