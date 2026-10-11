#include <gungnir/observability/otlp_http_exporter.hpp>
#include <gungnir/observability/propagation.hpp>
#include <gungnir/http/json.hpp>

#include <algorithm>
#include <charconv>
#include <climits>
#include <cmath>
#include <condition_variable>
#include <deque>
#include <functional>
#include <limits>
#include <mutex>
#include <stdexcept>
#include <thread>
#include <utility>

#include <curl/curl.h>

namespace gungnir::observability {
namespace {
using Json = http::Json;
using Clock = std::chrono::steady_clock;
using Milliseconds = std::chrono::milliseconds;

void ensure_curl_runtime() {
    struct Runtime {
        Runtime() {
            if (curl_global_init(CURL_GLOBAL_DEFAULT) != CURLE_OK)
                throw std::runtime_error("Unable to initialize libcurl for OTLP");
        }
        ~Runtime() { curl_global_cleanup(); }
    };
    static Runtime runtime;
    static_cast<void>(runtime);
}

std::string unix_nanos(std::chrono::system_clock::time_point point) {
    const auto value = std::chrono::duration_cast<std::chrono::nanoseconds>(point.time_since_epoch()).count();
    if (value < 0) throw std::invalid_argument("OTLP timestamps must not precede the Unix epoch");
    return std::to_string(value);
}

Json attributes_json(const Attributes& attributes) {
    Json::Array result;
    // Stable ordering also makes exports reproducible across hash implementations.
    std::vector<std::string> keys;
    for (const auto& [key, value] : attributes) { static_cast<void>(value); keys.push_back(key); }
    std::sort(keys.begin(), keys.end());
    for (const auto& key : keys)
        result.push_back(Json::object({{"key", key}, {"value", Json::object({{"stringValue", attributes.at(key)}})}}));
    return Json::array(std::move(result));
}

Json resource_json(const OtlpResource& resource) {
    auto attributes = resource.attributes;
    attributes.insert_or_assign("service.name", resource.service_name);
    if (!resource.service_version.empty()) attributes.insert_or_assign("service.version", resource.service_version);
    if (!resource.deployment_environment.empty()) attributes.insert_or_assign("deployment.environment.name", resource.deployment_environment);
    return Json::object({{"attributes", attributes_json(attributes)}});
}

int span_kind(std::string_view name) {
    if (name == "http.server.request") return 2;
    if (name == "database.query" || name == "mail.send") return 3;
    if (name == "queue.job") return 5;
    return 1;
}

void validate_span(const SpanRecord& span) {
    if (!valid_trace_id(span.trace_id, 32) || !valid_trace_id(span.span_id, 16) ||
        (!span.parent_span_id.empty() && !valid_trace_id(span.parent_span_id, 16)) || span.name.empty() ||
        span.ended_at < span.started_at)
        throw std::invalid_argument("Invalid OTLP span record");
}

void validate_metric(const MetricPoint& point) {
    if (point.name.empty() || !std::isfinite(point.value) ||
        (point.kind != MetricKind::gauge && point.value < 0) ||
        (point.kind != MetricKind::counter && point.kind != MetricKind::gauge && point.kind != MetricKind::histogram) ||
        ((!point.trace_id.empty() || !point.span_id.empty()) &&
         (!valid_trace_id(point.trace_id, 32) || !valid_trace_id(point.span_id, 16))))
        throw std::invalid_argument("Invalid OTLP metric point");
}

// Conservative lower bound on encoded size: refuse large inputs before copying
// them to the encoder. The exact serialized envelope is checked afterward.
void add_size(std::size_t& size, std::size_t amount, std::size_t limit) {
    if (size > limit || amount > limit - size) throw std::length_error("OTLP record exceeds the byte limit");
    size += amount;
}
void add_attributes_size(std::size_t& size, const Attributes& attributes, std::size_t limit) {
    for (const auto& [key, value] : attributes) {
        add_size(size, 32, limit); add_size(size, key.size(), limit); add_size(size, value.size(), limit);
    }
}

std::string lower(std::string_view value) {
    std::string result{value};
    for (auto& character : result)
        if (character >= 'A' && character <= 'Z') character = static_cast<char>(character - 'A' + 'a');
    return result;
}

void validate_settings(const OtlpHttpSettings& settings, const OtlpHttpPolicy& policy) {
    ensure_curl_runtime();
    if (settings.endpoint.find_first_of("\r\n?#") != std::string::npos || settings.endpoint.find('\0') != std::string::npos)
        throw std::invalid_argument("OTLP endpoint must not contain a query, fragment or control characters");
    // Parse with libcurl rather than accepting only a prefix (http:// alone is invalid).
    using Url = std::unique_ptr<CURLU, decltype(&curl_url_cleanup)>;
    Url url{curl_url(), &curl_url_cleanup};
    if (!url || curl_url_set(url.get(), CURLUPART_URL, settings.endpoint.c_str(), 0) != CURLUE_OK)
        throw std::invalid_argument("Invalid OTLP HTTP endpoint");
    char* raw_scheme = nullptr;
    if (curl_url_get(url.get(), CURLUPART_SCHEME, &raw_scheme, 0) != CURLUE_OK)
        throw std::invalid_argument("Invalid OTLP HTTP endpoint");
    const std::string scheme{raw_scheme}; curl_free(raw_scheme);
    if (scheme != "http" && scheme != "https") throw std::invalid_argument("OTLP endpoint must use http:// or https://");
    char* credentials = nullptr;
    const auto user = curl_url_get(url.get(), CURLUPART_USER, &credentials, 0); curl_free(credentials);
    if (user == CURLUE_OK) throw std::invalid_argument("OTLP credentials must be configured through headers");
    if (settings.resource.service_name.empty()) throw std::invalid_argument("OTLP service name must not be empty");
    if (settings.request_timeout.count() <= 0 || settings.request_timeout.count() > LONG_MAX || settings.batch_delay.count() <= 0 ||
        !settings.max_batch_size || !settings.max_queue_size || settings.max_batch_size > settings.max_queue_size)
        throw std::invalid_argument("Invalid OTLP batch size or timeout");
    for (const auto& path : {settings.traces_path, settings.metrics_path})
        if (path.empty() || path.front() != '/' || path.find_first_of("\r\n?#") != std::string::npos)
            throw std::invalid_argument("OTLP signal paths must be absolute HTTP paths");
    for (const auto& [name, value] : settings.headers) {
        constexpr std::string_view symbols{"!#$%&'*+-.^_`|~"};
        if (name.empty() || value.find_first_of("\r\n") != std::string::npos || value.find('\0') != std::string::npos)
            throw std::invalid_argument("Invalid OTLP HTTP header");
        for (const auto digit : name)
            if (!((digit >= 'a' && digit <= 'z') || (digit >= 'A' && digit <= 'Z') ||
                  (digit >= '0' && digit <= '9') || symbols.find(digit) != std::string_view::npos))
                throw std::invalid_argument("Invalid OTLP HTTP header name");
        const auto normalized = lower(name);
        if (normalized == "content-type" || normalized == "content-length" || normalized == "transfer-encoding" || normalized == "host")
            throw std::invalid_argument("OTLP transport headers are managed by the exporter");
    }
    if (!policy.max_record_bytes || !policy.max_queue_bytes || policy.max_record_bytes > policy.max_queue_bytes ||
        !policy.max_response_bytes || !policy.max_attempts || policy.retry_delay.count() <= 0 ||
        policy.max_retry_delay < policy.retry_delay || policy.export_timeout.count() <= 0 ||
        policy.flush_timeout.count() <= 0 || policy.shutdown_timeout.count() <= 0)
        throw std::invalid_argument("Invalid OTLP delivery policy");
    for (const auto duration : {settings.batch_delay, policy.retry_delay, policy.max_retry_delay, policy.export_timeout,
                               policy.flush_timeout, policy.shutdown_timeout})
        if (duration > std::chrono::hours{24}) throw std::invalid_argument("OTLP policy durations must not exceed 24 hours");
    if (resource_json(settings.resource).dump().size() > policy.max_record_bytes)
        throw std::invalid_argument("OTLP resource exceeds the record byte limit");
}

struct Response {
    std::string body;
    std::string retry_after;
    std::size_t maximum;
    bool oversized{false};
};
std::size_t receive_body(char* data, std::size_t size, std::size_t count, void* owner) noexcept {
    auto& response = *static_cast<Response*>(owner);
    if (size && count > std::numeric_limits<std::size_t>::max() / size) return 0;
    const auto bytes = size * count;
    if (bytes > response.maximum - response.body.size()) { response.oversized = true; return 0; }
    try { response.body.append(data, bytes); return bytes; } catch (...) { return 0; }
}
std::size_t receive_header(char* data, std::size_t size, std::size_t count, void* owner) noexcept {
    auto& response = *static_cast<Response*>(owner);
    if (size && count > std::numeric_limits<std::size_t>::max() / size) return 0;
    const auto bytes = size * count;
    try {
        const std::string_view line{data, bytes};
        // A final response replaces interim headers such as HTTP 100 Continue.
        if (line.starts_with("HTTP/")) response.retry_after.clear();
        const auto colon = line.find(':');
        if (colon != std::string_view::npos && lower(line.substr(0, colon)) == "retry-after") {
            auto value = line.substr(colon + 1);
            while (!value.empty() && (value.front() == ' ' || value.front() == '\t')) value.remove_prefix(1);
            while (!value.empty() && (value.back() == '\r' || value.back() == '\n' || value.back() == ' ')) value.remove_suffix(1);
            if (value.size() <= 128) response.retry_after = value;
        }
        return bytes;
    } catch (...) { return 0; }
}

struct Delivery {
    bool retryable{false};
    std::size_t rejected{0};
    std::string error;
    std::optional<Milliseconds> retry_after;
};
std::optional<Milliseconds> retry_delay(std::string_view value) {
    if (value.empty()) return {};
    std::uint64_t seconds = 0;
    const auto parsed = std::from_chars(value.data(), value.data() + value.size(), seconds);
    if (parsed.ec == std::errc{} && parsed.ptr == value.data() + value.size()) {
        if (seconds > static_cast<std::uint64_t>(LONG_MAX / 1000)) return Milliseconds{LONG_MAX};
        return Milliseconds{static_cast<long>(seconds * 1000)};
    }
    const auto date = curl_getdate(std::string{value}.c_str(), nullptr);
    if (date < 0) return {};
    const auto now = std::chrono::system_clock::now();
    return std::max(Milliseconds{0}, std::chrono::duration_cast<Milliseconds>(std::chrono::system_clock::from_time_t(date) - now));
}

// The framework JSON parser is recursive. Bound response nesting before parsing
// untrusted collector data, and never echo collector diagnostics/credentials.
void validate_response_depth(std::string_view source) {
    unsigned depth = 0;
    bool quoted = false, escaped = false;
    for (const char digit : source) {
        if (quoted) { if (escaped) escaped = false; else if (digit == '\\') escaped = true; else if (digit == '"') quoted = false; }
        else if (digit == '"') quoted = true;
        else if (digit == '{' || digit == '[') { if (++depth > 16) throw std::invalid_argument("OTLP response nesting exceeds the limit"); }
        else if (digit == '}' || digit == ']') { if (!depth) throw std::invalid_argument("Invalid OTLP response nesting"); --depth; }
    }
}
Delivery parse_success(const std::string& body, bool traces, std::size_t count) {
    try {
        validate_response_depth(body);
        const auto response = Json::parse(body);
        if (!response.is_object()) throw std::invalid_argument("Invalid response");
        const auto* partial = response.get("partialSuccess");
        if (!partial) return {};
        if (!partial->is_object()) throw std::invalid_argument("Invalid partial success");
        std::size_t rejected = 0;
        if (const auto* value = partial->get(traces ? "rejectedSpans" : "rejectedDataPoints")) {
            auto encoded = value->is_string() ? value->string() : value->dump();
            const auto parsed = std::from_chars(encoded.data(), encoded.data() + encoded.size(), rejected);
            if (parsed.ec != std::errc{} || parsed.ptr != encoded.data() + encoded.size() || rejected > count)
                throw std::invalid_argument("Invalid rejected count");
        }
        const auto* message = partial->get("errorMessage");
        if (message && !message->is_string()) throw std::invalid_argument("Invalid partial success message");
        const bool warning = message && !message->string().empty();
        return {.rejected=rejected, .error=(rejected || warning) ? "OTLP collector reported partial success" : "", .retry_after={}};
    } catch (...) {
        return {.rejected=count, .error="Invalid OTLP JSON response", .retry_after={}};
    }
}

Delivery post_json(const OtlpHttpSettings& settings, const OtlpHttpPolicy& policy,
                   std::string_view path, const std::string& payload, std::size_t count, Milliseconds timeout, bool traces,
                   const std::function<bool()>& expired) {
    using Easy = std::unique_ptr<CURL, decltype(&curl_easy_cleanup)>;
    using Headers = std::unique_ptr<curl_slist, decltype(&curl_slist_free_all)>;
    Easy handle{curl_easy_init(), &curl_easy_cleanup};
    if (!handle) throw std::runtime_error("Unable to allocate OTLP HTTP request");
    Headers headers{nullptr, &curl_slist_free_all};
    const auto append = [&](const std::string& value) {
        auto* updated = curl_slist_append(headers.get(), value.c_str());
        if (!updated) throw std::bad_alloc{};
        static_cast<void>(headers.release()); headers.reset(updated);
    };
    append("Content-Type: application/json"); append("Accept: application/json");
    for (const auto& [name, value] : settings.headers) append(name + ": " + value);
    auto url = settings.endpoint;
    while (!url.empty() && url.back() == '/') url.pop_back();
    url.append(path);
    Response response{.body={}, .retry_after={}, .maximum=policy.max_response_bytes};
    curl_easy_setopt(handle.get(), CURLOPT_URL, url.c_str());
    curl_easy_setopt(handle.get(), CURLOPT_POST, 1L);
    curl_easy_setopt(handle.get(), CURLOPT_HTTPHEADER, headers.get());
    curl_easy_setopt(handle.get(), CURLOPT_POSTFIELDS, payload.data());
    curl_easy_setopt(handle.get(), CURLOPT_POSTFIELDSIZE_LARGE, static_cast<curl_off_t>(payload.size()));
    curl_easy_setopt(handle.get(), CURLOPT_TIMEOUT_MS, static_cast<long>(std::min<Milliseconds::rep>(LONG_MAX, std::max<Milliseconds::rep>(1, timeout.count()))));
    curl_easy_setopt(handle.get(), CURLOPT_SSL_VERIFYPEER, settings.verify_tls_peer ? 1L : 0L);
    curl_easy_setopt(handle.get(), CURLOPT_SSL_VERIFYHOST, settings.verify_tls_host ? 2L : 0L);
    if (!policy.ca_file.empty()) curl_easy_setopt(handle.get(), CURLOPT_CAINFO, policy.ca_file.c_str());
    curl_easy_setopt(handle.get(), CURLOPT_NOSIGNAL, 1L);
    curl_easy_setopt(handle.get(), CURLOPT_WRITEFUNCTION, &receive_body);
    curl_easy_setopt(handle.get(), CURLOPT_WRITEDATA, &response);
    curl_easy_setopt(handle.get(), CURLOPT_HEADERFUNCTION, &receive_header);
    curl_easy_setopt(handle.get(), CURLOPT_HEADERDATA, &response);
    // No redirect following: credentials stay at the configured collector.
    using Multi = std::unique_ptr<CURLM, decltype(&curl_multi_cleanup)>;
    Multi multi{curl_multi_init(), &curl_multi_cleanup};
    if (!multi || curl_multi_add_handle(multi.get(), handle.get()) != CURLM_OK)
        throw std::runtime_error("Unable to allocate OTLP HTTP transfer");
    int running = 0;
    CURLcode status = CURLE_OK;
    while (true) {
        if (expired()) {
            curl_multi_remove_handle(multi.get(), handle.get());
            return {.rejected=count, .error="OTLP export deadline exceeded", .retry_after={}};
        }
        if (curl_multi_perform(multi.get(), &running) != CURLM_OK) { status = CURLE_RECV_ERROR; break; }
        if (!running) {
            int messages = 0;
            if (const auto* message = curl_multi_info_read(multi.get(), &messages); message && message->msg == CURLMSG_DONE)
                status = message->data.result;
            else status = CURLE_RECV_ERROR;
            break;
        }
        int descriptors = 0;
        if (curl_multi_poll(multi.get(), nullptr, 0, 50, &descriptors) != CURLM_OK) { status = CURLE_RECV_ERROR; break; }
    }
    curl_multi_remove_handle(multi.get(), handle.get());
    if (response.oversized) return {.rejected=count, .error="OTLP response exceeds the byte limit", .retry_after={}};
    if (status != CURLE_OK) {
        const bool retryable = status == CURLE_COULDNT_CONNECT || status == CURLE_COULDNT_RESOLVE_HOST ||
            status == CURLE_OPERATION_TIMEDOUT || status == CURLE_SEND_ERROR || status == CURLE_RECV_ERROR || status == CURLE_GOT_NOTHING;
        return {.retryable=retryable, .rejected=count, .error="OTLP HTTP request failed: " + std::string{curl_easy_strerror(status)}, .retry_after={}};
    }
    long code = 0;
    curl_easy_getinfo(handle.get(), CURLINFO_RESPONSE_CODE, &code);
    if (code == 200) return parse_success(response.body, traces, count);
    return {.retryable=code == 429 || code == 502 || code == 503 || code == 504, .rejected=count,
            .error="OTLP collector returned HTTP status " + std::to_string(code), .retry_after=retry_delay(response.retry_after)};
}
} // namespace

namespace otlp {
std::string encode_traces_json(const std::vector<SpanRecord>& spans, const OtlpResource& resource) {
    Json::Array records;
    for (const auto& span : spans) {
        validate_span(span);
        Json::Object status{{"code", static_cast<Int64>(span.status == SpanStatus::ok ? 1 : span.status == SpanStatus::error ? 2 : 0)}};
        if (!span.status_message.empty()) status.emplace("message", span.status_message);
        Json::Object record{{"traceId", span.trace_id}, {"spanId", span.span_id}, {"name", span.name},
            {"kind", static_cast<Int64>(span_kind(span.name))}, {"flags", Int64{1}}, {"startTimeUnixNano", unix_nanos(span.started_at)},
            {"endTimeUnixNano", unix_nanos(span.ended_at)}, {"attributes", attributes_json(span.attributes)}, {"status", Json::object(std::move(status))}};
        if (!span.parent_span_id.empty()) record.emplace("parentSpanId", span.parent_span_id);
        records.push_back(Json::object(std::move(record)));
    }
    const auto scope = Json::object({{"name", "gungnir"}, {"version", GUNGNIR_VERSION_STRING}});
    const auto scope_spans = Json::object({{"scope", scope}, {"spans", Json::array(std::move(records))}});
    return Json::object({{"resourceSpans", Json::array({Json::object({{"resource", resource_json(resource)}, {"scopeSpans", Json::array({scope_spans})}})})}}).dump();
}

std::string encode_metrics_json(const std::vector<MetricPoint>& points, const OtlpResource& resource) {
    struct Group { std::string name; MetricKind kind; Json::Array points; };
    std::vector<Group> groups;
    std::unordered_map<std::string, std::size_t> indices;
    for (const auto& point : points) {
        validate_metric(point);
        const auto key = std::to_string(static_cast<int>(point.kind)) + ':' + point.name;
        auto [found, inserted] = indices.emplace(key, groups.size());
        if (inserted) groups.push_back({point.name, point.kind, {}});
        Json::Object record{{"attributes", attributes_json(point.attributes)}, {"timeUnixNano", unix_nanos(point.timestamp)}};
        if (point.kind == MetricKind::histogram) {
            record.emplace("count", "1"); record.emplace("sum", point.value); record.emplace("min", point.value); record.emplace("max", point.value);
            record.emplace("bucketCounts", Json::array({"1"})); record.emplace("explicitBounds", Json::array({}));
        } else record.emplace("asDouble", point.value);
        if (!point.trace_id.empty()) record.emplace("exemplars", Json::array({Json::object({
            {"timeUnixNano", unix_nanos(point.timestamp)}, {"asDouble", point.value}, {"spanId", point.span_id}, {"traceId", point.trace_id}
        })}));
        groups[found->second].points.push_back(Json::object(std::move(record)));
    }
    Json::Array metrics;
    for (auto& group : groups) {
        Json::Object data{{"dataPoints", Json::array(std::move(group.points))}};
        const auto kind = group.kind == MetricKind::gauge ? "gauge" : group.kind == MetricKind::counter ? "sum" : "histogram";
        if (group.kind != MetricKind::gauge) data.emplace("aggregationTemporality", Int64{1});
        if (group.kind == MetricKind::counter) data.emplace("isMonotonic", true);
        metrics.push_back(Json::object({{"name", group.name}, {kind, Json::object(std::move(data))}}));
    }
    const auto scope = Json::object({{"name", "gungnir"}, {"version", GUNGNIR_VERSION_STRING}});
    const auto scope_metrics = Json::object({{"scope", scope}, {"metrics", Json::array(std::move(metrics))}});
    return Json::object({{"resourceMetrics", Json::array({Json::object({{"resource", resource_json(resource)}, {"scopeMetrics", Json::array({scope_metrics})}})})}}).dump();
}
} // namespace otlp

class OtlpHttpExporter::Impl {
public:
    Impl(OtlpHttpSettings value, OtlpHttpPolicy delivery_policy) : settings(std::move(value)), policy(std::move(delivery_policy)) {
        validate_settings(settings, policy);
        worker = std::thread{[this] { worker_loop(); }};
    }
    ~Impl() { shutdown(); }

    void export_span(const SpanRecord& span) {
        try {
            std::size_t size = 0;
            for (const auto& value : {std::string_view{span.name}, std::string_view{span.status_message}, std::string_view{span.trace_id},
                                     std::string_view{span.span_id}, std::string_view{span.parent_span_id}}) add_size(size, value.size(), policy.max_record_bytes);
            add_attributes_size(size, span.attributes, policy.max_record_bytes);
            size = otlp::encode_traces_json({span}, settings.resource).size();
            enqueue(spans, span, size, accepted_spans);
        } catch (const std::exception&) { reject("Invalid or oversized OTLP span record"); }
    }
    void export_metric(const MetricPoint& point) {
        try {
            std::size_t size = 0;
            for (const auto& value : {std::string_view{point.name}, std::string_view{point.trace_id}, std::string_view{point.span_id}})
                add_size(size, value.size(), policy.max_record_bytes);
            add_attributes_size(size, point.attributes, policy.max_record_bytes);
            size = otlp::encode_metrics_json({point}, settings.resource).size();
            enqueue(metrics, point, size, accepted_metrics);
        } catch (const std::exception&) { reject("Invalid or oversized OTLP metric point"); }
    }
    void flush() {
        std::unique_lock lock{mutex};
        const auto target_spans = accepted_spans, target_metrics = accepted_metrics;
        flush_requested = true; ready.notify_one();
        if (!drained.wait_for(lock, policy.flush_timeout, [&] { return completed_spans >= target_spans && completed_metrics >= target_metrics; }))
            throw std::runtime_error("OTLP flush deadline exceeded");
    }
    void shutdown() {
        // Serialize concurrent calls (the same exporter may back tracer and meter).
        std::lock_guard lifecycle{shutdown_mutex};
        {
            std::lock_guard lock{mutex};
            if (stopping) return;
            stopping = true; flush_requested = true;
            shutdown_deadline = Clock::now() + policy.shutdown_timeout;
        }
        ready.notify_all();
        if (worker.joinable()) worker.join();
    }
    std::size_t dropped() const noexcept { std::lock_guard lock{mutex}; return dropped_count; }
    std::string last_error() const { std::lock_guard lock{mutex}; return error; }
    OtlpHttpSettings settings;
private:
    template<class T> struct Queued { T record; std::size_t bytes; };
    template<class T> void enqueue(std::deque<Queued<T>>& queue, const T& record, std::size_t bytes, std::size_t& accepted) {
        std::lock_guard lock{mutex};
        if (stopping || failed || spans.size() + metrics.size() >= settings.max_queue_size ||
            bytes > policy.max_record_bytes || bytes > policy.max_queue_bytes - queued_bytes) {
            ++dropped_count; error = stopping ? "OTLP exporter is shut down" : failed ? "OTLP worker failed" : "OTLP queue or record limit exceeded"; return;
        }
        queue.push_back({record, bytes}); queued_bytes += bytes; ++accepted;
        if (spans.size() + metrics.size() >= settings.max_batch_size) ready.notify_one();
    }
    void reject(std::string message) { std::lock_guard lock{mutex}; ++dropped_count; error = std::move(message); }
    template<class T> std::vector<T> take_batch(std::deque<Queued<T>>& queue) {
        std::vector<T> batch;
        const auto count = std::min(queue.size(), settings.max_batch_size);
        batch.reserve(count);
        for (std::size_t index = 0; index < count; ++index) {
            queued_bytes -= queue.front().bytes; batch.push_back(std::move(queue.front().record)); queue.pop_front();
        }
        return batch;
    }
    Clock::time_point deadline(Clock::time_point export_deadline) {
        std::lock_guard lock{mutex};
        return stopping ? std::min(export_deadline, shutdown_deadline) : export_deadline;
    }
    Delivery deliver(std::string_view path, const std::string& payload, std::size_t count, bool traces) {
        const auto export_deadline = Clock::now() + policy.export_timeout;
        auto delay = policy.retry_delay;
        Delivery result{.rejected=count, .error="OTLP export deadline exceeded", .retry_after={}};
        for (unsigned attempt = 0; attempt < policy.max_attempts; ++attempt) {
            const auto remaining = std::chrono::duration_cast<Milliseconds>(deadline(export_deadline) - Clock::now());
            if (remaining.count() <= 0) break;
            result = post_json(settings, policy, path, payload, count, std::min(settings.request_timeout, remaining), traces,
                               [&] { return Clock::now() >= deadline(export_deadline); });
            if (!result.retryable || attempt + 1 == policy.max_attempts) break;
            const auto wait = result.retry_after.value_or(delay);
            // Do not retry early if Retry-After exceeds the remaining budget.
            if (wait >= deadline(export_deadline) - Clock::now()) break;
            const auto wake = Clock::now() + wait;
            std::unique_lock lock{mutex};
            ready.wait_until(lock, wake, [&] { return stopping && shutdown_deadline <= wake; });
            if (stopping && shutdown_deadline <= wake) break;
            delay = delay >= policy.max_retry_delay / 2 ? policy.max_retry_delay : delay * 2;
        }
        return result;
    }
    template<class T, class Encode> Delivery send(const std::vector<T>& batch, std::string_view path, bool traces, Encode encode) {
        if (batch.empty()) return {};
        try { return deliver(path, encode(batch, settings.resource), batch.size(), traces); }
        catch (...) { return {.rejected=batch.size(), .error="OTLP exporter failed to encode or send a batch", .retry_after={}}; }
    }
    void worker_loop() noexcept {
        try {
            while (true) {
                std::vector<SpanRecord> span_batch;
                std::vector<MetricPoint> metric_batch;
                {
                    std::unique_lock lock{mutex};
                    ready.wait_for(lock, settings.batch_delay, [&] {
                        return stopping || flush_requested || spans.size() + metrics.size() >= settings.max_batch_size;
                    });
                    if (stopping && Clock::now() >= shutdown_deadline) {
                        dropped_count += spans.size() + metrics.size();
                        completed_spans += spans.size(); completed_metrics += metrics.size();
                        if (!spans.empty() || !metrics.empty()) error = "OTLP shutdown deadline exceeded";
                        spans.clear(); metrics.clear(); queued_bytes = 0;
                    }
                    if (spans.empty() && metrics.empty()) {
                        flush_requested = false; drained.notify_all();
                        if (stopping) return;
                        continue;
                    }
                    span_batch = take_batch(spans); metric_batch = take_batch(metrics);
                    if (spans.empty() && metrics.empty()) flush_requested = false;
                }
                // A failed signal must not suppress delivery of the other signal.
                const auto traces = send(span_batch, settings.traces_path, true, &otlp::encode_traces_json);
                const auto measurements = send(metric_batch, settings.metrics_path, false, &otlp::encode_metrics_json);
                {
                    std::lock_guard lock{mutex};
                    dropped_count += traces.rejected + measurements.rejected;
                    completed_spans += span_batch.size(); completed_metrics += metric_batch.size();
                    error = !traces.error.empty() ? traces.error : measurements.error;
                    drained.notify_all();
                }
            }
        } catch (...) {
            std::lock_guard lock{mutex};
            // Allocation failures may happen before batches reach the transport.
            dropped_count += accepted_spans - completed_spans + accepted_metrics - completed_metrics;
            completed_spans = accepted_spans; completed_metrics = accepted_metrics;
            spans.clear(); metrics.clear(); queued_bytes = 0; failed = true;
            error = "OTLP worker failed"; drained.notify_all();
        }
    }
    OtlpHttpPolicy policy;
    mutable std::mutex mutex;
    std::mutex shutdown_mutex;
    std::condition_variable ready, drained;
    std::deque<Queued<SpanRecord>> spans;
    std::deque<Queued<MetricPoint>> metrics;
    std::thread worker;
    std::size_t queued_bytes{0}, dropped_count{0};
    std::size_t accepted_spans{0}, accepted_metrics{0}, completed_spans{0}, completed_metrics{0};
    std::string error;
    bool flush_requested{false}, stopping{false}, failed{false};
    Clock::time_point shutdown_deadline{Clock::time_point::max()};
};

OtlpHttpExporter::OtlpHttpExporter(OtlpHttpSettings settings) : OtlpHttpExporter(std::move(settings), {}) {}
OtlpHttpExporter::OtlpHttpExporter(OtlpHttpSettings settings, OtlpHttpPolicy policy) : impl_(std::make_unique<Impl>(std::move(settings), std::move(policy))) {}
OtlpHttpExporter::~OtlpHttpExporter() = default;
OtlpHttpExporter::OtlpHttpExporter(OtlpHttpExporter&&) noexcept = default;
OtlpHttpExporter& OtlpHttpExporter::operator=(OtlpHttpExporter&&) noexcept = default;
void OtlpHttpExporter::export_span(const SpanRecord& span) { impl_->export_span(span); }
void OtlpHttpExporter::export_metric(const MetricPoint& point) { impl_->export_metric(point); }
void OtlpHttpExporter::flush() { impl_->flush(); }
void OtlpHttpExporter::shutdown() { impl_->shutdown(); }
std::size_t OtlpHttpExporter::dropped() const noexcept { return impl_->dropped(); }
std::string OtlpHttpExporter::last_error() const { return impl_->last_error(); }
const OtlpHttpSettings& OtlpHttpExporter::settings() const noexcept { return impl_->settings; }
} // namespace gungnir::observability
