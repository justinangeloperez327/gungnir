#pragma once

#include <concepts>
#include <utility>

namespace gungnir {

template <typename T>
class Field {
public:
    using value_type = T;

    Field() = default;

    Field(const T& value)
        : value_(value),
          original_(value),
          initialized_(true),
          original_initialized_(true) {}

    Field(T&& value)
        : value_(std::move(value)),
          original_(value_),
          initialized_(true),
          original_initialized_(true) {}

    Field& operator=(const T& value) {
        value_ = value;
        initialized_ = true;
        return *this;
    }

    Field& operator=(T&& value) {
        value_ = std::move(value);
        initialized_ = true;
        return *this;
    }

    [[nodiscard]] T& get() noexcept {
        return value_;
    }

    [[nodiscard]] const T& get() const noexcept {
        return value_;
    }

    [[nodiscard]] const T& original() const noexcept {
        return original_;
    }

    [[nodiscard]] bool initialized() const noexcept {
        return initialized_;
    }

    [[nodiscard]] bool original_initialized() const noexcept {
        return original_initialized_;
    }

    [[nodiscard]] bool dirty() const noexcept {
        if (initialized_ != original_initialized_) {
            return true;
        }

        if (!initialized_) {
            return false;
        }

        if constexpr (std::equality_comparable<T>) {
            return value_ != original_;
        }

        return true;
    }

    void sync_original() {
        original_ = value_;
        original_initialized_ = initialized_;
    }

    void reset() {
        if (!original_initialized_) {
            value_ = T{};
            initialized_ = false;
            return;
        }

        value_ = original_;
        initialized_ = true;
    }

    operator T&() noexcept {
        return value_;
    }

    operator const T&() const noexcept {
        return value_;
    }

private:
    T value_{};
    T original_{};
    bool initialized_{false};
    bool original_initialized_{false};
};

} // namespace gungnir
