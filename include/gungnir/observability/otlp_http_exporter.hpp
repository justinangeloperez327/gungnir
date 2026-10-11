#pragma once

#include <chrono>
#include <cstddef>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include <gungnir/observability/metrics.hpp>
#include <gungnir/observability/trace.hpp>
#include <gungnir/version.hpp>

namespace gungnir::observability {

struct OtlpResource {
    std::string service_name{"gungnir"};
    std::string service_version{GUNGNIR_VERSION_STRING};
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

// Separate from OtlpHttpSettings to preserve the existing settings layout and
// constructor signature. Limits include retries and apply to each signal batch.
struct OtlpHttpPolicy {
    std::size_t max_record_bytes{64 * 1024};
    std::size_t max_queue_bytes{4 * 1024 * 1024};
    std::size_t max_response_bytes{64 * 1024};
    unsigned max_attempts{3};
    std::chrono::milliseconds retry_delay{100};
    std::chrono::milliseconds max_retry_delay{1000};
    std::chrono::milliseconds export_timeout{5000};
    std::chrono::milliseconds flush_timeout{5000};
    std::chrono::milliseconds shutdown_timeout{5000};
    // Empty uses libcurl's system trust store. Peer and host verification remain
    // controlled by OtlpHttpSettings and are both enabled by default.
    std::string ca_file;
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

    OtlpHttpExporter(OtlpHttpSettings settings, OtlpHttpPolicy policy);

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
