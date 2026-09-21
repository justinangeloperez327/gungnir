#pragma once

#include <memory>
#include <utility>

#include <gungnir/core/container.hpp>
#include <gungnir/routing/router.hpp>

namespace gungnir {

class Application {
public:
    Application();
    ~Application();

    Application(Application&&) noexcept;
    Application& operator=(Application&&) noexcept;

    Application(const Application&) = delete;
    Application& operator=(const Application&) = delete;

    [[nodiscard]] Container& container() noexcept;
    [[nodiscard]] const Container& container() const noexcept;

    [[nodiscard]] routing::Router& router() noexcept;
    [[nodiscard]] const routing::Router& router() const noexcept;

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

    void boot();
    void shutdown() noexcept;
    [[nodiscard]] bool is_booted() const noexcept;

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace gungnir
