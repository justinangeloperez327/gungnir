#pragma once

#include <optional>
#include <array>
#include <charconv>
#include <cmath>
#include <limits>
#include <utility>
#include <gungnir/model/decimal.hpp>
#include <stdexcept>
#include <type_traits>
#include <unordered_map>
#include <variant>

#include <gungnir/core/types.hpp>

namespace gungnir::http { class Json; }

namespace gungnir::model {

using AttributeValue = std::variant<
    std::monostate,
    std::nullptr_t,
    Boolean,
    Int64,
    UInt64,
    Double,
    Decimal,
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
    } else if constexpr (std::same_as<T, http::Json>) {
        if (const auto* found = std::get_if<String>(&value)) return T::parse(*found);
        if (std::holds_alternative<std::nullptr_t>(value) || std::holds_alternative<std::monostate>(value)) return T{nullptr};
    } else if constexpr (std::same_as<T, Decimal>) {
        if (const auto* found = std::get_if<Decimal>(&value)) return *found;
        if (const auto* found = std::get_if<String>(&value)) return Decimal{*found};
    } else if constexpr (std::same_as<T, String>) {
        if (const auto* found = std::get_if<Decimal>(&value)) return found->string();
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
            if constexpr (std::integral<T>) {
                if (!std::in_range<T>(*found)) throw std::out_of_range("Database integer is outside the model field range");
            }
            return static_cast<T>(*found);
        }

        if (const auto* found = std::get_if<UInt64>(&value)) {
            if constexpr (std::integral<T>) {
                if (!std::in_range<T>(*found)) throw std::out_of_range("Database integer is outside the model field range");
            }
            return static_cast<T>(*found);
        }
    } else if constexpr (std::unsigned_integral<T>) {
        if (const auto* found = std::get_if<UInt64>(&value)) {
            if constexpr (std::integral<T>) {
                if (!std::in_range<T>(*found)) throw std::out_of_range("Database integer is outside the model field range");
            }
            return static_cast<T>(*found);
        }

        if (const auto* found = std::get_if<Int64>(&value)) {
            if (*found < 0) {
                throw std::invalid_argument(
                    "Cannot assign a negative database value to an unsigned model field"
                );
            }
            if constexpr (std::integral<T>) {
                if (!std::in_range<T>(*found)) throw std::out_of_range("Database integer is outside the model field range");
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
    } else if constexpr (std::same_as<T, http::Json>) {
        return value.dump();
    } else if constexpr (std::same_as<T, std::nullptr_t> || std::same_as<T, std::monostate>) {
        return value;
    } else if constexpr (std::same_as<T, String> || std::same_as<T, Decimal>) {
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

// Explicit model casts may convert database text/numeric representations.
// Ordinary typed fields keep value_cast's strict conversion rules.
template <typename T>
[[nodiscard]] T cast_value(const AttributeValue& value, std::string_view cast) {
    if constexpr (is_optional_v<T>) {
        using Inner = typename is_optional<T>::value_type;
        if (std::holds_alternative<std::nullptr_t>(value) || std::holds_alternative<std::monostate>(value)) return std::nullopt;
        return T{cast_value<Inner>(value,cast)};
    } else if constexpr (std::same_as<T,String>) {
        if (cast == "string") {
            if (const auto* number = std::get_if<Int64>(&value)) return std::to_string(*number);
            if (const auto* number = std::get_if<UInt64>(&value)) return std::to_string(*number);
            if (const auto* flag = std::get_if<Boolean>(&value)) return *flag ? "true" : "false";
        }
    } else if constexpr (std::same_as<T,Boolean>) {
        if (cast == "bool" || cast == "boolean") {
            if (const auto* text = std::get_if<String>(&value)) {
                if (*text == "true" || *text == "1") return true;
                if (*text == "false" || *text == "0") return false;
                throw std::invalid_argument("Boolean cast requires true, false, 1 or 0");
            }
        }
    } else if constexpr (std::integral<T>) {
        if (cast == "int" || cast == "integer" || cast == "int64" || cast == "uint64") {
            if (const auto* text = std::get_if<String>(&value)) {
                T result{};
                const auto [end,error] = std::from_chars(text->data(),text->data()+text->size(),result);
                if (error == std::errc{} && end == text->data()+text->size()) return result;
                throw std::invalid_argument("Integer cast requires a complete in-range integer");
            }
        }
    } else if constexpr (std::floating_point<T>) {
        if (cast == "double" || cast == "float" || cast == "real") {
            if (const auto* number = std::get_if<Decimal>(&value)) {
                const auto result = static_cast<T>(number->to_double());
                if (!std::isfinite(result)) throw std::out_of_range("Decimal is outside the floating-point field range");
                return result;
            }
            if (const auto* number = std::get_if<Double>(&value)) {
                const auto result = static_cast<T>(*number);
                if (!std::isfinite(result)) throw std::out_of_range("Database value is outside the floating-point field range");
                return result;
            }
            if (const auto* text = std::get_if<String>(&value)) {
                T result{};
                const auto [end,error] = std::from_chars(text->data(),text->data()+text->size(),result);
                if (error == std::errc{} && end == text->data()+text->size() && std::isfinite(result)) return result;
                throw std::invalid_argument("Floating-point cast requires a finite in-range number");
            }
        }
    } else if constexpr (std::same_as<T,Decimal>) {
        if (cast == "decimal") {
            if (const auto* number = std::get_if<Int64>(&value)) return Decimal{std::to_string(*number)};
            if (const auto* number = std::get_if<UInt64>(&value)) return Decimal{std::to_string(*number)};
            if (const auto* number = std::get_if<Double>(&value)) {
                if (!std::isfinite(*number)) throw std::invalid_argument("Decimal cast requires a finite number");
                std::array<char,std::numeric_limits<Double>::max_digits10 + 16> buffer{};
                const auto [end,error] = std::to_chars(buffer.data(),buffer.data()+buffer.size(),*number);
                if (error == std::errc{}) return Decimal{std::string_view{buffer.data(),static_cast<std::size_t>(end-buffer.data())}};
                throw std::out_of_range("Unable to represent database decimal");
            }
        }
    }
    return value_cast<T>(value);
}

} // namespace gungnir::model
