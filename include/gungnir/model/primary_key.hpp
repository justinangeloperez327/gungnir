#pragma once

#include <concepts>
#include <string_view>

#include <gungnir/model/field.hpp>

namespace gungnir {

template <typename T>
class PrimaryKey : public Field<T> {
public:
    using value_type = T;
    using Field<T>::operator=;

    constexpr PrimaryKey() noexcept
        : name_("id"), incrementing_(std::integral<T>) {}

    constexpr explicit PrimaryKey(
        std::string_view name,
        bool incrementing = std::integral<T>
    ) noexcept
        : name_(name), incrementing_(incrementing) {}

    [[nodiscard]] constexpr std::string_view name() const noexcept {
        return name_;
    }

    [[nodiscard]] constexpr bool incrementing() const noexcept {
        return incrementing_;
    }

private:
    std::string_view name_;
    bool incrementing_;
};

} // namespace gungnir
