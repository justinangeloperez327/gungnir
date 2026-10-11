#include <cassert>
#include <atomic>
#include <chrono>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <memory>
#include <thread>

#include <gungnir/observability/otlp_http_exporter.hpp>
#include <gungnir/http/json.hpp>

int main(int argc, char** argv) {
    using namespace gungnir::observability;
    using namespace std::chrono_literals;
    assert(argc == 3);
    const std::string scenario{argv[2]};
    OtlpHttpSettings settings;
    settings.endpoint = argv[1]; settings.resource.service_name = "transport-test";
    settings.headers = {{"Authorization", "Bearer exporter-private"}};
    settings.batch_delay = 1h; settings.max_batch_size = 32; settings.max_queue_size = 64;
    settings.request_timeout = 300ms;
    OtlpHttpPolicy policy;
    policy.retry_delay = 10ms; policy.max_retry_delay = 20ms; policy.export_timeout = 600ms;
    policy.flush_timeout = 1500ms; policy.shutdown_timeout = 200ms; policy.max_response_bytes = 512;
    if (scenario == "tls_trusted" || scenario == "tls_hostname") policy.ca_file = std::getenv("GUNGNIR_OTLP_TEST_CA");
    SpanRecord span;
    span.trace_id = "0123456789abcdef0123456789abcdef"; span.span_id = "0123456789abcdef";
    span.name = "transport.work"; span.started_at = std::chrono::system_clock::now(); span.ended_at = span.started_at + 1ms;
    span.attributes = {{"unicode", "Freyja 🦅"}, {"escaped", "quote\"\nline"}};
    MetricPoint metric;
    metric.name = "transport.precision"; metric.value = 1.23456789012345e-12;
    metric.trace_id = span.trace_id; metric.span_id = span.span_id;

    // Encoding must retain sub-microsecond values and use the framework version.
    const auto encoded = gungnir::http::Json::parse(otlp::encode_metrics_json({metric}, settings.resource));
    const auto& resource = encoded.get("resourceMetrics")->as_array()[0];
    const auto& scope = resource.get("scopeMetrics")->as_array()[0];
    assert(scope.get("scope")->get("version")->string() == GUNGNIR_VERSION_STRING);
    const auto& data = scope.get("metrics")->as_array()[0].get("sum")->get("dataPoints")->as_array()[0];
    assert(std::stod(data.get("asDouble")->dump()) == metric.value);

    if (scenario == "shutdown" || scenario == "flush_deadline") {
        settings.max_batch_size = 1; settings.request_timeout = 10s; policy.export_timeout = 5s;
        if (scenario == "flush_deadline") policy.flush_timeout = 100ms;
        OtlpHttpExporter exporter{settings, policy}; exporter.export_span(span);
        std::this_thread::sleep_for(100ms);
        for (int count = 0; count < 7; ++count) exporter.export_metric(metric);
        if (scenario == "flush_deadline") {
            bool timed_out = false;
            try { exporter.flush(); } catch (const std::runtime_error&) { timed_out = true; }
            assert(timed_out);
        }
        const auto before = std::chrono::steady_clock::now();
        std::thread first{[&] { exporter.shutdown(); }}, second{[&] { exporter.shutdown(); }};
        first.join(); second.join();
        assert(std::chrono::steady_clock::now() - before < 1500ms);
        exporter.flush(); exporter.shutdown();
        assert(exporter.dropped() == 8 && !exporter.last_error().empty());
    } else if (scenario == "overflow") {
        settings.max_batch_size = 128; settings.max_queue_size = 128;
        policy.max_record_bytes = 1024; policy.max_queue_bytes = 2048;
        OtlpHttpExporter exporter{settings, policy};
        const auto capacity = policy.max_queue_bytes / otlp::encode_traces_json({span}, settings.resource).size();
        for (int count = 0; count < 50; ++count) exporter.export_span(span);
        assert(exporter.dropped() == 50 - capacity);
        exporter.flush(); assert(exporter.dropped() == 50 - capacity); exporter.shutdown();
    } else if (scenario == "invalid_records") {
        policy.max_record_bytes = 1024;
        OtlpHttpExporter exporter{settings, policy};
        auto invalid = span; invalid.trace_id = "bad"; exporter.export_span(invalid);
        invalid = span; invalid.name = std::string(2000, 'x'); exporter.export_span(invalid);
        metric.value = std::numeric_limits<double>::quiet_NaN(); exporter.export_metric(metric);
        metric.value = -1; exporter.export_metric(metric);
        exporter.flush(); assert(exporter.dropped() == 4); exporter.shutdown();
        exporter.export_span(span); assert(exporter.dropped() == 5);
    } else if (scenario == "flush_snapshot") {
        OtlpHttpExporter exporter{settings, policy}; exporter.export_span(span);
        std::atomic<bool> stop{false};
        std::thread producer{[&] { while (!stop.load()) exporter.export_metric(metric); }};
        std::this_thread::sleep_for(10ms);
        const auto before = std::chrono::steady_clock::now();
        exporter.flush();
        assert(std::chrono::steady_clock::now() - before < 1s && !stop.load());
        stop = true; producer.join(); exporter.shutdown();
    } else {
        if (scenario == "retry_after" || scenario == "retry_date") policy.export_timeout = 200ms;
        OtlpHttpExporter exporter{settings, policy};
        exporter.export_span(span); exporter.export_span(span);
        exporter.export_metric(metric); exporter.export_metric(metric); exporter.export_metric(metric);
        exporter.flush();
        const auto expected = scenario == "partial" ? 3U :
            scenario == "rejected" || scenario == "malformed" || scenario == "oversized" || scenario == "nested" || scenario == "retry_after" ||
            scenario == "retry_date" || scenario == "bad_partial" || scenario == "redirect" ? 2U :
            scenario == "outage" || scenario == "tls_untrusted" || scenario == "tls_hostname" ? 5U : 0U;
        assert(exporter.dropped() == expected);
        assert(exporter.last_error().empty() == (expected == 0 && scenario != "warning"));
        exporter.shutdown(); exporter.shutdown();
    }
    // Invalid configuration fails before a worker or request is started.
    for (const auto& bad : {"http://", "ftp://localhost", "http://user:secret@localhost", "http://localhost?secret=x"}) {
        auto invalid = settings; invalid.endpoint = bad;
        bool rejected = false;
        try { OtlpHttpExporter exporter{invalid, policy}; } catch (const std::invalid_argument&) { rejected = true; }
        assert(rejected);
    }
    auto invalid = settings; invalid.headers = {{"X-Bad:Name", "x"}};
    bool rejected = false;
    try { OtlpHttpExporter exporter{invalid, policy}; } catch (const std::invalid_argument&) { rejected = true; }
    assert(rejected);
    std::cout << "OTLP transport " << scenario << " passed\n";
}
