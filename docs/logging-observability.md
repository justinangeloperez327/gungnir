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
