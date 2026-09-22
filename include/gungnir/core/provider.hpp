#pragma once
#include <string_view>

namespace gungnir {

class Application;

class Provider {
public:
    virtual ~Provider() = default;
    virtual void register_services(Application&) {}
    virtual void boot(Application&) {}
    virtual void ready(Application&) {}
    virtual void shutdown(Application&) noexcept {}
    [[nodiscard]] virtual std::string_view name() const noexcept { return "provider"; }
};

} // namespace gungnir
