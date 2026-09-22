#pragma once

#include <chrono>
#include <optional>
#include <string>
#include <string_view>

namespace gungnir::cache {

using Duration = std::chrono::seconds;

class Store {
public:
    virtual ~Store() = default;

    [[nodiscard]] virtual std::optional<std::string> get(std::string_view key) = 0;
    virtual void put(std::string key, std::string value, std::optional<Duration> ttl = std::nullopt) = 0;
    virtual bool forget(std::string_view key) = 0;
    virtual void flush() = 0;

    [[nodiscard]] virtual bool has(std::string_view key) {
        return get(key).has_value();
    }
};

} // namespace gungnir::cache
