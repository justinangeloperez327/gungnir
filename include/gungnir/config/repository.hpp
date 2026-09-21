#pragma once

#include <concepts>
#include <optional>
#include <string_view>
#include <type_traits>
#include <unordered_map>
#include <variant>

#include <gungnir/core/types.hpp>

namespace gungnir::config {

using Value = std::variant<
    std::monostate,
    Boolean,
    Int64,
    Double,
    String
>;

class Repository {
public:
    Repository() = default;

    Repository& set(
        String key,
        Value value
    );

    Repository& set(
        String key,
        String value
    );

    Repository& set(
        String key,
        const char* value
    );

    Repository& set(
        String key,
        Boolean value
    );

    Repository& set(
        String key,
        Int64 value
    );

    Repository& set(
        String key,
        Double value
    );

    template <std::integral T>
    requires (
        !std::same_as<
            std::remove_cvref_t<T>,
            Boolean
        >
    )
    Repository& set(
        String key,
        T value
    ) {
        return set(
            std::move(key),
            static_cast<Int64>(value)
        );
    }

    template <std::floating_point T>
    Repository& set(
        String key,
        T value
    ) {
        return set(
            std::move(key),
            static_cast<Double>(value)
        );
    }

    [[nodiscard]] bool has(
        std::string_view key
    ) const;

    [[nodiscard]] std::optional<Value> find(
        std::string_view key
    ) const;

    [[nodiscard]] String string(
        std::string_view key,
        String fallback = {}
    ) const;

    [[nodiscard]] Int64 integer(
        std::string_view key,
        Int64 fallback = 0
    ) const;

    [[nodiscard]] Boolean boolean(
        std::string_view key,
        Boolean fallback = false
    ) const;

    [[nodiscard]] Double number(
        std::string_view key,
        Double fallback = 0.0
    ) const;

private:
    std::unordered_map<String, Value> values_;
};

} // namespace gungnir::config
