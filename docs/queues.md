# Queues and Jobs

> **Status: Gungnir 1.0 current contract.** This page describes the current implementation. Native C++ APIs and `.gnr` syntax are identified separately. Proposed contracts are in the [design specification](design/queues.md).

## Current behavior

Queue contracts separate envelopes, drivers and workers. The envelope carries serialized payload and job identity/attempt information. Worker handlers are registered by name; worker options control retries and execution policy. Memory and optional Redis drivers are provided.

Workers expose stop requests and cancellation-aware execution. Lease renewal supports driver-owned reservations.

Structured `job` declarations generate immutable data constructors, typed JSON payloads, `from_payload` and `register_job(worker)`. Jobs with injected services use `register_job(worker, container)` and resolve services during decoding. Async handlers are awaited to completion by the synchronous worker; failures propagate to its retry handling. The container must outlive registered handlers.

## Limits and planned work

Jobs must tolerate retries; do not promise exactly-once side effects. Memory queues are process-local. Queue delivery does not share the request's open database transaction. `queue::Dispatcher` defaults to publishing after the active transaction commits; nested rollback discards that scope's pending jobs. Publication failures propagate after the database has committed, so this is not a durable transactional outbox. Pass `after_commit=false` for immediate dispatch. Static `.gnr` dispatch syntax remains separate.

Register `ServicesProvider` with explicit queue/cache/mail/storage adapters during bootstrap. It also exposes scheduler, event, resource-policy and notification services. Native `register_job`, `register_listener` and `register_policy` helpers wire generated declarations from boot hooks. `schedule_job` connects a scheduled action to the dispatcher; starting workers and running the scheduler remain explicit.

## Implementation references

- [include/gungnir/queue/job.hpp](../include/gungnir/queue/job.hpp)
- [include/gungnir/queue/driver.hpp](../include/gungnir/queue/driver.hpp)
- [include/gungnir/queue/worker.hpp](../include/gungnir/queue/worker.hpp)
- [include/gungnir/queue/redis_driver.hpp](../include/gungnir/queue/redis_driver.hpp)

See the [documentation index](README.md), [getting started](getting-started.md), and [target design](design/queues.md).
