# External observability

Generated applications can send traces and metrics to an OpenTelemetry Collector
with the optional `gungnir::otlp` adapter. `Logger` can write newline-delimited
JSON to a process stream for a platform log agent. The native instrumentation and
injected `.gnr` APIs use the owning application's tracing and metric services.

## Build and bootstrap

Build/install an SDK with `GUNGNIR_WITH_OTLP=ON`. This adapter requires libcurl
7.68 or newer; the core and application portable SDK profiles do not enable it
by default. Generated project CMake links the installed optional adapter and
sets `GNR_ADAPTER_OTLP`. Native consumers link `gungnir::otlp` explicitly.

The following helper belongs in `bootstrap/app.hpp`. Call it with the same
`ServiceOptions` used to configure your queue, storage, and other adapters, then
register that one `ServicesProvider`.

```cpp
#include <gungnir/core/services.hpp>
#include <gungnir/logging/json_stream_sink.hpp>
#include <gungnir/observability/otlp_http_exporter.hpp>
#include <iostream>

inline void configure_observability(gungnir::Application& app,
                                    gungnir::ServiceOptions& services) {
    using namespace gungnir;
    services.logger = std::make_shared<logging::Logger>();
    services.logger->sink(std::make_shared<logging::JsonStreamSink>(std::cout));

    observability::OtlpHttpSettings settings;
    settings.endpoint = app.env().get("OBSERVABILITY_OTLP_ENDPOINT", "http://127.0.0.1:4318");
    settings.resource.service_name = app.env().get("APP_NAME", "my-application");
    settings.resource.service_version = app.env().get("APP_VERSION", "1.0.0");
    settings.resource.deployment_environment = app.environment();
    const auto instance = app.env().get("INSTANCE_ID");
    if (!instance.empty()) settings.resource.attributes["service.instance.id"] = instance;
    const auto authorization = app.env().get("OBSERVABILITY_AUTHORIZATION");
    if (!authorization.empty()) settings.headers["Authorization"] = authorization;
    settings.request_timeout = std::chrono::milliseconds{500};
    settings.batch_delay = std::chrono::milliseconds{500};

    observability::OtlpHttpPolicy policy;
    policy.export_timeout = std::chrono::milliseconds{1500};
    policy.flush_timeout = std::chrono::milliseconds{2500};
    policy.shutdown_timeout = std::chrono::milliseconds{1500};
    policy.ca_file = app.env().get("OBSERVABILITY_CA_FILE");
    auto exporter = std::make_shared<observability::OtlpHttpExporter>(settings, policy);
    services.tracer = std::make_shared<observability::Tracer>(exporter);
    services.meter = std::make_shared<observability::Meter>(exporter);
}
```

For example, `bootstrap::configure` can construct `ServiceOptions`, call this
helper, select its other adapters, and call
`app.provider<gungnir::ServicesProvider>(services)`. Environment values override
`.env`. These observability variable names are the application's explicit
bootstrap contract; Gungnir does not automatically interpret the standard
`OTEL_*` variables. Set an HTTPS endpoint and authorization value supplied by
that collector when using a remote service. Peer and hostname verification are
on by default. An empty CA file uses libcurl's system trust store; a private
collector can provide its CA certificate file without disabling verification.

Resources carry `service.name`, optional `service.version`,
`deployment.environment.name`, and custom attributes. Instrumentation scope is
`gungnir` with the actual framework version, currently `1.0.0`. Use a distinct
`service.instance.id` per application process when a backend requires separate
metric writers. Do not store credentials in resource or measurement attributes.

## HTTP and worker correlation

The HTTP listener accepts a valid version `00` W3C `traceparent` identity as the
request span's parent. IDs must have exactly 32/16 lowercase hexadecimal digits
and cannot be all zero. Absent, malformed, duplicate/combined, and unsupported
version headers start a fresh trace rather than rejecting the request.

Native callers can use `observability::format_traceparent(current_context())`
on outbound HTTP requests and `parse_traceparent` at their own transport
boundaries. The core tracer records every span when a sink is configured. The
remote sampled flag is validated but does not change this local policy;
parent-based/ratio sampling, `tracestate`, and baggage are not implemented.
This is an identity propagation API, not a full OpenTelemetry SDK.

Framework request, database, and queue spans inherit the active logical context
across coroutine/timer suspension. Queue drivers persist trace/parent IDs in
transport metadata, so a fresh worker process continues the request trace after
the producer exits. A job's own telemetry receives the `queue.job` parent;
retries create new processing spans with the original producer parent. A custom
`.gnr` Span handle receives the active parent but does not activate itself.
Native `span.scope()` can establish a custom child context explicitly.

Boot/ready hooks use services installed during provider registration. Generated
workers and migrations activate the refreshed application context after boot.
Native hosts should call `app.boot()` before establishing an `app.activate()`
scope for synchronous worker, scheduler, or database operations.

Logs emitted through injected `Logger` include `trace_id` and `span_id` when a
span context is active. Application fields can add a request ID or other bounded
operation information. Metric exemplars include active trace/span IDs. Keep
metric labels bounded: use operation/outcome categories rather than user IDs,
job IDs, raw paths, or arbitrary input.

The current exporter sends measurement events: counter increments and
single-observation histograms use delta temporality; gauges are observations.
Histograms contain count, sum, min/max, and one unbounded bucket. Exported numbers
retain double precision. Start times/interval aggregation, configurable histogram
buckets and quantiles, and cumulative conversion are not provided. Configure
backend processing to match these semantics before using rates or percentiles.

## Delivery and loss contract

`OtlpHttpSettings` retains its existing constructor/layout. The additional
`OtlpHttpPolicy` constructor controls delivery budgets:

| Setting | Default | Behavior |
| --- | --- | --- |
| `max_batch_size` | 512 | Maximum records taken from each signal queue per pass |
| `max_queue_size` | 2048 | Combined pending span/measurement count |
| `max_record_bytes` | 64 KiB | Maximum single-record JSON envelope; oversized/invalid records are dropped |
| `max_queue_bytes` | 4 MiB | Combined pending envelope bytes, conservatively counting resource overhead per record |
| `max_response_bytes` | 64 KiB | Maximum collector response body; oversize/deep/malformed responses are terminal |
| `max_attempts` | 3 | Maximum sends of a signal batch, including the first attempt |
| `retry_delay` / `max_retry_delay` | 100 ms / 1 s | Bounded exponential delay when no `Retry-After` is supplied |
| `export_timeout` | 5 s | Total signal batch budget, including attempts and backoff |
| `flush_timeout` | 5 s | Maximum wait for records accepted before the flush call |
| `shutdown_timeout` | 5 s | Maximum drain budget after stopping; remaining records are counted as dropped |

The worker holds at most one batch per signal in addition to pending queues;
serialized requests/responses and container overhead also consume memory.
Batch thresholds apply to combined queue activity. Ordinary requests enqueue
telemetry without waiting for collector I/O. Full queues drop incoming records.

HTTP 429/502/503/504 and transient network failures can retry the same payload.
Other HTTP failures, TLS verification errors, oversized/malformed replies, and
partial success responses do not retry. `Retry-After` seconds and HTTP dates are
honored within the export budget; a delay beyond the budget abandons the batch
rather than retrying early. Retries after ambiguous network failures can create
duplicates. There is no persistent spool or delivery guarantee.

Traces and metrics are sent independently: failure of one does not suppress the
other. A JSON partial success reply counts the collector's rejected records;
zero-rejection warnings remain visible without counting a loss. Responses are
consumed internally rather than printed into application output.

`dropped()` is cumulative for queue/record rejection, terminal transport failure,
collector partial rejection, shutdown expiry, and writes after shutdown.
`last_error()` describes the last completed batch or local rejection and clears
on a later fully successful batch. Monitor the loss count and surface diagnostics
through a separate log stream; do not rely on an unavailable collector to report
its own outage. Collector error bodies and request credentials are not copied
into these diagnostics.

`flush()` waits for a snapshot of accepted records, so ongoing producers cannot
extend that snapshot. A flush timeout throws; a transport rejection reports loss
through diagnostics. `shutdown()` stops acceptance, drains within its budget,
counts remaining losses, and joins the worker. Concurrent/repeated shutdown is
safe. In-flight I/O is polled for shutdown expiry at up to 50 ms intervals.
Application shutdown attempts both cleanup stages even after a flush exception
and exposes lifecycle exceptions in `shutdown_errors()`. The process stop budget
must cover HTTP/work draining plus observability flush and shutdown budgets.

## Structured logs and redaction

`logging::JsonStreamSink` serializes concurrent writes, escapes message/context
values, and flushes each newline-delimited JSON record. Records contain
`timestamp_unix_nano`, `level`, `message`, and `context`. The default encoded record
limit is 64 KiB. `dropped()` counts oversized records and failed stream writes;
Logger isolates sink failures. Stream exceptions/writes can block, so choose an
appropriate process stream or adapter. The stream must outlive the sink.
Rotation, retention, buffering, and durable shipping are the platform log
agent's responsibility. This is not OTLP log export.

Injected Logger/Telemetry and `.gnr` Span attributes redact sensitive field
names using the existing case-insensitive password/secret/token/authorization/
cookie/api-key rules. Native attributes, resource attributes, messages, names,
and error text retain their explicit caller contract. Never put secrets in
those values or print exporter settings/authorization headers.

## Acceptance evidence

The PR CI observability job downloads checksum-pinned official
`otelcol-contrib` **0.160.0**, builds a custom SDK, and runs:

- `gungnir.observability`: propagation validation, structured logging, redaction,
  concurrent writers, and logical framework context;
- `gungnir.otlp` and `gungnir.otlp_transport`: JSON encoding/precision, authenticated
  HTTP, retries/rejections/partial success, queue byte limits, response bounds,
  TLS CA/hostname verification, concurrent shutdown, and snapshot/deadline flush;
- `gungnir.application_otlp`: installed generated HTTP/SQLite/Redis application,
  concurrent independent request traces, real timer suspension, producer exit,
  separate workers, worker/HTTP errors, collector-decoded resources/exemplars,
  field redaction, graceful exporter drain, and collector outage isolation;
- `installed.otlp`: external native consumer linked against the installed SDK,
  including compilation of this guide's bootstrap helper.

The real Collector test uses its OTLP HTTP receiver and JSON file exporter to
inspect decoded data. The separate fault server tests failure behavior and is
not used as interoperability evidence. Vendor backends, gRPC/protobuf export,
remote TLS/proxy deployment, external log-agent ingestion, and production load
are outside this acceptance coverage.

To reproduce, provision Redis and the official Collector binary, configure
`GUNGNIR_WITH_OTLP=ON`, `GUNGNIR_OTLP_INTEGRATION_TESTS=ON`,
`GUNGNIR_WITH_REDIS=ON`, and `GUNGNIR_WITH_SQLITE=ON`, build the CI targets,
then set `GUNGNIR_OTELCOL` and `GUNGNIR_REDIS_HOST`/`GUNGNIR_REDIS_PORT` before
running `ctest --output-on-failure -R 'observability|otlp'`. The installed app test
manages its own loopback collector and temporary application.

See [Logging and Observability](logging-observability.md),
[Production](production.md), the [OTLP protocol](https://opentelemetry.io/docs/specs/otlp/),
and the [Collector release](https://github.com/open-telemetry/opentelemetry-collector-releases/releases/tag/v0.160.0).
