#pragma once

#include <optional>
#include <stdexcept>
#include <type_traits>
#include <unordered_map>
#include <variant>

#include <gungnir/core/types.hpp>

namespace gungnir::model {

using AttributeValue = std::variant<
    std::monostate,
    std::nullptr_t,
    Boolean,
    Int64,
    UInt64,
    Double,
    String
>;

using AttributeMap = std::unordered_map<String, AttributeValue>;

template <typename>
struct is_optional : std::false_type {};

template <typename T>
struct is_optional<std::optional<T>> : std::true_type {
    using value_type = T;
};

template <typename T>
inline constexpr bool is_optional_v = is_optional<T>::value;

template <typename T>
[[nodiscard]] T value_cast(const AttributeValue& value) {
    if constexpr (is_optional_v<T>) {
        using Inner = typename is_optional<T>::value_type;

        if (
            std::holds_alternative<std::nullptr_t>(value) ||
            std::holds_alternative<std::monostate>(value)
        ) {
            return std::nullopt;
        }

        return T{value_cast<Inner>(value)};
    } else if constexpr (std::same_as<T, String>) {
        if (const auto* found = std::get_if<String>(&value)) {
            return *found;
        }
    } else if constexpr (std::same_as<T, Boolean>) {
        if (const auto* found = std::get_if<Boolean>(&value)) {
            return *found;
        }

        if (const auto* found = std::get_if<Int64>(&value)) {
            return *found != 0;
        }

        if (const auto* found = std::get_if<UInt64>(&value)) {
            return *found != 0;
        }
    } else if constexpr (std::signed_integral<T>) {
        if (const auto* found = std::get_if<Int64>(&value)) {
            return static_cast<T>(*found);
        }

        if (const auto* found = std::get_if<UInt64>(&value)) {
            return static_cast<T>(*found);
        }
    } else if constexpr (std::unsigned_integral<T>) {
        if (const auto* found = std::get_if<UInt64>(&value)) {
            return static_cast<T>(*found);
        }

        if (const auto* found = std::get_if<Int64>(&value)) {
            if (*found < 0) {
                throw std::invalid_argument(
                    "Cannot assign a negative database value to an unsigned model field"
                );
            }
            return static_cast<T>(*found);
        }
    } else if constexpr (std::floating_point<T>) {
        if (const auto* found = std::get_if<Double>(&value)) {
            return static_cast<T>(*found);
        }

        if (const auto* found = std::get_if<Int64>(&value)) {
            return static_cast<T>(*found);
        }

        if (const auto* found = std::get_if<UInt64>(&value)) {
            return static_cast<T>(*found);
        }
    }

    throw std::invalid_argument(
        "Database value cannot be converted to the requested model field type"
    );
}

template <typename T>
[[nodiscard]] AttributeValue to_value(const T& value) {
    if constexpr (is_optional_v<T>) {
        if (!value) {
            return nullptr;
        }

        return to_value(*value);
    } else if constexpr (std::same_as<T, String>) {
        return value;
    } else if constexpr (std::same_as<T, Boolean>) {
        return value;
    } else if constexpr (std::signed_integral<T>) {
        return static_cast<Int64>(value);
    } else if constexpr (std::unsigned_integral<T>) {
        return static_cast<UInt64>(value);
    } else if constexpr (std::floating_point<T>) {
        return static_cast<Double>(value);
    } else {
        static_assert(
            std::same_as<T, void>,
            "Gungnir model field type is not serializable yet"
        );
    }
}

} // namespace gungnir::model
