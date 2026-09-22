# Queues and Jobs

Gungnir queues separate job data, transport and execution.

## Job envelope

A queued job is represented by an `Envelope` containing an opaque identifier, stable job name, serialized payload and retry counters. Serialization is explicit; the queue does not persist arbitrary C++ object memory.

## Drivers

`queue::Driver` defines push, pop, acknowledge, release and failure operations. `MemoryDriver` is a process-local development and test implementation. It is not durable and must not be presented as a production queue.

## Worker

`Worker` maps stable job names to handlers and processes one job at a time with `run_one()`. Failed handlers are released until `max_attempts` is reached, after which the driver receives the failed job.

## Delivery semantics

The generic contract does not promise exactly-once delivery. Production adapters must document their acknowledgement, visibility timeout, redelivery and crash-recovery semantics.

## Async execution

Background execution is provided by queue workers, not by pretending synchronous application code is asynchronous. A production worker process, shutdown handling, signals, concurrency and backoff remain runtime concerns.

## Serialization and compatibility

Job names and payload schemas are externalized data contracts once persisted. Applications should version payloads when deployments may process jobs produced by older code.
