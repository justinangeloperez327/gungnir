# Queues and Jobs

> **Future design specification.** This document describes intended long-term architecture beyond the published `1.0` contract. Examples and requirements may exceed the current implementation. See the [current implementation guide](../queues.md) before using an API.


## Release alignment

This document describes possible future architecture beyond 1.0. Current implementation guides and executable tests define the published contract.

Gungnir queues separate background job data, transport, retry policy, and worker execution.

Async application code is not automatically queued. Queued work has a separate execution lifecycle.

# Core model

A queued job is represented by a stable envelope:

~~~text
job ID
job name/type
serialized payload
attempt count
available-at time
metadata
~~~

The queue must not persist arbitrary C++ object memory.

# Serialization

Job payloads must be explicitly serializable.

Good payloads contain stable application data such as:

~~~text
record IDs
strings
numbers
small structured values
explicit versioned payload objects
~~~

Do not enqueue request-scoped objects such as Request, Response, Next, open transactions, native references, or scoped service instances.

# Drivers

The queue driver owns transport/storage behavior.

Possible drivers include:

- MemoryDriver for tests and local development;
- Redis-backed durable/shared queue when enabled;
- future broker/database adapters.

MemoryDriver is not a durable production queue.

# Delivery semantics

The generic queue contract must not claim exactly-once execution.

Production drivers should document:

~~~text
reservation/visibility behavior
acknowledgement
redelivery
retry
lease timeout
crash recovery
failed-job storage
~~~

Applications should design jobs to tolerate retry where required.

# Worker

A worker:

1. reserves a job;
2. resolves its registered handler;
3. executes the handler;
4. acknowledges success;
5. releases/retries or fails according to policy.

Handler registration uses stable semantic job identity.

# Worker lifecycle

Workers participate in the application lifecycle.

Shutdown should:

1. stop reserving new jobs;
2. allow the current owned job to complete within policy;
3. observe cancellation;
4. release or fail work correctly if execution cannot complete.

# Retry policy

Retry behavior should be explicit.

Useful policy may include:

~~~text
maximum attempts
fixed/exponential backoff
retryable error categories
dead-letter/failed-job storage
~~~

Do not retry every failure blindly.

# Delayed jobs

Drivers may support delayed availability.

Delay semantics must be represented explicitly rather than implemented through worker sleep that occupies a worker slot.

# Lease renewal

Drivers using visibility leases may support lease renewal for long-running jobs.

A lost lease must not be silently treated as successful ownership.

# Failed jobs

Production queue systems should support inspection of failed jobs and explicit retry/delete operations where the driver supports it.

Failure payloads/logs must not leak secrets unnecessarily.

# Dependency injection

Job handlers may resolve application services from a worker-owned application scope.

A new scope should be created per job when scoped services are supported.

# Async handlers

A job handler may be async when the runtime supports it.

Async does not change queue delivery semantics.

The worker still owns acknowledgement, retry, cancellation, and lease behavior.

# Transactions

Database work inside jobs follows normal transaction rules.

Do not enqueue a job while assuming it automatically shares the caller's current open transaction.

If dispatch must happen only after commit, use an explicit after-commit queue contract.

# Observability

Queue execution should record useful context such as:

~~~text
job name
job ID
attempt
duration
result
failure category
queue/driver
~~~

Tracing context may be serialized/propagated explicitly when supported.

# Security

Queue payloads may persist beyond the originating request.

Avoid embedding credentials or unnecessary sensitive data.

Production queue backends require appropriate access control and transport security.

# Design rule

~~~text
async = current task may suspend
queue = work moves to another execution lifecycle
driver owns delivery semantics
worker owns execution lifecycle
payload is explicit and serializable
~~~

