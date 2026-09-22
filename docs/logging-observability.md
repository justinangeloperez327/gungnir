# Logging and Observability

Gungnir provides a structured logging boundary rather than coupling application code to a specific logging vendor.

## Records

Each record has a level, message, structured string context and timestamp. Context can carry request identifiers, route names, job identifiers or other correlation fields without embedding them into message text.

## Logger and sinks

A Logger fans records out to configured Sink implementations. MemorySink is thread-safe and intended for tests and development support.

Production applications should provide sinks with explicit durability, rotation, buffering and failure policies appropriate to their environment.

## Request correlation

HTTP middleware can attach an accepted or generated request identifier to logging context. The logging API itself does not trust proxy headers or generate identifiers, keeping those security decisions at the HTTP boundary.

## Telemetry

This group does not claim OpenTelemetry, tracing exporters or metrics backends. Those integrations require concrete lifecycle, propagation and exporter contracts rather than placeholder APIs.

## Failure semantics

Sink exceptions currently propagate to the caller. Applications that require best-effort logging should implement that behavior in a sink or wrapper rather than having the framework silently discard logging failures.
