#pragma once

#include <initializer_list>
#include <unordered_map>
#include <utility>

#include <gungnir/core/types.hpp>
#include <gungnir/view/value.hpp>

namespace gungnir::view {

struct Entry {
    String key;
    Value value;

    template <typename T>
    Entry(String name, T&& item)
        : key(std::move(name)),
          value(make_value(std::forward<T>(item))) {}
};

class Data {
public:
    Data() = default;

    Data(std::initializer_list<Entry> entries) {
        for (const auto& entry : entries) {
            values_.insert_or_assign(entry.key, entry.value);
        }
    }

    [[nodiscard]] const Value* get(std::string_view key) const noexcept {
        const auto found = values_.find(String{key});
        if (found == values_.end()) {
            return nullptr;
        }

        return &found->second;
    }

    [[nodiscard]] const std::unordered_map<String, Value>& values() const noexcept {
        return values_;
    }

private:
    std::unordered_map<String, Value> values_;
};

} // namespace gungnir::view
