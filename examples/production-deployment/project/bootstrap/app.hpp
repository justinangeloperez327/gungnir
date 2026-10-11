#pragma once

#include <gungnir/core/services.hpp>
#include <gungnir/core/timer.hpp>
#include <gungnir/cache/redis_store.hpp>
#include <gungnir/cache/redis_lock.hpp>
#include <gungnir/queue/redis_driver.hpp>
#include <gungnir/scheduler/redis_lock.hpp>
#include <gungnir/session/redis_store.hpp>
#include <gungnir/session/middleware.hpp>
#include <gungnir/database/db.hpp>
#include <gungnir/http/security.hpp>
#include <gungnir/logging/json_stream_sink.hpp>
#include <gungnir/production/health.hpp>
#include <gungnir/security/security.hpp>
#include <gungnir/storage/local_disk.hpp>
#ifdef GNR_ADAPTER_OTLP
#include <gungnir/observability/otlp_http_exporter.hpp>
#endif

#include <charconv>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <regex>

#ifndef GNR_ADAPTER_REDIS
#error This example requires an installed Gungnir SDK built with GUNGNIR_WITH_REDIS=ON
#endif

namespace bootstrap {
inline std::string required(const gungnir::config::Environment& env, const char* name) {
    auto value = env.get(name);
    if (value.empty()) throw std::invalid_argument(std::string{"Required production setting: "} + name);
    return value;
}

inline int bounded_integer(const std::string& value, const char* name, int minimum, int maximum) {
    int result{};
    auto parsed = std::from_chars(value.data(), value.data() + value.size(), result);
    if (parsed.ec != std::errc{} || parsed.ptr != value.data() + value.size() || result < minimum || result > maximum)
        throw std::invalid_argument(std::string{"Invalid production setting: "} + name);
    return result;
}

inline std::filesystem::path absolute_path(const std::string& value, const char* name) {
    std::filesystem::path result{value};
    if (!result.is_absolute() || !std::regex_match(value, std::regex{R"(/[A-Za-z0-9_./-]+)"}) || result.lexically_normal() != result)
        throw std::invalid_argument(std::string{"Invalid production path: "} + name);
    return result;
}

inline void configure(gungnir::Application& app) {
    using namespace gungnir;
    const auto env_file = absolute_path(required(app.env(), "GUNGNIR_DEPLOY_ENV"), "GUNGNIR_DEPLOY_ENV");
    app.load_environment(env_file, false);
    if (!app.is_production() || app.debug() || app.config().string("server.host") != "127.0.0.1")
        throw std::invalid_argument("This deployment requires production mode, debug=false and a loopback listener");
    if (app.config().string("database.default") != "sqlite" || app.config().integer("database.pool_size") != 1)
        throw std::invalid_argument("This deployment requires the explicit single-host SQLite profile and one connection per process");
    const auto data = absolute_path(required(app.env(), "DEPLOY_DATA_DIR"), "DEPLOY_DATA_DIR");
    if (absolute_path(required(app.env(), "DB_DATABASE"), "DB_DATABASE") != data / "application.sqlite")
        throw std::invalid_argument("DB_DATABASE must name application.sqlite inside DEPLOY_DATA_DIR");
    if (!std::filesystem::is_directory(data / "storage")) throw std::invalid_argument("Provision the shared storage directory before starting");
    const auto public_host = required(app.env(), "APP_PUBLIC_HOST");
    if (!std::regex_match(public_host, std::regex{R"([a-z0-9][a-z0-9.-]{0,252})"}))
        throw std::invalid_argument("Invalid production setting: APP_PUBLIC_HOST");
    const auto token = required(app.env(), "APP_API_TOKEN");
    const auto password = required(app.env(), "REDIS_PASSWORD");
    const auto hex = std::regex{R"([a-f0-9]{64})"};
    if (!std::regex_match(token, hex) || !std::regex_match(password, hex) || token == password)
        throw std::invalid_argument("APP_API_TOKEN and REDIS_PASSWORD must be independent 64-character lowercase hex secrets");
    const auto name = required(app.env(), "APP_NAMESPACE");
    if (!std::regex_match(name, std::regex{R"([a-z0-9][a-z0-9-]{0,63})"}))
        throw std::invalid_argument("Invalid production setting: APP_NAMESPACE");
    const auto role = app.env().get("GUNGNIR_DEPLOY_INSTANCE", "command");
    if ((role == "worker" || role == "scheduler") && std::filesystem::exists(data / "maintenance"))
        throw std::invalid_argument("Background work is paused for deployment maintenance");
    std::ifstream revision_file{app.base_path() / "REVISION"};
    std::string revision;
    std::getline(revision_file, revision);
    if (!std::regex_match(revision, std::regex{R"([A-Za-z0-9][A-Za-z0-9_-]{0,63})"}))
        throw std::invalid_argument("A built release must provide a valid REVISION file");
    const auto delay = std::chrono::milliseconds{bounded_integer(app.env().get("REQUEST_WORK_DELAY_MS", "0"), "REQUEST_WORK_DELAY_MS", 0, 1000)};
    const auto prefix = "gungnir:deployment:" + name + ":";

    cache::RedisSettings cache_settings;
    cache_settings.host = "127.0.0.1";
    cache_settings.port = static_cast<std::uint16_t>(bounded_integer(required(app.env(), "REDIS_PORT"), "REDIS_PORT", 1, 65535));
    cache_settings.password = password;
    cache_settings.prefix = prefix + "cache:";
    cache_settings.connect_timeout = std::chrono::milliseconds{250};
    cache_settings.command_timeout = std::chrono::milliseconds{500};
    auto cache_store = std::make_shared<cache::RedisStore>(cache_settings);

    cache::RedisLockSettings locks;
    locks.host = cache_settings.host; locks.port = cache_settings.port; locks.password = password;
    locks.connect_timeout = cache_settings.connect_timeout; locks.command_timeout = cache_settings.command_timeout;
    locks.prefix = prefix + "cache-locks:";
    auto cache_locks = std::make_shared<cache::RedisLockStore>(locks);
    locks.prefix = prefix + "scheduler-locks:";
    auto scheduler_locks = std::make_shared<scheduler::RedisLockStore>(locks);

    queue::RedisSettings queue_settings;
    queue_settings.host = cache_settings.host; queue_settings.port = cache_settings.port; queue_settings.password = password;
    queue_settings.connect_timeout = cache_settings.connect_timeout; queue_settings.command_timeout = cache_settings.command_timeout;
    queue_settings.prefix = prefix + "queue:";
    queue_settings.visibility_timeout = std::chrono::seconds{30};
    auto queue_driver = std::make_shared<queue::RedisDriver>(queue_settings);

    auto logger = std::make_shared<logging::Logger>();
    logger->sink(std::make_shared<logging::JsonStreamSink>(std::cout));
    ServiceOptions services;
    services.cache = cache_store;
    services.cache_locks = cache_locks;
    services.queue = queue_driver;
    services.scheduler_locks = scheduler_locks;
    services.worker_options.lease_renewal_interval = std::chrono::seconds{2};
    services.worker_options.retry_backoff = {std::chrono::seconds{1}, std::chrono::seconds{2}};
    services.logger = logger;
    services.storage = std::make_shared<storage::Manager>();
    services.storage->add("local", std::make_shared<storage::LocalDisk>(data / "storage"));

    const auto otlp_endpoint = app.env().get("OTLP_ENDPOINT");
    if (!otlp_endpoint.empty()) {
#ifdef GNR_ADAPTER_OTLP
        observability::OtlpHttpSettings settings;
        settings.endpoint = otlp_endpoint;
        settings.resource.service_name = "gungnir-production-example";
        settings.resource.deployment_environment = "production";
        settings.resource.attributes = {{"service.instance.id", role}, {"deployment.revision", revision}};
        const auto authorization = app.env().get("OTLP_AUTHORIZATION");
        if (!authorization.empty()) settings.headers.emplace("Authorization", authorization);
        observability::OtlpHttpPolicy policy;
        policy.ca_file = app.env().get("OTLP_CA_FILE");
        auto exporter = std::make_shared<observability::OtlpHttpExporter>(settings, policy);
        services.tracer = std::make_shared<observability::Tracer>(exporter);
        services.meter = std::make_shared<observability::Meter>(exporter);
#else
        throw std::invalid_argument("OTLP_ENDPOINT requires an installed SDK with the optional OTLP adapter");
#endif
    }
    app.provider<ServicesProvider>(std::move(services));

    http::RuntimeOptions runtime;
    runtime.max_request_bytes = 64 * 1024;
    runtime.max_header_bytes = 16 * 1024;
    runtime.max_connections = 128;
    runtime.max_active_dispatches = 64;
    runtime.read_timeout = runtime.write_timeout = runtime.request_timeout = runtime.idle_timeout = std::chrono::seconds{5};
    runtime.shutdown_timeout = std::chrono::seconds{5};
    app.http_runtime(runtime);

    session::RedisSessionSettings sessions;
    sessions.redis = cache_settings; sessions.redis.prefix = prefix + "sessions:";
    auto session_store = std::make_shared<session::RedisStore>(sessions);

    auto health = std::make_shared<production::Health>();
    health->liveness("application", [] { return true; });
    health->readiness("lifecycle", [&app] {
        return app.lifecycle_stage() == LifecycleStage::ready || app.lifecycle_stage() == LifecycleStage::running;
    });
    health->readiness("maintenance", [data] { return !std::filesystem::exists(data / "maintenance"); });
    health->readiness("database", [] {
        (void)database::DB::connection()->execute("SELECT id FROM notes LIMIT 1");
        return true;
    });
    // Probe each owned connection, including sessions. A server restart can
    // invalidate one client's connection while another client's PING succeeds.
    health->readiness("redis-cache", [cache_store] { return cache_store->ping(); });
    health->readiness("redis-queue", [queue_driver] { return queue_driver->ping(); });
    health->readiness("redis-cache-locks", [cache_locks] { return cache_locks->ping(); });
    health->readiness("redis-scheduler-locks", [scheduler_locks] { return scheduler_locks->ping(); });
    health->readiness("redis-sessions", [session_store] { return session_store->ping(); });

    app.router().use(http::host_validation({public_host}));
    app.router().use(http::trusted_proxies({.proxies = {"127.0.0.1"}}));
    app.router().use(http::security_headers());
    app.router().use(http::request_id());
    app.router().use([health, token, role, revision](Request& request, Next next) -> Task<Response> {
        if (request.path() == "/health/live") co_return Response::json(Json::Object{{"live", health->live()}}, health->live() ? 200 : 503);
        if (request.path() == "/health/ready") {
            const auto ready = health->ready();
            co_return Response::json(Json::Object{{"ready", ready}}, ready ? 200 : 503);
        }
        if (!request.secure()) co_return Response::text("HTTPS required", 400);
        if (!security::constant_time_equal(request.header("Authorization"), "Bearer " + token))
            co_return Response::text("Unauthenticated", 401);
        if (!health->ready()) co_return Response::text("Service unavailable", 503);
        if (request.path() == "/context") co_return Response::json(Json::Object{
            {"secure", request.secure()}, {"client_ip", std::string{request.client_ip()}}, {"instance", role}, {"revision", revision}});
        auto response = co_await next(request);
        response.header("X-Deployment-Instance", role).header("X-Deployment-Revision", revision);
        co_return response;
    });
    app.router().use([logger, role, delay](Request& request, Next next) -> Task<Response> {
        logger->info("Request started", {{"instance", role}, {"method", std::string{request.method_name()}}, {"path", std::string{request.path()}}});
        if (delay.count() > 0 && request.method() == http::Method::post && request.path() == "/notes") co_await sleep_for(delay);
        co_return co_await next(request);
    });
    app.router().use(session::middleware(session_store, {.secure = true}));
    app.on_shutdown([logger, role](Application&) { logger->info("Application stopped", {{"instance", role}}); });
}

inline void boot(gungnir::Application&) {
    // Schema changes are exclusively the explicit migrations command.
    (void)gungnir::database::DB::connection()->execute("PRAGMA journal_mode = WAL");
    (void)gungnir::database::DB::connection()->execute("PRAGMA synchronous = FULL");
}
} // namespace bootstrap
