#pragma once

#include <memory>

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

    [[nodiscard]] routing::Router& router() noexcept;
    [[nodiscard]] const routing::Router& router() const noexcept;

    void boot();
    void shutdown() noexcept;
    [[nodiscard]] bool is_booted() const noexcept;

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace gungnir
