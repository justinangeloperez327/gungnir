#include <cassert>
#include <chrono>
#include <string>
#include <vector>

#include <gungnir/observability/otlp_http_exporter.hpp>
#include <gungnir/observability/propagation.hpp>
#include <gungnir/logging/json_stream_sink.hpp>
#include <sstream>
#include "guide.hpp"

int main() {
    using namespace gungnir::observability;

    OtlpResource resource;
    resource.service_name =
        "package-consumer";

    SpanRecord span;
    span.trace_id =
        "0123456789abcdef0123456789abcdef";
    span.span_id =
        "0123456789abcdef";
    span.name =
        "package.test";
    span.started_at =
        std::chrono::system_clock::now();
    span.ended_at =
        span.started_at;

    const auto payload =
        otlp::encode_traces_json(
            std::vector<SpanRecord>{
                span
            },
            resource
        );

    assert(
        payload.find(
            "\"package-consumer\""
        ) != std::string::npos
    );

    OtlpHttpSettings settings;
    settings.resource =
        resource;

    OtlpHttpPolicy policy;
    policy.shutdown_timeout = std::chrono::milliseconds{100};
    OtlpHttpExporter exporter{settings, policy};
    assert(parse_traceparent(format_traceparent({span.trace_id, span.span_id})));
    std::ostringstream output;
    gungnir::logging::JsonStreamSink logger{output};
    logger.write({.message="installed logger"});
    assert(output.str().find("installed logger") != std::string::npos);

    assert(
        exporter.settings()
            .resource
            .service_name ==
        "package-consumer"
    );

    exporter.shutdown();

    auto app = gungnir::Application::create();
    gungnir::ServiceOptions services;
    configure_observability(app, services);
    assert(services.logger && services.tracer && services.meter);
    services.tracer->shutdown(); services.meter->shutdown();

    return 0;
}
