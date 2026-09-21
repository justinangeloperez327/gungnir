#include <gungnir/view/value.hpp>

#include <sstream>
#include <stdexcept>

namespace gungnir::view {

Value Value::array(std::vector<Value> values) {
    Value result;
    result.storage_ = std::make_shared<ArrayStorage>(
        ArrayStorage{std::move(values)}
    );
    return result;
}

Value Value::object(std::unordered_map<String, Value> values) {
    Value result;
    result.storage_ = std::make_shared<ObjectStorage>(
        ObjectStorage{std::move(values)}
    );
    return result;
}

bool Value::is_null() const noexcept {
    return std::holds_alternative<std::nullptr_t>(storage_);
}

bool Value::is_array() const noexcept {
    return std::holds_alternative<std::shared_ptr<ArrayStorage>>(storage_);
}

bool Value::is_object() const noexcept {
    return std::holds_alternative<std::shared_ptr<ObjectStorage>>(storage_);
}

bool Value::truthy() const noexcept {
    if (is_null()) {
        return false;
    }

    if (const auto* value = std::get_if<Boolean>(&storage_)) {
        return *value;
    }

    if (const auto* value = std::get_if<Int64>(&storage_)) {
        return *value != 0;
    }

    if (const auto* value = std::get_if<UInt64>(&storage_)) {
        return *value != 0;
    }

    if (const auto* value = std::get_if<Double>(&storage_)) {
        return *value != 0.0;
    }

    if (const auto* value = std::get_if<String>(&storage_)) {
        return !value->empty();
    }

    if (const auto* value =
            std::get_if<std::shared_ptr<ArrayStorage>>(&storage_)) {
        return *value && !(*value)->values.empty();
    }

    if (const auto* value =
            std::get_if<std::shared_ptr<ObjectStorage>>(&storage_)) {
        return *value && !(*value)->values.empty();
    }

    return false;
}

const std::vector<Value>& Value::as_array() const {
    const auto* value =
        std::get_if<std::shared_ptr<ArrayStorage>>(&storage_);

    if (!value || !*value) {
        throw std::logic_error("Gungnir view value is not an array");
    }

    return (*value)->values;
}

const std::unordered_map<String, Value>& Value::as_object() const {
    const auto* value =
        std::get_if<std::shared_ptr<ObjectStorage>>(&storage_);

    if (!value || !*value) {
        throw std::logic_error("Gungnir view value is not an object");
    }

    return (*value)->values;
}

const Value* Value::get(std::string_view key) const noexcept {
    const auto* value =
        std::get_if<std::shared_ptr<ObjectStorage>>(&storage_);

    if (!value || !*value) {
        return nullptr;
    }

    const auto found = (*value)->values.find(String{key});
    if (found == (*value)->values.end()) {
        return nullptr;
    }

    return &found->second;
}

String Value::string() const {
    if (is_null()) {
        return {};
    }

    if (const auto* value = std::get_if<Boolean>(&storage_)) {
        return *value ? "true" : "false";
    }

    if (const auto* value = std::get_if<Int64>(&storage_)) {
        return std::to_string(*value);
    }

    if (const auto* value = std::get_if<UInt64>(&storage_)) {
        return std::to_string(*value);
    }

    if (const auto* value = std::get_if<Double>(&storage_)) {
        std::ostringstream output;
        output << *value;
        return output.str();
    }

    if (const auto* value = std::get_if<String>(&storage_)) {
        return *value;
    }

    return {};
}

} // namespace gungnir::view
