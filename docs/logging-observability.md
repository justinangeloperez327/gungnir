# Logging and Observability

Inject Logger and Telemetry to record application work:

```gnr
controller InstrumentedController {
    inject Logger logger;
    inject Telemetry telemetry;
    show(Request request) {
        const span = telemetry.span("page.render", {"route": "/page"});
        logger.info("Page requested", {"request_id": request.header("X-Request-ID")});
        telemetry.counter("page.requests", 1, {"result": "success"});
        telemetry.gauge("page.active", 1);
        telemetry.histogram("page.duration", 0.5);
        span.attribute("page", "welcome");
        span.end();
        return text("Welcome");
    }
}
```

Logger supports `debug`, `info`, `warning`, and `error`, with a message and an
optional object of fields. Telemetry supports `span`, `counter`, `gauge`, and
`histogram`. Attributes must be an object of scalars. Names must be nonempty,
values finite, and counters and histograms nonnegative. Counters default to one.
Spans support `attribute`, `error`, `end`, `traceId`, and `spanId`.

## Application configuration

Select native sinks in `bootstrap/app.hpp`. Memory sinks are useful for tests:

```cpp
#include <gungnir/logging/memory_sink.hpp>
#include <gungnir/observability/memory_span_sink.hpp>
#include <gungnir/observability/memory_metric_sink.hpp>

gungnir::ServiceOptions options;
auto logs = std::make_shared<gungnir::logging::MemorySink>();
options.logger = std::make_shared<gungnir::logging::Logger>();
options.logger->sink(logs);
options.tracer = std::make_shared<gungnir::observability::Tracer>(
    std::make_shared<gungnir::observability::MemorySpanSink>());
options.meter = std::make_shared<gungnir::observability::Meter>(
    std::make_shared<gungnir::observability::MemoryMetricSink>());
app.provider<gungnir::ServicesProvider>(options);
```

Explicit tracers and meters belong to the application's execution context.
Framework HTTP, database, and background instrumentation uses that context,
including resumed tasks. Native global defaults remain available when no
application override is configured. Logger has no output sink until one is selected.

Shutdown flushes and shuts down explicitly configured tracing and metric sinks.
Cleanup attempts shutdown even after a flush failure;
`Application::shutdown_errors()` exposes collected failures. Generated HTTP and
background executables connect signals to cancellation. Telemetry sink failures
are isolated from ordinary request results.

## Correlation and sensitive data

Logger attaches current trace and span identifiers when a trace is active.
Metrics use the same context. A custom span receives the current parent;
creating a Span handle alone does not activate it. Use native
`observability::activate` scopes for custom child context.

Application facades redact field names containing password, secret, token,
authorization, cookie, api_key, or apikey, ignoring case. Nested objects and
arrays are rejected. Messages, span names, error messages, and other fields are
application-controlled: exclude secrets from them. Native APIs retain their
explicit attribute contract.

## Exporters

The OTLP HTTP exporter is built with `GUNGNIR_WITH_OTLP=ON` and exported as
`gungnir::otlp`. Configure its endpoint, credentials, bounded buffering, and
sinks explicitly in bootstrap. Test required remote propagation and exporter
behavior for the deployment.

See [Production](production.md), [Dependency Injection](dependency-injection.md),
and [the native OTLP exporter](../include/gungnir/observability/otlp_http_exporter.hpp).
