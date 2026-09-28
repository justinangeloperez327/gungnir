#include <cassert>
#include <chrono>
#include <cstdlib>
#include <memory>
#include <string>
#include <vector>

#include <gungnir/observability/otlp_http_exporter.hpp>

int main() {
    using namespace gungnir::observability;
    using namespace std::chrono_literals;

    const auto started =
        std::chrono::system_clock::now();

    SpanRecord root;
    root.trace_id =
        "0123456789abcdef0123456789abcdef";
    root.span_id =
        "0123456789abcdef";
    root.name =
        "http.server.request";
    root.attributes = {
        {
            "http.request.method",
            "GET"
        },
        {
            "url.path",
            "/health"
        }
    };
    root.status =
        SpanStatus::ok;
    root.started_at = started;
    root.ended_at =
        started + 5ms;

    SpanRecord child;
    child.trace_id =
        root.trace_id;
    child.span_id =
        "fedcba9876543210";
    child.parent_span_id =
        root.span_id;
    child.name =
        "database.query";
    child.attributes = {
        {
            "db.system",
            "postgresql"
        }
    };
    child.status =
        SpanStatus::error;
    child.status_message =
        "query failed";
    child.started_at = started + 1ms;
    child.ended_at = started + 4ms;

    OtlpResource resource;
    resource.service_name =
        "gungnir-test";
    resource.service_version =
        "1.2.3";
    resource.deployment_environment =
        "test";
    resource.attributes.insert_or_assign(
        "service.instance.id",
        "instance-1"
    );

    const auto traces_json =
        otlp::encode_traces_json(
            {root, child},
            resource
        );

    assert(
        traces_json.find(
            "\"resourceSpans\""
        ) != std::string::npos
    );

    assert(
        traces_json.find(
            "\"service.name\""
        ) != std::string::npos
    );

    assert(
        traces_json.find(
            "\"gungnir-test\""
        ) != std::string::npos
    );

    assert(
        traces_json.find(
            "\"traceId\":\"0123456789abcdef0123456789abcdef\""
        ) != std::string::npos
    );

    assert(
        traces_json.find(
            "\"parentSpanId\":\"0123456789abcdef\""
        ) != std::string::npos
    );

    assert(
        traces_json.find(
            "\"kind\":2"
        ) != std::string::npos
    );

    assert(
        traces_json.find(
            "\"code\":2"
        ) != std::string::npos
    );

    MetricPoint counter;
    counter.kind =
        MetricKind::counter;
    counter.name =
        "http.server.request.count";
    counter.value = 1.0;
    counter.attributes = {
        {
            "http.request.method",
            "GET"
        }
    };
    counter.trace_id =
        root.trace_id;
    counter.span_id =
        root.span_id;
    counter.timestamp =
        started;

    MetricPoint gauge;
    gauge.kind =
        MetricKind::gauge;
    gauge.name =
        "http.server.connection.active";
    gauge.value = 2.0;
    gauge.timestamp =
        started;

    MetricPoint histogram;
    histogram.kind =
        MetricKind::histogram;
    histogram.name =
        "http.server.request.duration";
    histogram.value = 5.0;
    histogram.timestamp =
        started;

    const auto metrics_json =
        otlp::encode_metrics_json(
            {
                counter,
                gauge,
                histogram
            },
            resource
        );

    assert(
        metrics_json.find(
            "\"resourceMetrics\""
        ) != std::string::npos
    );

    assert(
        metrics_json.find(
            "\"sum\""
        ) != std::string::npos
    );

    assert(
        metrics_json.find(
            "\"isMonotonic\":true"
        ) != std::string::npos
    );

    assert(
        metrics_json.find(
            "\"gauge\""
        ) != std::string::npos
    );

    assert(
        metrics_json.find(
            "\"histogram\""
        ) != std::string::npos
    );

    assert(
        metrics_json.find(
            "\"exemplars\""
        ) != std::string::npos
    );

    OtlpHttpSettings settings;
    settings.resource = resource;
    settings.request_timeout =
        500ms;
    settings.batch_delay =
        20ms;
    settings.max_batch_size = 2;
    settings.max_queue_size = 8;

    const auto* live_endpoint =
        std::getenv(
            "GUNGNIR_OTLP_ENDPOINT"
        );

    if (live_endpoint != nullptr) {
        settings.endpoint =
            live_endpoint;
    } else {
        settings.endpoint =
            "http://127.0.0.1:1";
    }

    auto exporter =
        std::make_shared<
            OtlpHttpExporter
        >(settings);

    exporter->export_span(root);
    exporter->export_span(child);
    exporter->export_metric(counter);
    exporter->export_metric(gauge);
    exporter->export_metric(histogram);

    exporter->flush();

    assert(
        exporter->dropped() == 0
    );

    if (live_endpoint != nullptr) {
        assert(
            exporter
                ->last_error()
                .empty()
        );
    } else {
        assert(
            !exporter
                ->last_error()
                .empty()
        );
    }

    exporter->shutdown();

    return 0;
}
