#include <gungnir/observability/otlp_http_exporter.hpp>

#include <algorithm>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <memory>
#include <mutex>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <unordered_map>
#include <utility>
#include <vector>

#include <curl/curl.h>

namespace gungnir::observability {

namespace {

class CurlRuntime {
public:
    CurlRuntime() {
        if (
            curl_global_init(
                CURL_GLOBAL_DEFAULT
            ) != CURLE_OK
        ) {
            throw std::runtime_error(
                "Unable to initialize libcurl for OTLP"
            );
        }
    }

    ~CurlRuntime() {
        curl_global_cleanup();
    }
};

void ensure_curl_runtime() {
    static CurlRuntime runtime;
    static_cast<void>(runtime);
}

using EasyHandle =
    std::unique_ptr<
        CURL,
        decltype(&curl_easy_cleanup)
    >;

using HeaderList =
    std::unique_ptr<
        curl_slist,
        decltype(&curl_slist_free_all)
    >;

[[nodiscard]]
std::string escape_json(
    std::string_view value
) {
    std::string output;
    output.reserve(
        value.size() + 8
    );

    constexpr char hex[] =
        "0123456789abcdef";

    for (
        const unsigned char character :
        value
    ) {
        switch (character) {
            case '"':
                output += "\\\"";
                break;
            case '\\':
                output += "\\\\";
                break;
            case '\b':
                output += "\\b";
                break;
            case '\f':
                output += "\\f";
                break;
            case '\n':
                output += "\\n";
                break;
            case '\r':
                output += "\\r";
                break;
            case '\t':
                output += "\\t";
                break;
            default:
                if (character < 0x20) {
                    output += "\\u00";
                    output.push_back(
                        hex[
                            (
                                character >>
                                4
                            ) &
                            0x0f
                        ]
                    );
                    output.push_back(
                        hex[
                            character &
                            0x0f
                        ]
                    );
                } else {
                    output.push_back(
                        static_cast<char>(
                            character
                        )
                    );
                }
                break;
        }
    }

    return output;
}

void append_json_string(
    std::string& output,
    std::string_view value
) {
    output.push_back('"');
    output +=
        escape_json(value);
    output.push_back('"');
}

[[nodiscard]]
std::string unix_nanos(
    std::chrono::system_clock::time_point
        point
) {
    return
        std::to_string(
            std::chrono::
                duration_cast<
                    std::chrono::nanoseconds
                >(
                    point.time_since_epoch()
                ).count()
        );
}

void append_attributes(
    std::string& output,
    const Attributes& attributes
) {
    output += "[";

    bool first = true;

    for (
        const auto& [key, value] :
        attributes
    ) {
        if (!first) {
            output += ",";
        }

        first = false;

        output +=
            "{\"key\":";

        append_json_string(
            output,
            key
        );

        output +=
            ",\"value\":{\"stringValue\":";

        append_json_string(
            output,
            value
        );

        output += "}}";
    }

    output += "]";
}

[[nodiscard]]
Attributes resource_attributes(
    const OtlpResource& resource
) {
    auto attributes =
        resource.attributes;

    attributes.insert_or_assign(
        "service.name",
        resource.service_name
    );

    if (
        !resource
            .service_version
            .empty()
    ) {
        attributes.insert_or_assign(
            "service.version",
            resource.service_version
        );
    }

    if (
        !resource
            .deployment_environment
            .empty()
    ) {
        attributes.insert_or_assign(
            "deployment.environment.name",
            resource.deployment_environment
        );
    }

    return attributes;
}

void append_resource(
    std::string& output,
    const OtlpResource& resource
) {
    output +=
        "{\"attributes\":";

    append_attributes(
        output,
        resource_attributes(
            resource
        )
    );

    output += "}";
}

[[nodiscard]]
int otlp_span_kind(
    std::string_view name
) noexcept {
    if (
        name ==
        "http.server.request"
    ) {
        return 2;
    }

    if (
        name ==
            "database.query" ||
        name ==
            "mail.send"
    ) {
        return 3;
    }

    if (name == "queue.job") {
        return 5;
    }

    return 1;
}

[[nodiscard]]
int otlp_status(
    SpanStatus status
) noexcept {
    switch (status) {
        case SpanStatus::ok:
            return 1;
        case SpanStatus::error:
            return 2;
        case SpanStatus::unset:
            return 0;
    }

    return 0;
}

void append_exemplars(
    std::string& output,
    const MetricPoint& point
) {
    if (
        point.trace_id.empty() ||
        point.span_id.empty()
    ) {
        return;
    }

    output +=
        ",\"exemplars\":[{\"timeUnixNano\":";

    append_json_string(
        output,
        unix_nanos(
            point.timestamp
        )
    );

    output +=
        ",\"asDouble\":" +
        std::to_string(
            point.value
        ) +
        ",\"spanId\":";

    append_json_string(
        output,
        point.span_id
    );

    output +=
        ",\"traceId\":";

    append_json_string(
        output,
        point.trace_id
    );

    output += "}]";
}

void append_number_point(
    std::string& output,
    const MetricPoint& point
) {
    output +=
        "{\"attributes\":";

    append_attributes(
        output,
        point.attributes
    );

    output +=
        ",\"timeUnixNano\":";

    append_json_string(
        output,
        unix_nanos(
            point.timestamp
        )
    );

    output +=
        ",\"asDouble\":" +
        std::to_string(
            point.value
        );

    append_exemplars(
        output,
        point
    );

    output += "}";
}

void append_histogram_point(
    std::string& output,
    const MetricPoint& point
) {
    output +=
        "{\"attributes\":";

    append_attributes(
        output,
        point.attributes
    );

    output +=
        ",\"timeUnixNano\":";

    append_json_string(
        output,
        unix_nanos(
            point.timestamp
        )
    );

    output +=
        ",\"count\":\"1\""
        ",\"sum\":" +
        std::to_string(
            point.value
        ) +
        ",\"min\":" +
        std::to_string(
            point.value
        ) +
        ",\"max\":" +
        std::to_string(
            point.value
        ) +
        ",\"bucketCounts\":[\"1\"]"
        ",\"explicitBounds\":[]";

    append_exemplars(
        output,
        point
    );

    output += "}";
}

[[nodiscard]]
std::string endpoint_url(
    const OtlpHttpSettings& settings,
    std::string_view path
) {
    auto endpoint =
        settings.endpoint;

    while (
        !endpoint.empty() &&
        endpoint.back() == '/'
    ) {
        endpoint.pop_back();
    }

    if (
        path.empty() ||
        path.front() != '/'
    ) {
        endpoint.push_back('/');
    }

    endpoint.append(
        path.data(),
        path.size()
    );

    return endpoint;
}

void validate_settings(
    const OtlpHttpSettings& settings
) {
    if (
        !settings.endpoint.starts_with(
            "http://"
        ) &&
        !settings.endpoint.starts_with(
            "https://"
        )
    ) {
        throw std::invalid_argument(
            "OTLP endpoint must use http:// or https://"
        );
    }

    if (
        settings.resource
            .service_name
            .empty()
    ) {
        throw std::invalid_argument(
            "OTLP service name must not be empty"
        );
    }

    if (
        settings
            .request_timeout
            .count() <= 0 ||
        settings
            .batch_delay
            .count() <= 0
    ) {
        throw std::invalid_argument(
            "OTLP timeouts must be greater than zero"
        );
    }

    if (
        settings.max_batch_size == 0 ||
        settings.max_queue_size == 0 ||
        settings.max_batch_size >
            settings.max_queue_size
    ) {
        throw std::invalid_argument(
            "OTLP batch size must be between one and max queue size"
        );
    }

    for (
        const auto& [name, value] :
        settings.headers
    ) {
        if (
            name.empty() ||
            name.find('\r') !=
                std::string::npos ||
            name.find('\n') !=
                std::string::npos ||
            value.find('\r') !=
                std::string::npos ||
            value.find('\n') !=
                std::string::npos
        ) {
            throw std::invalid_argument(
                "OTLP HTTP header contains invalid characters"
            );
        }
    }
}

void append_header(
    curl_slist*& headers,
    const std::string& value
) {
    auto* updated =
        curl_slist_append(
            headers,
            value.c_str()
        );

    if (updated == nullptr) {
        throw std::bad_alloc{};
    }

    headers = updated;
}

void post_json(
    const OtlpHttpSettings& settings,
    std::string_view path,
    const std::string& payload
) {
    ensure_curl_runtime();

    EasyHandle handle{
        curl_easy_init(),
        &curl_easy_cleanup
    };

    if (!handle) {
        throw std::runtime_error(
            "Unable to allocate OTLP HTTP request"
        );
    }

    curl_slist* raw_headers =
        nullptr;

    HeaderList headers{
        nullptr,
        &curl_slist_free_all
    };

    append_header(
        raw_headers,
        "Content-Type: application/json"
    );

    for (
        const auto& [name, value] :
        settings.headers
    ) {
        append_header(
            raw_headers,
            name + ": " + value
        );
    }

    headers.reset(
        raw_headers
    );

    const auto url =
        endpoint_url(
            settings,
            path
        );

    curl_easy_setopt(
        handle.get(),
        CURLOPT_URL,
        url.c_str()
    );

    curl_easy_setopt(
        handle.get(),
        CURLOPT_POST,
        1L
    );

    curl_easy_setopt(
        handle.get(),
        CURLOPT_HTTPHEADER,
        headers.get()
    );

    curl_easy_setopt(
        handle.get(),
        CURLOPT_POSTFIELDS,
        payload.data()
    );

    curl_easy_setopt(
        handle.get(),
        CURLOPT_POSTFIELDSIZE_LARGE,
        static_cast<curl_off_t>(
            payload.size()
        )
    );

    curl_easy_setopt(
        handle.get(),
        CURLOPT_TIMEOUT_MS,
        static_cast<long>(
            settings
                .request_timeout
                .count()
        )
    );

    curl_easy_setopt(
        handle.get(),
        CURLOPT_SSL_VERIFYPEER,
        settings.verify_tls_peer
            ? 1L
            : 0L
    );

    curl_easy_setopt(
        handle.get(),
        CURLOPT_SSL_VERIFYHOST,
        settings.verify_tls_host
            ? 2L
            : 0L
    );

    curl_easy_setopt(
        handle.get(),
        CURLOPT_NOSIGNAL,
        1L
    );

    const auto status =
        curl_easy_perform(
            handle.get()
        );

    if (status != CURLE_OK) {
        throw std::runtime_error(
            "OTLP HTTP request failed: " +
            std::string{
                curl_easy_strerror(
                    status
                )
            }
        );
    }

    long response_code = 0;

    curl_easy_getinfo(
        handle.get(),
        CURLINFO_RESPONSE_CODE,
        &response_code
    );

    if (
        response_code < 200 ||
        response_code >= 300
    ) {
        throw std::runtime_error(
            "OTLP collector returned HTTP status " +
            std::to_string(
                response_code
            )
        );
    }
}

} // namespace

namespace otlp {

std::string encode_traces_json(
    const std::vector<SpanRecord>& spans,
    const OtlpResource& resource
) {
    std::string output;

    output +=
        "{\"resourceSpans\":[{\"resource\":";

    append_resource(
        output,
        resource
    );

    output +=
        ",\"scopeSpans\":[{\"scope\":"
        "{\"name\":\"gungnir\","
        "\"version\":\"0.1.0\"},"
        "\"spans\":[";

    bool first = true;

    for (const auto& span : spans) {
        if (!first) {
            output += ",";
        }

        first = false;

        output += "{\"traceId\":";
        append_json_string(
            output,
            span.trace_id
        );

        output += ",\"spanId\":";
        append_json_string(
            output,
            span.span_id
        );

        if (
            !span
                .parent_span_id
                .empty()
        ) {
            output +=
                ",\"parentSpanId\":";

            append_json_string(
                output,
                span.parent_span_id
            );
        }

        output += ",\"name\":";
        append_json_string(
            output,
            span.name
        );

        output +=
            ",\"kind\":" +
            std::to_string(
                otlp_span_kind(
                    span.name
                )
            );

        output +=
            ",\"startTimeUnixNano\":";

        append_json_string(
            output,
            unix_nanos(
                span.started_at
            )
        );

        output +=
            ",\"endTimeUnixNano\":";

        append_json_string(
            output,
            unix_nanos(
                span.ended_at
            )
        );

        output +=
            ",\"attributes\":";

        append_attributes(
            output,
            span.attributes
        );

        output +=
            ",\"status\":{\"code\":" +
            std::to_string(
                otlp_status(
                    span.status
                )
            );

        if (
            !span
                .status_message
                .empty()
        ) {
            output +=
                ",\"message\":";

            append_json_string(
                output,
                span.status_message
            );
        }

        output += "}}";
    }

    output += "]}]}]}";

    return output;
}

std::string encode_metrics_json(
    const std::vector<MetricPoint>& points,
    const OtlpResource& resource
) {
    struct Group {
        MetricKind kind;
        std::string name;
        std::vector<
            const MetricPoint*
        > points;
    };

    std::vector<Group> groups;
    std::unordered_map<
        std::string,
        std::size_t
    > indices;

    for (const auto& point : points) {
        const auto key =
            std::to_string(
                static_cast<int>(
                    point.kind
                )
            ) +
            ":" +
            point.name;

        const auto found =
            indices.find(key);

        if (found == indices.end()) {
            indices.emplace(
                key,
                groups.size()
            );

            groups.push_back({
                point.kind,
                point.name,
                {}
            });

            groups.back()
                .points
                .push_back(
                    &point
                );
        } else {
            groups[
                found->second
            ].points.push_back(
                &point
            );
        }
    }

    std::string output;

    output +=
        "{\"resourceMetrics\":[{\"resource\":";

    append_resource(
        output,
        resource
    );

    output +=
        ",\"scopeMetrics\":[{\"scope\":"
        "{\"name\":\"gungnir\","
        "\"version\":\"0.1.0\"},"
        "\"metrics\":[";

    bool first_metric = true;

    for (const auto& group : groups) {
        if (!first_metric) {
            output += ",";
        }

        first_metric = false;

        output += "{\"name\":";
        append_json_string(
            output,
            group.name
        );

        if (
            group.kind ==
            MetricKind::gauge
        ) {
            output +=
                ",\"gauge\":{\"dataPoints\":[";
        } else if (
            group.kind ==
            MetricKind::counter
        ) {
            output +=
                ",\"sum\":{"
                "\"aggregationTemporality\":1,"
                "\"isMonotonic\":true,"
                "\"dataPoints\":[";
        } else {
            output +=
                ",\"histogram\":{"
                "\"aggregationTemporality\":1,"
                "\"dataPoints\":[";
        }

        bool first_point = true;

        for (
            const auto* point :
            group.points
        ) {
            if (!first_point) {
                output += ",";
            }

            first_point = false;

            if (
                group.kind ==
                MetricKind::histogram
            ) {
                append_histogram_point(
                    output,
                    *point
                );
            } else {
                append_number_point(
                    output,
                    *point
                );
            }
        }

        output += "]}}";
    }

    output += "]}]}]}";

    return output;
}

} // namespace otlp

class OtlpHttpExporter::Impl {
public:
    explicit Impl(
        OtlpHttpSettings value
    )
        : settings(
            std::move(value)
          ) {
        validate_settings(
            settings
        );

        worker =
            std::thread{
                [this] {
                    worker_loop();
                }
            };
    }

    ~Impl() {
        shutdown();
    }

    void export_span(
        const SpanRecord& span
    ) {
        std::lock_guard lock{
            mutex
        };

        if (stopping) {
            ++dropped_count;
            return;
        }

        if (
            spans.size() +
                metrics.size() >=
            settings.max_queue_size
        ) {
            ++dropped_count;
            return;
        }

        spans.push_back(span);

        if (
            spans.size() >=
            settings.max_batch_size
        ) {
            ready.notify_one();
        }
    }

    void export_metric(
        const MetricPoint& point
    ) {
        std::lock_guard lock{
            mutex
        };

        if (stopping) {
            ++dropped_count;
            return;
        }

        if (
            spans.size() +
                metrics.size() >=
            settings.max_queue_size
        ) {
            ++dropped_count;
            return;
        }

        metrics.push_back(point);

        if (
            metrics.size() >=
            settings.max_batch_size
        ) {
            ready.notify_one();
        }
    }

    void flush() {
        std::unique_lock lock{
            mutex
        };

        if (
            stopping &&
            !worker.joinable()
        ) {
            return;
        }

        flush_requested = true;
        ready.notify_one();

        drained.wait(
            lock,
            [this] {
                return
                    spans.empty() &&
                    metrics.empty() &&
                    !exporting;
            }
        );
    }

    void shutdown() {
        {
            std::lock_guard lock{
                mutex
            };

            if (
                stopping &&
                !worker.joinable()
            ) {
                return;
            }

            stopping = true;
            flush_requested = true;
        }

        ready.notify_all();

        if (worker.joinable()) {
            worker.join();
        }
    }

    [[nodiscard]]
    std::size_t dropped()
        const noexcept {
        std::lock_guard lock{
            mutex
        };

        return dropped_count;
    }

    [[nodiscard]]
    std::string last_error()
        const {
        std::lock_guard lock{
            mutex
        };

        return error;
    }

    OtlpHttpSettings settings;

private:
    template <typename T>
    static std::vector<T> take_batch(
        std::deque<T>& queue,
        std::size_t maximum
    ) {
        const auto count =
            std::min(
                maximum,
                queue.size()
            );

        std::vector<T> batch;
        batch.reserve(count);

        for (
            std::size_t index = 0;
            index < count;
            ++index
        ) {
            batch.push_back(
                std::move(
                    queue.front()
                )
            );

            queue.pop_front();
        }

        return batch;
    }

    void remember_error(
        std::string message
    ) {
        std::lock_guard lock{
            mutex
        };

        error =
            std::move(message);
    }

    void send(
        const std::vector<SpanRecord>&
            span_batch,
        const std::vector<MetricPoint>&
            metric_batch
    ) {
        try {
            if (!span_batch.empty()) {
                post_json(
                    settings,
                    settings.traces_path,
                    otlp::
                        encode_traces_json(
                            span_batch,
                            settings.resource
                        )
                );
            }

            if (!metric_batch.empty()) {
                post_json(
                    settings,
                    settings.metrics_path,
                    otlp::
                        encode_metrics_json(
                            metric_batch,
                            settings.resource
                        )
                );
            }

            std::lock_guard lock{
                mutex
            };

            error.clear();
        } catch (
            const std::exception& failure
        ) {
            remember_error(
                failure.what()
            );
        } catch (...) {
            remember_error(
                "Unknown OTLP exporter failure"
            );
        }
    }

    void worker_loop()
        noexcept {
        while (true) {
            std::vector<SpanRecord>
                span_batch;

            std::vector<MetricPoint>
                metric_batch;

            {
                std::unique_lock lock{
                    mutex
                };

                ready.wait_for(
                    lock,
                    settings.batch_delay,
                    [this] {
                        return
                            stopping ||
                            flush_requested ||
                            spans.size() >=
                                settings.max_batch_size ||
                            metrics.size() >=
                                settings.max_batch_size;
                    }
                );

                if (
                    stopping &&
                    spans.empty() &&
                    metrics.empty()
                ) {
                    exporting = false;
                    drained.notify_all();
                    return;
                }

                if (
                    spans.empty() &&
                    metrics.empty()
                ) {
                    flush_requested = false;
                    drained.notify_all();
                    continue;
                }

                exporting = true;

                span_batch =
                    take_batch(
                        spans,
                        settings
                            .max_batch_size
                    );

                metric_batch =
                    take_batch(
                        metrics,
                        settings
                            .max_batch_size
                    );

                if (
                    spans.empty() &&
                    metrics.empty()
                ) {
                    flush_requested = false;
                }
            }

            send(
                span_batch,
                metric_batch
            );

            {
                std::lock_guard lock{
                    mutex
                };

                exporting = false;

                if (
                    spans.empty() &&
                    metrics.empty()
                ) {
                    drained.notify_all();
                }

                if (
                    stopping ||
                    flush_requested ||
                    !spans.empty() ||
                    !metrics.empty()
                ) {
                    ready.notify_one();
                }
            }
        }
    }

    mutable std::mutex mutex;
    std::condition_variable ready;
    std::condition_variable drained;
    std::deque<SpanRecord> spans;
    std::deque<MetricPoint> metrics;
    std::thread worker;
    std::size_t dropped_count{0};
    std::string error;
    bool exporting{false};
    bool flush_requested{false};
    bool stopping{false};
};

OtlpHttpExporter::OtlpHttpExporter(
    OtlpHttpSettings settings
)
    : impl_(
        std::make_unique<Impl>(
            std::move(settings)
        )
      ) {}

OtlpHttpExporter::~OtlpHttpExporter() =
    default;

OtlpHttpExporter::OtlpHttpExporter(
    OtlpHttpExporter&&
) noexcept = default;

OtlpHttpExporter&
OtlpHttpExporter::operator=(
    OtlpHttpExporter&&
) noexcept = default;

void OtlpHttpExporter::export_span(
    const SpanRecord& span
) {
    impl_->export_span(span);
}

void OtlpHttpExporter::export_metric(
    const MetricPoint& point
) {
    impl_->export_metric(point);
}

void OtlpHttpExporter::flush() {
    impl_->flush();
}

void OtlpHttpExporter::shutdown() {
    impl_->shutdown();
}

std::size_t OtlpHttpExporter::dropped()
    const noexcept {
    return impl_->dropped();
}

std::string OtlpHttpExporter::last_error()
    const {
    return impl_->last_error();
}

const OtlpHttpSettings&
OtlpHttpExporter::settings()
    const noexcept {
    return impl_->settings;
}

} // namespace gungnir::observability
