#pragma once

#include <utility>

namespace gungnir {

template <typename T>
class Field {
public:
    using value_type = T;

    Field() = default;
    Field(const T& value) : value_(value) {}
    Field(T&& value) : value_(std::move(value)) {}

    Field& operator=(const T& value) {
        value_ = value;
        return *this;
    }

    Field& operator=(T&& value) {
        value_ = std::move(value);
        return *this;
    }

    [[nodiscard]] T& get() noexcept {
        return value_;
    }

    [[nodiscard]] const T& get() const noexcept {
        return value_;
    }

    operator T&() noexcept {
        return value_;
    }

    operator const T&() const noexcept {
        return value_;
    }

private:
    T value_{};
};

} // namespace gungnir
