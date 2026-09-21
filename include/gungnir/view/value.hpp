#pragma once

#include <concepts>
#include <memory>
#include <optional>
#include <string_view>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <variant>
#include <vector>

#include <gungnir/core/types.hpp>
#include <gungnir/model/value.hpp>

namespace gungnir::view {

struct ArrayStorage;
struct ObjectStorage;

class Value {
public:
    using Storage = std::variant<
        std::nullptr_t,
        Boolean,
        Int64,
        UInt64,
        Double,
        String,
        std::shared_ptr<ArrayStorage>,
        std::shared_ptr<ObjectStorage>
    >;

    Value() noexcept : storage_(nullptr) {}
    Value(std::nullptr_t) noexcept : storage_(nullptr) {}
    Value(Boolean value) : storage_(value) {}
    Value(Int64 value) : storage_(value) {}
    Value(UInt64 value) : storage_(value) {}
    Value(Double value) : storage_(value) {}
    Value(String value) : storage_(std::move(value)) {}
    Value(std::string_view value) : storage_(String{value}) {}
    Value(const char* value) : storage_(String{value ? value : ""}) {}

    [[nodiscard]] static Value array(std::vector<Value> values);
    [[nodiscard]] static Value object(
        std::unordered_map<String, Value> values
    );

    [[nodiscard]] bool is_null() const noexcept;
    [[nodiscard]] bool is_array() const noexcept;
    [[nodiscard]] bool is_object() const noexcept;
    [[nodiscard]] bool truthy() const noexcept;

    [[nodiscard]] const std::vector<Value>& as_array() const;
    [[nodiscard]] const std::unordered_map<String, Value>& as_object() const;

    [[nodiscard]] const Value* get(std::string_view key) const noexcept;
    [[nodiscard]] String string() const;

private:
    Storage storage_;
};

struct ArrayStorage {
    std::vector<Value> values;
};

struct ObjectStorage {
    std::unordered_map<String, Value> values;
};

[[nodiscard]] inline Value make_value(const Value& value) {
    return value;
}

[[nodiscard]] inline Value make_value(Value&& value) {
    return std::move(value);
}

[[nodiscard]] inline Value make_value(std::nullptr_t) {
    return Value{nullptr};
}

[[nodiscard]] inline Value make_value(Boolean value) {
    return Value{value};
}

template <std::signed_integral T>
requires (!std::same_as<std::remove_cvref_t<T>, Boolean>)
[[nodiscard]] inline Value make_value(T value) {
    return Value{static_cast<Int64>(value)};
}

template <std::unsigned_integral T>
[[nodiscard]] inline Value make_value(T value) {
    return Value{static_cast<UInt64>(value)};
}

template <std::floating_point T>
[[nodiscard]] inline Value make_value(T value) {
    return Value{static_cast<Double>(value)};
}

[[nodiscard]] inline Value make_value(const String& value) {
    return Value{value};
}

[[nodiscard]] inline Value make_value(String&& value) {
    return Value{std::move(value)};
}

[[nodiscard]] inline Value make_value(std::string_view value) {
    return Value{value};
}

[[nodiscard]] inline Value make_value(const char* value) {
    return Value{value};
}

[[nodiscard]] inline Value make_value(
    const model::AttributeValue& value
) {
    return std::visit(
        [](const auto& item) -> Value {
            using Item = std::remove_cvref_t<decltype(item)>;

            if constexpr (std::same_as<Item, std::monostate>) {
                return Value{nullptr};
            } else {
                return make_value(item);
            }
        },
        value
    );
}

template <typename T>
[[nodiscard]] Value make_value(const std::optional<T>& value) {
    if (!value) {
        return Value{nullptr};
    }

    return make_value(*value);
}

template <typename T>
concept ModelLike = requires(const T& value) {
    { value.attributes() } -> std::same_as<model::AttributeMap>;
};

template <ModelLike T>
[[nodiscard]] Value make_value(const T& value) {
    std::unordered_map<String, Value> object;

    for (const auto& [name, attribute] : value.attributes()) {
        object.insert_or_assign(name, make_value(attribute));
    }

    return Value::object(std::move(object));
}

template <typename T>
concept ViewRange =
    !ModelLike<T> &&
    !std::same_as<std::remove_cvref_t<T>, String> &&
    !std::same_as<std::remove_cvref_t<T>, model::AttributeMap> &&
    requires(const T& value) {
        value.begin();
        value.end();
    };

template <ViewRange T>
[[nodiscard]] Value make_value(const T& value) {
    std::vector<Value> array;

    for (const auto& item : value) {
        array.push_back(make_value(item));
    }

    return Value::array(std::move(array));
}

} // namespace gungnir::view
