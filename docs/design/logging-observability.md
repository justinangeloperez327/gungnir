# Logging and Observability

> **Future design specification.** This document describes intended long-term architecture beyond the published `1.0` contract. Examples and requirements may exceed the current implementation. See the [current implementation guide](../logging-observability.md) before using an API.


## Release alignment

This document describes possible future architecture beyond 1.0. Current implementation guides and executable tests define the published contract.

Gungnir provides framework-neutral logging, tracing, and metrics boundaries.

Observability should describe what the application/runtime is doing without coupling source code to one vendor.

# Structured logging

A log record should include:

~~~text
level
message
timestamp
structured context
~~~

Context may include request ID, route name, job ID, scheduler task, model/database operation, or trace identifiers.

Do not encode all metadata into one message string.

# Logger and sinks

A Logger sends records to configured sinks.

Memory sinks are useful for tests.

Production sinks must define buffering, durability, rotation/retention, failure behavior, and shutdown flushing appropriate to the deployment.

# Request correlation

The HTTP boundary may attach an accepted or generated request ID according to security policy.

That ID should flow through middleware, controller execution, database operations, and logs.

# Tracing

Tracing models:

~~~text
trace
span
parent/child relation
attributes
status
timing
~~~

A tracer with no configured sink/exporter should be a low-overhead no-op.

# Async context propagation

Tracing context must follow the logical execution path across:

- executor scheduling;
- coroutine suspension/resumption;
- timers;
- request processing.

Do not depend only on raw thread identity.

# Queue propagation

Queue handoff crosses an execution boundary.

If trace propagation is desired, the relevant trace context must be serialized into queue metadata explicitly.

# Scheduler propagation

Scheduled tasks begin a runtime execution context and should receive task identity plus tracing/metrics context.

# Framework instrumentation

Useful instrumentation points include:

- HTTP request lifecycle;
- routing/controller execution;
- database queries/transactions;
- cache operations;
- queue reservation/execution;
- scheduler runs;
- mail delivery;
- storage I/O.

Instrumentation should use stable semantic operation names rather than generated C++ implementation names.

# Metrics

Metrics may include:

~~~text
counter
gauge
histogram
~~~

Metric names and labels should remain bounded.

Do not use unbounded values such as raw user IDs or full URLs as labels.

# Exporters

Exporters such as OTLP/OpenTelemetry integrations should use bounded queues and explicit retry/drop policy.

Observability must not become an unbounded memory or shutdown liability.

# Failure semantics

Logging/telemetry failure should normally not crash a healthy application unless configured as critical.

Failures should still be observable through fallback diagnostics where possible.

# Sensitive data

Do not log:

- passwords;
- auth tokens;
- session IDs unnecessarily;
- private keys;
- raw payment/secret material;
- full database credentials.

Request/body logging must be explicit and sanitized.

# Shutdown

Observability exporters participate in the application lifecycle.

Shutdown should flush bounded pending telemetry according to deadline and then release resources.

# Design rule

~~~text
structured data over message parsing
context follows logical async execution
bounded telemetry
vendor-neutral core
secrets stay out of logs
~~~

