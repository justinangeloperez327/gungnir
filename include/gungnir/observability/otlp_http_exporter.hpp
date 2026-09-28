#pragma once

#include <chrono>
#include <cstddef>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include <gungnir/observability/metrics.hpp>
#include <gungnir/observability/trace.hpp>

namespace gungnir::observability {

struct OtlpResource {
    std::string service_name{"gungnir"};
    std::string service_version{"0.1.0"};
    std::string deployment_environment;
    Attributes attributes;
};

struct OtlpHttpSettings {
    std::string endpoint{
        "http://127.0.0.1:4318"
    };
    std::string traces_path{
        "/v1/traces"
    };
    std::string metrics_path{
        "/v1/metrics"
    };
    OtlpResource resource;
    std::unordered_map<
        std::string,
        std::string
    > headers;
    std::chrono::milliseconds
        request_timeout{
            10000
        };
    std::chrono::milliseconds
        batch_delay{
            5000
        };
    std::size_t max_batch_size{512};
    std::size_t max_queue_size{2048};
    bool verify_tls_peer{true};
    bool verify_tls_host{true};
};

namespace otlp {

[[nodiscard]]
std::string encode_traces_json(
    const std::vector<SpanRecord>& spans,
    const OtlpResource& resource
);

[[nodiscard]]
std::string encode_metrics_json(
    const std::vector<MetricPoint>& points,
    const OtlpResource& resource
);

} // namespace otlp

class OtlpHttpExporter final :
    public SpanSink,
    public MetricSink {
public:
    explicit OtlpHttpExporter(
        OtlpHttpSettings settings = {}
    );

    ~OtlpHttpExporter() override;

    OtlpHttpExporter(
        const OtlpHttpExporter&
    ) = delete;

    OtlpHttpExporter& operator=(
        const OtlpHttpExporter&
    ) = delete;

    OtlpHttpExporter(
        OtlpHttpExporter&&
    ) noexcept;

    OtlpHttpExporter& operator=(
        OtlpHttpExporter&&
    ) noexcept;

    void export_span(
        const SpanRecord& span
    ) override;

    void export_metric(
        const MetricPoint& point
    ) override;

    void flush() override;
    void shutdown() override;

    [[nodiscard]]
    std::size_t dropped()
        const noexcept;

    [[nodiscard]]
    std::string last_error()
        const;

    [[nodiscard]]
    const OtlpHttpSettings& settings()
        const noexcept;

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace gungnir::observability
