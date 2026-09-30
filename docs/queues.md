# Queues and Jobs

> **Status: experimental, pre-1.0.** This page describes the current implementation. Native C++ APIs and `.gnr` syntax are identified separately. Proposed contracts are in the [design specification](design/queues.md).

## Current behavior

Queue contracts separate envelopes, drivers and workers. The envelope carries serialized payload and job identity/attempt information. Worker handlers are registered by name; worker options control retries and execution policy. Memory and optional Redis drivers are provided.

Workers expose stop requests and cancellation-aware execution. Lease renewal supports driver-owned reservations.

## Limits and planned work

Jobs must tolerate retries; do not promise exactly-once side effects. Memory queues are process-local. Queue delivery does not share the request's open database transaction. Static `.gnr` dispatch syntax and automatic after-commit scheduling need explicit integration.

## Implementation references

- [include/gungnir/queue/job.hpp](../include/gungnir/queue/job.hpp)
- [include/gungnir/queue/driver.hpp](../include/gungnir/queue/driver.hpp)
- [include/gungnir/queue/worker.hpp](../include/gungnir/queue/worker.hpp)
- [include/gungnir/queue/redis_driver.hpp](../include/gungnir/queue/redis_driver.hpp)

See the [documentation index](README.md), [getting started](getting-started.md), and [target design](design/queues.md).
