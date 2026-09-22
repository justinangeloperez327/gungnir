#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <memory>
#include <vector>
#include <utility>

#include <gungnir/config/environment.hpp>
#include <gungnir/config/repository.hpp>
#include <gungnir/core/container.hpp>
#include <gungnir/core/lifecycle.hpp>
#include <gungnir/core/mode.hpp>
#include <gungnir/core/provider.hpp>
#include <gungnir/database/backend.hpp>
#include <gungnir/database/driver.hpp>
#include <gungnir/database/manager.hpp>
#include <gungnir/database/registry.hpp>
#include <gungnir/database/settings.hpp>
#include <gungnir/http/middleware.hpp>
#include <gungnir/routing/router.hpp>
#include <gungnir/view/engine.hpp>

namespace gungnir {

class Application {
public:
    [[nodiscard]] static Application create(
        std::filesystem::path base_path = {}
    );

    Application();
    ~Application();

    Application(Application&& other) noexcept;
    Application& operator=(Application&& other) noexcept;

    Application(const Application&) = delete;
    Application& operator=(const Application&) = delete;

    [[nodiscard]] Container& container() noexcept;
    [[nodiscard]] const Container& container() const noexcept;

    [[nodiscard]] routing::Router& router() noexcept;
    [[nodiscard]] const routing::Router& router() const noexcept;

    [[nodiscard]] database::Manager& database() noexcept;
    [[nodiscard]] const database::Manager& database() const noexcept;

    [[nodiscard]] database::DriverRegistry& database_drivers() noexcept;
    [[nodiscard]] const database::DriverRegistry& database_drivers()
        const noexcept;

    [[nodiscard]] view::Engine& views() noexcept;
    [[nodiscard]] const view::Engine& views() const noexcept;

    [[nodiscard]] config::Repository& config() noexcept;
    [[nodiscard]] const config::Repository& config() const noexcept;

    [[nodiscard]] config::Environment& env() noexcept;
    [[nodiscard]] const config::Environment& env() const noexcept;

    [[nodiscard]] const std::filesystem::path& base_path()
        const noexcept;

    [[nodiscard]] String environment() const;
    [[nodiscard]] bool debug() const;
    [[nodiscard]] ApplicationMode mode() const noexcept;
    [[nodiscard]] bool is_production() const noexcept;
    [[nodiscard]] LifecycleStage lifecycle_stage() const noexcept;

    Application& provider(std::shared_ptr<Provider> value);

    template <typename ProviderType, typename... Args>
    Application& provider(Args&&... args) {
        return provider(std::make_shared<ProviderType>(std::forward<Args>(args)...));
    }

    Application& on_boot(Lifecycle::Hook hook);
    Application& on_ready(Lifecycle::Hook hook);
    Application& on_shutdown(Lifecycle::Hook hook);

    Application& load_environment(
        std::filesystem::path path = ".env",
        bool optional = true
    );

    Application& view_root(std::filesystem::path path);

    Application& database(
        String name,
        database::Backend backend,
        database::DriverFactory factory,
        std::size_t pool_size = 1
    );

    Application& database_driver(
        database::Backend backend,
        database::ConfiguredDriverFactory factory
    );

    Application& configure_database();

    template <typename Service, typename Implementation = Service>
    Application& bind() {
        container().template bind<Service, Implementation>();
        return *this;
    }

    template <typename Service, typename Factory>
    Application& bind(Factory&& factory) {
        container().template bind<Service>(std::forward<Factory>(factory));
        return *this;
    }

    template <typename Service, typename Implementation = Service>
    Application& singleton() {
        container().template singleton<Service, Implementation>();
        return *this;
    }

    template <typename Service, typename Factory>
    Application& singleton(Factory&& factory) {
        container().template singleton<Service>(std::forward<Factory>(factory));
        return *this;
    }

    template <typename Service>
    Application& instance(std::shared_ptr<Service> value) {
        container().template instance<Service>(std::move(value));
        return *this;
    }

    template <typename Service>
    [[nodiscard]] std::shared_ptr<Service> resolve() {
        return container().template resolve<Service>();
    }

    template <typename MiddlewareType>
    Application& middleware() {
        router().use(
            http::make_middleware<MiddlewareType>(
                container()
            )
        );
        return *this;
    }

    void boot();
    void shutdown() noexcept;
    [[nodiscard]] bool is_booted() const noexcept;

    void run();

    void listen(
        std::uint16_t port = 8000,
        String host = "127.0.0.1"
    );
    void stop() noexcept;
    [[nodiscard]] bool is_running() const noexcept;

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace gungnir
