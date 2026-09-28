# Logging and Observability

Gungnir provides a structured logging boundary rather than coupling application code to a specific logging vendor.

## Records

Each record has a level, message, structured string context and timestamp. Context can carry request identifiers, route names, job identifiers or other correlation fields without embedding them into message text.

## Logger and sinks

A Logger fans records out to configured Sink implementations. MemorySink is thread-safe and intended for tests and development support.

Production applications should provide sinks with explicit durability, rotation, buffering and failure policies appropriate to their environment.

## Request correlation

HTTP middleware can attach an accepted or generated request identifier to logging context. The logging API itself does not trust proxy headers or generate identifiers, keeping those security decisions at the HTTP boundary.

## Tracing

Gungnir provides a dependency-free tracing core in `gungnir::observability`.

`Tracer::start_span()` creates spans with trace IDs, span IDs, parent relationships, structured attributes, status, error text, and system-clock start/end timestamps. A tracer without a `SpanSink` is a true no-op and returns an invalid span without allocating trace state or generating IDs.

`Span::scope()` activates the span context for nested work on the current execution path. `current_context()` and `activate()` allow explicit capture/reactivation where code owns an asynchronous boundary.

The core executor captures the active trace context when work is posted and reactivates it on the worker thread. This preserves trace parentage across `Executor::post()`, `Executor::schedule()`, and coroutine hops implemented through `co_await executor.yield()`.

`SpanSink` is the exporter boundary. `MemorySpanSink` is provided for tests. Vendor/export-protocol integrations remain optional layers above this contract.

## Telemetry lifecycle

`Tracer::flush()` and `Tracer::shutdown()` forward lifecycle requests to the configured sink. Span exporter exceptions are isolated from application execution when spans end; exporter implementations should surface operational health through their own diagnostics rather than throw through request/job code.

## Failure semantics

Sink exceptions currently propagate to the caller. Applications that require best-effort logging should implement that behavior in a sink or wrapper rather than having the framework silently discard logging failures.


## Framework tracing

When a global tracer with a sink is configured, Gungnir instruments the major runtime boundaries automatically:

- HTTP dispatch creates `http.server.request` spans with method, normalized request path and response status.
- Database execution creates `database.query` spans with database system, connection name and operation verb. Bind values and full statements are intentionally not recorded by default.
- Queue workers create `queue.job` spans with job ID, logical job name and attempt count.
- Scheduler actions create `scheduler.task` spans with task name, schedule type and timezone.
- Mail delivery creates `mail.send` spans with operation type and recipient count; addresses, subject and body content are not recorded.

Queue envelopes carry trace ID and parent span ID metadata separately from the application payload. Memory and Redis drivers capture that metadata at enqueue time, and workers continue the originating trace even when processing occurs later or in another process. Redis decoding remains compatible with queue envelopes persisted before trace metadata existed.

HTTP route coroutine context is suspended and restored across Gungnir executor yields and timer sleeps. Framework-owned async boundaries must preserve trace context; custom application awaiters that move execution to another thread should capture `current_context()` and reactivate it when resuming.


## Metrics

Gungnir provides dependency-free `Counter`, `Gauge`, and `Histogram` instruments through `observability::Meter`. A meter without a `MetricSink` is a no-op. Metric sink failures are isolated from application execution.

Each emitted `MetricPoint` contains the instrument kind, name, numeric value, structured attributes, timestamp, and—when a trace span is active—the current trace/span IDs for exemplar-style correlation.

`MemoryMetricSink` stores points for deterministic tests.

Framework metrics currently include:

- `http.server.request.count` and `http.server.request.duration`, with method, normalized path, status and outcome;
- `http.server.connection.active` gauge;
- `db.client.operation.count` and `db.client.operation.duration`, with database system, connection, operation and outcome;
- `messaging.process.count`, `messaging.process.duration`, `messaging.process.failures`, and `messaging.worker.active`;
- `scheduler.task.executions`, `scheduler.task.duration`, and `scheduler.task.failures`;
- `cache.request.count` with hit/miss result;
- `mail.send.count`, `mail.send.duration`, and `mail.recipient.count`.

High-cardinality or sensitive values such as SQL bindings, request query strings, mail addresses, subjects, bodies and queue payloads are not emitted by the built-in metrics.
