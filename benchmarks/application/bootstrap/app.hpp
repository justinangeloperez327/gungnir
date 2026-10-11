#pragma once

#include <gungnir/gungnir.hpp>
#include <gungnir/core/services.hpp>
#include <gungnir/core/timer.hpp>
#include <gungnir/database/db.hpp>
#include <gungnir/http/security.hpp>
#include <gungnir/observability/metrics.hpp>
#include <gungnir/security/security.hpp>

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <mutex>

namespace bootstrap {
// Fixed-size buckets keep measurement overhead and memory bounded regardless
// of request count. Percentiles are bucket upper bounds, not exact samples.
class DatabaseMetrics final : public gungnir::observability::MetricSink {
public:
    void export_metric(const gungnir::observability::MetricPoint& point) override {
        if (point.name != "db.client.operation.duration") return;
        std::lock_guard lock{mutex_};
        const auto bucket = std::min(buckets_.size() - 1,
            static_cast<std::size_t>(std::max(0.0, point.value) / width_ms));
        ++buckets_[bucket]; ++count_; sum_ += point.value;
        maximum_ = std::max(maximum_, point.value);
        if (point.value >= width_ms * (buckets_.size() - 1)) ++overflow_;
        if (const auto outcome = point.attributes.find("outcome");
            outcome != point.attributes.end() && outcome->second == "error") ++errors_;
    }

    void reset() {
        std::lock_guard lock{mutex_};
        buckets_.fill(0); count_ = errors_ = overflow_ = 0; sum_ = maximum_ = 0;
    }

    gungnir::Json snapshot() const {
        std::lock_guard lock{mutex_};
        const auto percentile = [this](double fraction) {
            if (count_ == 0) return 0.0;
            const auto rank = static_cast<std::uint64_t>(std::ceil(fraction * count_));
            std::uint64_t cumulative = 0;
            for (std::size_t index = 0; index < buckets_.size(); ++index) {
                cumulative += buckets_[index];
                if (cumulative >= rank)
                    return index + 1 == buckets_.size() ? maximum_ : (index + 1) * width_ms;
            }
            return maximum_;
        };
        return gungnir::Json::object({
            {"queries", static_cast<gungnir::Int64>(count_)},
            {"errors", static_cast<gungnir::Int64>(errors_)},
            {"overflow", static_cast<gungnir::Int64>(overflow_)},
            {"bucket_width_ms", width_ms},
            {"mean_ms", count_ ? sum_ / count_ : 0.0}, {"max_ms", maximum_},
            {"p50_upper_ms", percentile(0.50)}, {"p95_upper_ms", percentile(0.95)},
            {"p99_upper_ms", percentile(0.99)}});
    }

private:
    static constexpr double width_ms = 0.01;
    mutable std::mutex mutex_;
    std::array<std::uint64_t, 4096> buckets_{};
    std::uint64_t count_{}, errors_{}, overflow_{};
    double sum_{}, maximum_{};
};

struct Requests {
    std::atomic<gungnir::Int64> active{}, peak{}, completed{}, admitted_drain{};
    gungnir::Int64 proc_id{};
    std::string sqlite_version;
};

inline void configure(gungnir::Application& app) {
    using namespace gungnir;
    if (app.config().string("server.host") != "127.0.0.1" || !app.is_production() || app.debug())
        throw std::invalid_argument("The load fixture requires loopback, production and debug=false");
    const auto token = app.env().get("GUNGNIR_LOAD_TOKEN");
    if (token.size() != 64) throw std::invalid_argument("Missing load fixture credential");
    auto database_metrics = std::make_shared<DatabaseMetrics>();
    auto requests = std::make_shared<Requests>();
    // /proc may be mounted from a parent PID namespace. Its stat ID is the
    // addressable ID for the external sampler, while signals use Popen's ID.
    std::ifstream proc{std::filesystem::path{"/proc/self/stat"}};
    if (!(proc >> requests->proc_id) || requests->proc_id <= 0)
        throw std::invalid_argument("The load fixture requires readable Linux /proc");
    ServiceOptions services;
    services.meter = std::make_shared<observability::Meter>(database_metrics);
    app.provider<ServicesProvider>(std::move(services));
    app.on_boot([requests](Application&) {
        const auto version = database::DB::connection()->execute("SELECT sqlite_version() AS version");
        requests->sqlite_version = std::get<std::string>(version.rows.front().at("version"));
    });

    http::RuntimeOptions runtime;
    runtime.max_connections = 128;
    runtime.max_active_dispatches = 64;
    // Retain the normal 100-request keep-alive policy; the generator must
    // honor Connection: close and measure subsequent reconnects.
    runtime.max_request_bytes = 16 * 1024;
    runtime.max_header_bytes = 8 * 1024;
    runtime.read_timeout = runtime.write_timeout = runtime.idle_timeout = std::chrono::seconds{10};
    runtime.request_timeout = std::chrono::seconds{15};
    runtime.shutdown_timeout = std::chrono::seconds{5};
    app.http_runtime(runtime);
    app.router().use(http::host_validation({"localhost"}));
    app.router().use([token, requests](Request& request, Next next) -> Task<Response> {
        if (!security::constant_time_equal(request.header("Authorization"), "Bearer " + token))
            co_return Response::text("Unauthenticated", 401);
        if (request.path().starts_with("/bench/") && request.path() != "/bench/drain")
            co_return co_await next(request);
        const auto active = ++requests->active;
        auto peak = requests->peak.load();
        while (peak < active && !requests->peak.compare_exchange_weak(peak, active)) {}
        struct Finish {
            std::shared_ptr<Requests> requests;
            ~Finish() { --requests->active; ++requests->completed; }
        } finish{requests};
        co_return co_await next(request);
    });
    app.router().post("/bench/reset", [database_metrics, requests] {
        if (requests->active != 0) return Response::text("Requests still active", 409);
        database_metrics->reset(); requests->peak = 0; requests->completed = 0;
        requests->admitted_drain = 0;
        return Response::json(Json::Object{{"reset", true}});
    });
    app.router().get("/bench/stats", [database_metrics, requests, &app] {
        return Response::json(Json::Object{
            {"database", database_metrics->snapshot()},
            {"active", requests->active.load()}, {"peak_active", requests->peak.load()},
            {"completed", requests->completed.load()}, {"admitted_drain", requests->admitted_drain.load()},
            {"sqlite_version", requests->sqlite_version},
            {"proc_id", requests->proc_id},
            {"leased", static_cast<Int64>(app.database().pool_stats().leased)}});
    });
    const auto gate = app.base_path() / "drain.release";
    app.router().get("/bench/drain", [requests, gate](Request& request) -> Task<Response> {
        ++requests->admitted_drain;
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds{10};
        while (!std::filesystem::exists(gate)) {
            if (std::chrono::steady_clock::now() >= deadline)
                co_return Response::text("Drain gate was not released", 504);
            co_await sleep_for(std::chrono::milliseconds{10});
        }
        co_return Response::json(Json::Object{{"drained", true}, {"client", request.query("client")}});
    });
    app.on_shutdown([requests, &app](Application&) {
        std::ofstream result{app.base_path() / "stopped.json"};
        result << Json::object({
            {"active", requests->active.load()}, {"completed", requests->completed.load()},
            {"leased", static_cast<Int64>(app.database().pool_stats().leased)}}).dump();
    });
}

inline void boot(gungnir::Application&) {
    // Real persistent SQLite, with the same durability policy as the Linux
    // deployment reference. Migrations and seed data are prepared offline.
    (void)gungnir::database::DB::connection()->execute("PRAGMA journal_mode = WAL");
    (void)gungnir::database::DB::connection()->execute("PRAGMA synchronous = FULL");
}
} // namespace bootstrap
