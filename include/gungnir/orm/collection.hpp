#pragma once

#include <cstddef>
#include <functional>
#include <optional>
#include <stdexcept>
#include <type_traits>
#include <utility>
#include <vector>

#include <gungnir/core/types.hpp>
#include <gungnir/model/value.hpp>

namespace gungnir::orm {

template <typename Model>
class Collection {
public:
    using value_type = Model;
    using container_type = std::vector<Model>;
    using iterator = typename container_type::iterator;
    using const_iterator = typename container_type::const_iterator;

    Collection() = default;

    explicit Collection(container_type values)
        : values_(std::move(values)) {}

    [[nodiscard]] bool empty() const noexcept {
        return values_.empty();
    }

    [[nodiscard]] std::size_t size() const noexcept {
        return values_.size();
    }

    [[nodiscard]] std::size_t count() const noexcept {
        return values_.size();
    }

    [[nodiscard]] Model& first() {
        if (values_.empty()) {
            throw std::out_of_range("Gungnir ORM collection is empty");
        }
        return values_.front();
    }

    [[nodiscard]] const Model& first() const {
        if (values_.empty()) {
            throw std::out_of_range("Gungnir ORM collection is empty");
        }
        return values_.front();
    }

    [[nodiscard]] Model& last() {
        if (values_.empty()) {
            throw std::out_of_range("Gungnir ORM collection is empty");
        }
        return values_.back();
    }

    [[nodiscard]] const Model& last() const {
        if (values_.empty()) {
            throw std::out_of_range("Gungnir ORM collection is empty");
        }
        return values_.back();
    }

    [[nodiscard]] Model& at(std::size_t index) {
        return values_.at(index);
    }

    [[nodiscard]] const Model& at(std::size_t index) const {
        return values_.at(index);
    }

    [[nodiscard]] Model& operator[](std::size_t index) noexcept {
        return values_[index];
    }

    [[nodiscard]] const Model& operator[](std::size_t index) const noexcept {
        return values_[index];
    }

    [[nodiscard]] std::optional<Model> find(
        const model::AttributeValue& key
    ) const {
        for (const auto& item : values_) {
            const auto item_key = item.primary_key_value();
            if (item_key && *item_key == key) {
                return item;
            }
        }

        return std::nullopt;
    }

    [[nodiscard]] std::vector<model::AttributeValue> pluck(
        String attribute
    ) const {
        std::vector<model::AttributeValue> result;
        result.reserve(values_.size());

        for (const auto& item : values_) {
            if (const auto value = item.attribute_value(attribute)) {
                result.push_back(*value);
            }
        }

        return result;
    }

    template <typename Predicate>
    [[nodiscard]] Collection filter(Predicate&& predicate) const {
        Collection result;

        for (const auto& item : values_) {
            if (std::invoke(predicate, item)) {
                result.push(item);
            }
        }

        return result;
    }

    template <typename Callback>
    void each(Callback&& callback) {
        for (auto& item : values_) {
            std::invoke(callback, item);
        }
    }

    template <typename Callback>
    void each(Callback&& callback) const {
        for (const auto& item : values_) {
            std::invoke(callback, item);
        }
    }

    template <typename Callback>
    [[nodiscard]] auto map(Callback&& callback) const {
        using Result = std::remove_cvref_t<
            std::invoke_result_t<Callback&, const Model&>
        >;

        std::vector<Result> result;
        result.reserve(values_.size());

        for (const auto& item : values_) {
            result.push_back(std::invoke(callback, item));
        }

        return result;
    }

    void push(Model value) {
        values_.push_back(std::move(value));
    }

    void clear() noexcept {
        values_.clear();
    }

    [[nodiscard]] container_type& values() noexcept {
        return values_;
    }

    [[nodiscard]] const container_type& values() const noexcept {
        return values_;
    }

    iterator begin() noexcept { return values_.begin(); }
    iterator end() noexcept { return values_.end(); }
    const_iterator begin() const noexcept { return values_.begin(); }
    const_iterator end() const noexcept { return values_.end(); }

private:
    container_type values_;
};

} // namespace gungnir::orm
