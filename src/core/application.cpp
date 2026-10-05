#include <gungnir/core/application.hpp>

#include <limits>
#include <stdexcept>
#include <utility>

#include <gungnir/database/runtime.hpp>
#include <gungnir/config/service.hpp>
#include <gungnir/http/server.hpp>
#include <gungnir/routing/route.hpp>
#include <gungnir/view/runtime.hpp>

namespace gungnir {

namespace {

std::filesystem::path normalize_base_path(
    std::filesystem::path path
) {
    if (path.empty()) {
        path =
            std::filesystem::current_path();
    }

    return std::filesystem::absolute(
        std::move(path)
    ).lexically_normal();
}

std::filesystem::path resolve_from(
    const std::filesystem::path& base,
    std::filesystem::path path
) {
    if (path.is_absolute()) {
        return path.lexically_normal();
    }

    return (base / path).lexically_normal();
}

void apply_environment_defaults(
    config::Repository& repository,
    const config::Environment& environment,
    const std::filesystem::path& base
) {
    repository
        .set(
            "app.name",
            environment.get(
                "APP_NAME",
                "Gungnir"
            )
        )
        .set(
            "app.env",
            environment.get(
                "APP_ENV",
                "development"
            )
        )
        .set(
            "app.debug",
            environment.boolean(
                "APP_DEBUG",
                false
            )
        )
        .set(
            "server.host",
            environment.get(
                "APP_HOST",
                "127.0.0.1"
            )
        )
        .set(
            "server.port",
            environment.integer(
                "APP_PORT",
                8000
            )
        )
        .set(
            "paths.base",
            base.string()
        )
        .set(
            "paths.views",
            environment.get(
                "VIEW_PATH",
                "views"
            )
        )
        .set(
            "database.default",
            environment.get(
                "DB_CONNECTION",
                ""
            )
        )
        .set(
            "database.host",
            environment.get(
                "DB_HOST",
                ""
            )
        )
        .set(
            "database.database",
            environment.get(
                "DB_DATABASE",
                ""
            )
        )
        .set(
            "database.username",
            environment.get(
                "DB_USERNAME",
                ""
            )
        )
        .set(
            "database.password",
            environment.get(
                "DB_PASSWORD",
                ""
            )
        )
        .set(
            "database.options",
            environment.get(
                "DB_OPTIONS",
                ""
            )
        )
        .set(
            "database.name",
            environment.get(
                "DB_NAME",
                "default"
            )
        )
        .set(
            "database.pool_size",
            environment.integer(
                "DB_POOL_SIZE",
                1
            )
        )
        .set(
            "database.pool_acquire_timeout_ms",
            environment.integer(
                "DB_POOL_ACQUIRE_TIMEOUT_MS",
                5000
            )
        )
        .set(
            "database.pool_validation_interval_ms",
            environment.integer(
                "DB_POOL_VALIDATION_INTERVAL_MS",
                30000
            )
        )
        .set(
            "database.pool_reconnect_attempts",
            environment.integer(
                "DB_POOL_RECONNECT_ATTEMPTS",
                1
            )
        );

    const auto database_port =
        environment.find("DB_PORT");

    if (
        database_port &&
        !database_port->empty()
    ) {
        repository.set(
            "database.port",
            environment.integer(
                "DB_PORT"
            )
        );
    }
}

} // namespace

class Application::Impl {
public:
    Impl()
        : config(
            std::make_shared<
                config::Repository
            >()
          ),
          environment(
            std::make_shared<
                config::Environment
            >()
          ),
          base_path(
            normalize_base_path({})
          ) {}

    std::shared_ptr<Container> container{std::make_shared<Container>()};
    std::shared_ptr<routing::Router> router{std::make_shared<routing::Router>()};
    std::shared_ptr<database::Manager> database{std::make_shared<database::Manager>()};
    std::shared_ptr<database::DriverRegistry> drivers{
        std::make_shared<database::DriverRegistry>()
    };
    std::shared_ptr<view::Engine> views{
        std::make_shared<view::Engine>()
    };
    std::shared_ptr<config::Repository> config;
    std::shared_ptr<config::Environment> environment;
    std::filesystem::path base_path;
    std::unique_ptr<http::detail::Server> server;
    bool booted{false};
    Lifecycle lifecycle;
    std::shared_ptr<http::MiddlewareRegistry> middleware_registry{std::make_shared<http::MiddlewareRegistry>()};
    detail::ContextHandle context{std::make_shared<detail::ExecutionContext>()};
    http::RuntimeOptions runtime_options;
    std::vector<std::exception_ptr> shutdown_errors;
    std::size_t registered_providers{0};
    std::vector<std::shared_ptr<Provider>> providers;
};

Application Application::create(
    std::filesystem::path base_path
) {
    Application app;

    app.impl_->base_path =
        normalize_base_path(
            std::move(base_path)
        );

    app.load_environment(
        ".env",
        true
    );

    return app;
}

Application::Application()
    : impl_(std::make_unique<Impl>()) {
    impl_->context->router = impl_->router;
    impl_->context->container = impl_->container;
    impl_->context->middleware = impl_->middleware_registry;
    impl_->context->manager = impl_->database;
    impl_->context->view = impl_->views;
    detail::applications.push_back(impl_->context);
    impl_->router->execution_context(impl_->context);
    impl_->router->middleware_registry(*impl_->middleware_registry);
    impl_->router->service_container(*impl_->container);

    impl_->container->instance<
        config::Repository
    >(impl_->config);

    impl_->container->instance<config::Service>(std::make_shared<config::Service>(impl_->config));

    impl_->container->instance<
        config::Environment
    >(impl_->environment);

    impl_->container->instance<
        database::DriverRegistry
    >(impl_->drivers);

    apply_environment_defaults(
        *impl_->config,
        *impl_->environment,
        impl_->base_path
    );

    view_root(
        impl_->config->string(
            "paths.views",
            "views"
        )
    );
}

Application::~Application() { shutdown(); }

Application::Application(Application&& other) noexcept = default;
Application& Application::operator=(Application&& other) noexcept {
    if (this != &other) { shutdown(); impl_ = std::move(other.impl_); }
    return *this;
}

detail::ContextHandle Application::execution_context() const { return impl_->context; }
detail::ExecutionScope Application::activate() const { return detail::ExecutionScope{impl_->context}; }
const std::vector<std::exception_ptr>& Application::shutdown_errors() const noexcept {
    return impl_->shutdown_errors;
}

Container& Application::container() noexcept {
    return *impl_->container;
}

const Container&
Application::container() const noexcept {
    return *impl_->container;
}

routing::Router&
Application::router() noexcept {
    return *impl_->router;
}

const routing::Router&
Application::router() const noexcept {
    return *impl_->router;
}

database::Manager&
Application::database() noexcept {
    return *impl_->database;
}

const database::Manager&
Application::database() const noexcept {
    return *impl_->database;
}

database::DriverRegistry&
Application::database_drivers() noexcept {
    return *impl_->drivers;
}

const database::DriverRegistry&
Application::database_drivers() const noexcept {
    return *impl_->drivers;
}

view::Engine&
Application::views() noexcept {
    return *impl_->views;
}

const view::Engine&
Application::views() const noexcept {
    return *impl_->views;
}

config::Repository&
Application::config() noexcept {
    return *impl_->config;
}

const config::Repository&
Application::config() const noexcept {
    return *impl_->config;
}

config::Environment&
Application::env() noexcept {
    return *impl_->environment;
}

const config::Environment&
Application::env() const noexcept {
    return *impl_->environment;
}

http::MiddlewareRegistry& Application::middleware_registry() noexcept {
    return *impl_->middleware_registry;
}

const std::filesystem::path&
Application::base_path() const noexcept {
    return impl_->base_path;
}

String Application::environment() const {
    return impl_->config->string(
        "app.env",
        "development"
    );
}

ApplicationMode Application::mode() const noexcept {
    return application_mode(environment());
}

bool Application::is_production() const noexcept {
    return ::gungnir::is_production(mode());
}

LifecycleStage Application::lifecycle_stage() const noexcept {
    return impl_ ? impl_->lifecycle.stage() : LifecycleStage::stopped;
}

Application& Application::provider(std::shared_ptr<Provider> value) {
    if (!value) throw std::invalid_argument("Gungnir provider cannot be null");
    if (impl_->lifecycle.stage() != LifecycleStage::created) throw std::logic_error("Providers must be registered before application boot");
    impl_->providers.push_back(std::move(value));
    return *this;
}

Application& Application::on_boot(Lifecycle::Hook hook) { impl_->lifecycle.on_boot(std::move(hook)); return *this; }
Application& Application::on_ready(Lifecycle::Hook hook) { impl_->lifecycle.on_ready(std::move(hook)); return *this; }
Application& Application::on_shutdown(Lifecycle::Hook hook) { impl_->lifecycle.on_shutdown(std::move(hook)); return *this; }

bool Application::debug() const {
    return impl_->config->boolean(
        "app.debug",
        false
    );
}

Application&
Application::load_environment(
    std::filesystem::path path,
    bool optional
) {
    if (!impl_) {
        throw std::logic_error(
            "Gungnir application is not initialized"
        );
    }

    path = resolve_from(
        impl_->base_path,
        std::move(path)
    );

    impl_->environment->load(
        path,
        optional
    );

    apply_environment_defaults(
        *impl_->config,
        *impl_->environment,
        impl_->base_path
    );

    view_root(
        impl_->config->string(
            "paths.views",
            "views"
        )
    );

    return *this;
}

Application& Application::view_root(
    std::filesystem::path path
) {
    if (!path.is_absolute()) {
        path = resolve_from(
            impl_->base_path,
            std::move(path)
        );
    }

    impl_->views->root(
        std::move(path)
    );

    return *this;
}

Application& Application::database(
    String name,
    database::Backend backend,
    database::DriverFactory factory,
    std::size_t pool_size
) {
    impl_->database->add(
        std::move(name),
        backend,
        std::move(factory),
        pool_size
    );

    return *this;
}

Application& Application::database_driver(
    database::Backend backend,
    database::ConfiguredDriverFactory factory
) {
    impl_->drivers->add(
        backend,
        std::move(factory)
    );

    return *this;
}

Application& Application::configure_database() {
    const auto configured =
        impl_->config->string(
            "database.default"
        );

    if (configured.empty()) {
        return *this;
    }

    auto settings =
        database::settings_from(
            *impl_->config
        );

    if (settings.backend == database::Backend::sqlite) {
        if (settings.database != ":memory:") settings.database = resolve_from(impl_->base_path, settings.database).string();
        if (settings.database == ":memory:" && settings.pool_size != 1)
            throw std::invalid_argument("SQLite :memory: requires a one-connection pool");
    }
    if (
        impl_->database->has(
            settings.name
        )
    ) {
        return *this;
    }

    impl_->database->add(
        settings.name,
        settings.backend,
        impl_->drivers->bind(
            settings
        ),
        database::PoolOptions{
            .size =
                settings.pool_size,
            .acquire_timeout =
                settings.pool_acquire_timeout,
            .validation_interval =
                settings.pool_validation_interval,
            .reconnect_attempts =
                settings.pool_reconnect_attempts
        }
    );

    return *this;
}

void Application::boot() {
    if (!impl_ || impl_->lifecycle.stage() != LifecycleStage::created)
        throw std::logic_error("Gungnir application can only boot once; create a new application after shutdown");
    auto scope = activate();
    impl_->shutdown_errors.clear();
    try {
        impl_->lifecycle.stage(LifecycleStage::registering);
        for (auto& provider : impl_->providers) {
            ++impl_->registered_providers; // Includes a partially registered provider.
            provider->register_services(*this);
        }
        impl_->lifecycle.stage(LifecycleStage::booting);
        configure_database();
        for (auto& provider : impl_->providers) provider->boot(*this);
        impl_->lifecycle.fire_boot(*this);
        for (auto& provider : impl_->providers) provider->ready(*this);
        impl_->lifecycle.fire_ready(*this);
        impl_->booted = true;
        impl_->lifecycle.stage(LifecycleStage::ready);
    } catch (...) {
        const auto failure = std::current_exception();
        shutdown();
        std::rethrow_exception(failure);
    }
}

void Application::shutdown() noexcept {
    if (!impl_ || impl_->lifecycle.stage() == LifecycleStage::stopped ||
        impl_->lifecycle.stage() == LifecycleStage::stopping) return;
    const auto was_started = impl_->lifecycle.stage() != LifecycleStage::created;
    auto scope = activate();
    impl_->lifecycle.stage(LifecycleStage::stopping);
    stop();
    while (impl_->registered_providers > 0) {
        auto& provider = impl_->providers[--impl_->registered_providers];
        try { provider->shutdown(*this); }
        catch (...) { impl_->shutdown_errors.push_back(std::current_exception()); }
    }
    if (was_started) impl_->lifecycle.fire_shutdown(*this, impl_->shutdown_errors);
    impl_->booted = false;
    impl_->lifecycle.stage(LifecycleStage::stopped);
    std::erase_if(detail::applications, [&](const auto& weak) {
        auto context = weak.lock();
        return !context || context == impl_->context;
    });
}

bool Application::is_booted()
    const noexcept {
    return
        impl_ &&
        impl_->booted;
}

Application& Application::http_runtime(
    http::RuntimeOptions options
) {
    if (!impl_) {
        throw std::logic_error(
            "Gungnir application is not initialized"
        );
    }

    if (impl_->server) impl_->server->configure(options);
    impl_->runtime_options = std::move(options);

    return *this;
}

Application& Application::tls(
    http::TlsOptions tls_options
) {
    auto options =
        http_runtime();

    options.tls =
        std::move(tls_options);

    return http_runtime(
        std::move(options)
    );
}

Application& Application::http2(
    http::Http2Options http2_options
) {
    auto options =
        http_runtime();

    options.http2 =
        std::move(http2_options);

    return http_runtime(
        std::move(options)
    );
}

const http::RuntimeOptions&
Application::http_runtime()
    const noexcept {
    return impl_->runtime_options;
}

void Application::run() {
    run(CancellationToken{});
}

void Application::run(CancellationToken cancellation) {
    const auto configured_port =
        impl_->config->integer(
            "server.port",
            8000
        );

    if (
        configured_port <= 0 ||
        configured_port >
            static_cast<Int64>(
                std::numeric_limits<
                    std::uint16_t
                >::max()
            )
    ) {
        throw std::out_of_range(
            "Configured server.port must be between 1 and 65535"
        );
    }

    listen(
        static_cast<std::uint16_t>(
            configured_port
        ),
        impl_->config->string(
            "server.host",
            "127.0.0.1"
        ),
        std::move(cancellation)
    );
}

void Application::listen(
    std::uint16_t port,
    String host
) {
    listen(
        port,
        std::move(host),
        CancellationToken{}
    );
}

void Application::listen(
    std::uint16_t port,
    String host,
    CancellationToken cancellation
) {
    if (!impl_) {
        throw std::logic_error(
            "Gungnir application is not initialized"
        );
    }

    if (!impl_->booted) {
        boot();
    }

    auto scope = activate();
    if (!impl_->server) {
        auto server = std::make_unique<http::detail::Server>(*impl_->router);
        server->configure(impl_->runtime_options);
        server->view_engine(impl_->views);
        impl_->server = std::move(server);
    }
    impl_->lifecycle.stage(LifecycleStage::running);

    try { impl_->server->listen(std::move(host), port, std::move(cancellation)); }
    catch (...) { impl_->lifecycle.stage(LifecycleStage::ready); throw; }
    impl_->lifecycle.stage(LifecycleStage::ready);
}

void Application::stop() noexcept {
    if (
        !impl_ ||
        !impl_->server
    ) {
        return;
    }

    impl_->server->stop();
}

bool Application::is_running()
    const noexcept {
    return
        impl_ &&
        impl_->server &&
        impl_->server->running();
}

bool Application::is_accepting()
    const noexcept {
    return
        impl_ &&
        impl_->server &&
        impl_->server->accepting();
}

std::size_t Application::active_http_dispatches()
    const noexcept {
    if (
        !impl_ ||
        !impl_->server
    ) {
        return 0;
    }

    return
        impl_->server
            ->active_dispatches();
}

} // namespace gungnir
