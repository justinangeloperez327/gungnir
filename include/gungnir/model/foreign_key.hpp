#pragma once

#include <string_view>

#include <gungnir/core/types.hpp>
#include <gungnir/model/field.hpp>

namespace gungnir {

template <typename Related, typename T = Integer>
class ForeignKey : public Field<T> {
public:
    using related_type = Related;
    using value_type = T;
    using Field<T>::operator=;

    constexpr ForeignKey() noexcept = default;

    constexpr explicit ForeignKey(
        std::string_view name,
        std::string_view related_key = "id"
    ) noexcept
        : name_(name), related_key_(related_key) {}

    [[nodiscard]] constexpr std::string_view name() const noexcept {
        return name_;
    }

    [[nodiscard]] constexpr std::string_view related_key() const noexcept {
        return related_key_;
    }

private:
    std::string_view name_;
    std::string_view related_key_{"id"};
};

} // namespace gungnir
