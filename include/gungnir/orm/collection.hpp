#pragma once

#include <cstddef>
#include <stdexcept>
#include <utility>
#include <vector>

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

    void push(Model value) {
        values_.push_back(std::move(value));
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
