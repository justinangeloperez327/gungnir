#include <cassert>
#include <chrono>
#include <string>
#include <vector>

#include <gungnir/observability/otlp_http_exporter.hpp>

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

    OtlpHttpExporter exporter{
        settings
    };

    assert(
        exporter.settings()
            .resource
            .service_name ==
        "package-consumer"
    );

    exporter.shutdown();

    return 0;
}
