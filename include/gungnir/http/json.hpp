#pragma once

#include <concepts>
#include <cstdint>
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

namespace gungnir::http {

class Json;
struct JsonArrayStorage;
struct JsonObjectStorage;

class Json {
public:
    using Array = std::vector<Json>;
    using Object = std::unordered_map<String, Json>;
    using Storage = std::variant<
        std::nullptr_t,
        Boolean,
        Int64,
        UInt64,
        Double,
        String,
        std::shared_ptr<JsonArrayStorage>,
        std::shared_ptr<JsonObjectStorage>
    >;

    Json() noexcept;
    Json(std::nullptr_t) noexcept;
    Json(Boolean value);
    Json(Int64 value);
    Json(UInt64 value);
    Json(Double value);
    Json(String value);
    Json(std::string_view value);
    Json(const char* value);

    [[nodiscard]] static Json array(Array values);
    [[nodiscard]] static Json object(Object values);
    [[nodiscard]] static Json parse(std::string_view source);

    [[nodiscard]] bool is_null() const noexcept;
    [[nodiscard]] bool is_boolean() const noexcept;
    [[nodiscard]] bool is_integer() const noexcept;
    [[nodiscard]] bool is_number() const noexcept;
    [[nodiscard]] bool is_string() const noexcept;
    [[nodiscard]] bool is_array() const noexcept;
    [[nodiscard]] bool is_object() const noexcept;

    [[nodiscard]] const Array& as_array() const;
    [[nodiscard]] const Object& as_object() const;
    [[nodiscard]] const Json* get(std::string_view key) const noexcept;

    [[nodiscard]] String string() const;
    [[nodiscard]] String dump() const;

private:
    Storage storage_;
};

struct JsonArrayStorage {
    Json::Array values;
};

struct JsonObjectStorage {
    Json::Object values;
};

[[nodiscard]] Json make_json(const Json& value);
[[nodiscard]] Json make_json(Json&& value);
[[nodiscard]] Json make_json(std::nullptr_t);
[[nodiscard]] Json make_json(Boolean value);

template <std::signed_integral T>
requires (!std::same_as<std::remove_cvref_t<T>, Boolean>)
[[nodiscard]] Json make_json(T value) {
    return Json{static_cast<Int64>(value)};
}

template <std::unsigned_integral T>
[[nodiscard]] Json make_json(T value) {
    return Json{static_cast<UInt64>(value)};
}

template <std::floating_point T>
[[nodiscard]] Json make_json(T value) {
    return Json{static_cast<Double>(value)};
}

[[nodiscard]] Json make_json(const String& value);
[[nodiscard]] Json make_json(String&& value);
[[nodiscard]] Json make_json(std::string_view value);
[[nodiscard]] Json make_json(const char* value);
[[nodiscard]] Json make_json(const model::AttributeValue& value);

template <typename T>
[[nodiscard]] Json make_json(const std::optional<T>& value) {
    if (!value) {
        return Json{nullptr};
    }

    return make_json(*value);
}

template <typename T>
concept JsonModel = requires(const T& value) {
    { value.attributes() } -> std::same_as<model::AttributeMap>;
};

template <JsonModel T>
[[nodiscard]] Json make_json(const T& value) {
    Json::Object object;

    for (const auto& [name, attribute] : value.attributes()) {
        object.insert_or_assign(name, make_json(attribute));
    }

    return Json::object(std::move(object));
}

template <typename T>
concept JsonMap = requires(const T& value) {
    typename T::key_type;
    typename T::mapped_type;
    requires std::convertible_to<typename T::key_type, String>;
    value.begin();
    value.end();
};

template <JsonMap T>
[[nodiscard]] Json make_json(const T& value) {
    Json::Object object;

    for (const auto& [key, item] : value) {
        object.insert_or_assign(String{key}, make_json(item));
    }

    return Json::object(std::move(object));
}

template <typename T>
concept JsonRange =
    !JsonModel<T> &&
    !JsonMap<T> &&
    !std::same_as<std::remove_cvref_t<T>, String> &&
    requires(const T& value) {
        value.begin();
        value.end();
    };

template <JsonRange T>
[[nodiscard]] Json make_json(const T& value) {
    Json::Array array;

    for (const auto& item : value) {
        array.push_back(make_json(item));
    }

    return Json::array(std::move(array));
}

} // namespace gungnir::http
