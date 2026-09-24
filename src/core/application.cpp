#include <gungnir/core/application.hpp>

#include <limits>
#include <stdexcept>
#include <utility>

#include <gungnir/database/runtime.hpp>
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
          ),
          server(
            std::make_unique<
                http::detail::Server
            >(router)
          ) {}

    Container container;
    routing::Router router;
    database::Manager database;
    std::shared_ptr<database::DriverRegistry> drivers{
        std::make_shared<database::DriverRegistry>()
    };
    view::Engine views;
    std::shared_ptr<config::Repository> config;
    std::shared_ptr<config::Environment> environment;
    std::filesystem::path base_path;
    std::unique_ptr<http::detail::Server> server;
    bool booted{false};
    Lifecycle lifecycle;
    http::MiddlewareRegistry middleware_registry;
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
    routing::detail::bind_route_runtime(
        impl_->router,
        impl_->container
    );
    impl_->router.middleware_registry(impl_->middleware_registry);

    view::runtime::use(
        impl_->views
    );

    impl_->container.instance<
        config::Repository
    >(impl_->config);

    impl_->container.instance<
        config::Environment
    >(impl_->environment);

    impl_->container.instance<
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

Application::~Application() {
    stop();

    if (impl_) {
        routing::detail::unbind_route_runtime(
            impl_->router,
            impl_->container
        );

        if (
            view::runtime::using_engine(
                impl_->views
            )
        ) {
            view::runtime::clear();
        }
    }

    shutdown();
}

Application::Application(
    Application&& other
) noexcept
    : impl_(std::move(other.impl_)) {
    if (impl_) {
        routing::detail::bind_route_runtime(
            impl_->router,
            impl_->container
        );

        view::runtime::use(
            impl_->views
        );
    }

    if (
        impl_ &&
        impl_->booted
    ) {
        database::runtime::use(
            impl_->database
        );
    }
}

Application& Application::operator=(
    Application&& other
) noexcept {
    if (this == &other) {
        return *this;
    }

    if (impl_) {
        routing::detail::unbind_route_runtime(
            impl_->router,
            impl_->container
        );

        if (
            view::runtime::using_engine(
                impl_->views
            )
        ) {
            view::runtime::clear();
        }
    }

    shutdown();
    impl_ = std::move(other.impl_);

    if (impl_) {
        routing::detail::bind_route_runtime(
            impl_->router,
            impl_->container
        );

        view::runtime::use(
            impl_->views
        );
    }

    if (
        impl_ &&
        impl_->booted
    ) {
        database::runtime::use(
            impl_->database
        );
    }

    return *this;
}

Container& Application::container() noexcept {
    return impl_->container;
}

const Container&
Application::container() const noexcept {
    return impl_->container;
}

routing::Router&
Application::router() noexcept {
    return impl_->router;
}

const routing::Router&
Application::router() const noexcept {
    return impl_->router;
}

database::Manager&
Application::database() noexcept {
    return impl_->database;
}

const database::Manager&
Application::database() const noexcept {
    return impl_->database;
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
    return impl_->views;
}

const view::Engine&
Application::views() const noexcept {
    return impl_->views;
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
    return impl_->middleware_registry;
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
    if (impl_->booted) throw std::logic_error("Providers must be registered before application boot");
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

    impl_->views.root(
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
    impl_->database.add(
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

    const auto settings =
        database::settings_from(
            *impl_->config
        );

    if (
        impl_->database.has(
            settings.name
        )
    ) {
        return *this;
    }

    impl_->database.add(
        settings.name,
        settings.backend,
        impl_->drivers->bind(
            settings
        ),
        settings.pool_size
    );

    return *this;
}

void Application::boot() {
    if (impl_->booted) {
        throw std::logic_error(
            "Gungnir application is already booted"
        );
    }

    impl_->lifecycle.stage(LifecycleStage::registering);
    for (auto& provider : impl_->providers) provider->register_services(*this);

    impl_->lifecycle.stage(LifecycleStage::booting);
    configure_database();
    for (auto& provider : impl_->providers) provider->boot(*this);
    impl_->lifecycle.fire_boot(*this);

    database::runtime::use(
        impl_->database
    );

    impl_->booted = true;
    impl_->lifecycle.stage(LifecycleStage::ready);
    for (auto& provider : impl_->providers) provider->ready(*this);
    impl_->lifecycle.fire_ready(*this);
}

void Application::shutdown() noexcept {
    if (
        !impl_ ||
        !impl_->booted
    ) {
        return;
    }

    impl_->lifecycle.stage(LifecycleStage::stopping);
    for (auto it = impl_->providers.rbegin(); it != impl_->providers.rend(); ++it) (*it)->shutdown(*this);
    impl_->lifecycle.fire_shutdown(*this);

    if (
        database::runtime::using_manager(
            impl_->database
        )
    ) {
        database::runtime::clear();
    }

    impl_->booted = false;
    impl_->lifecycle.stage(LifecycleStage::stopped);
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

    impl_->server->configure(
        std::move(options)
    );

    return *this;
}

const http::RuntimeOptions&
Application::http_runtime()
    const noexcept {
    return impl_->server->options();
}

void Application::run() {
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
        )
    );
}

void Application::listen(
    std::uint16_t port,
    String host
) {
    if (!impl_) {
        throw std::logic_error(
            "Gungnir application is not initialized"
        );
    }

    if (!impl_->booted) {
        boot();
    }

    impl_->lifecycle.stage(LifecycleStage::running);
    impl_->server->listen(
        std::move(host),
        port
    );
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

} // namespace gungnir
