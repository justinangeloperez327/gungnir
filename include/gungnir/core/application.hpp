#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <utility>

#include <gungnir/config/environment.hpp>
#include <gungnir/config/repository.hpp>
#include <gungnir/core/container.hpp>
#include <gungnir/database/backend.hpp>
#include <gungnir/database/driver.hpp>
#include <gungnir/database/manager.hpp>
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
