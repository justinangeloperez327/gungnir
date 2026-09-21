#pragma once

#include <span>
#include <string_view>

#include <gungnir/core/types.hpp>
#include <gungnir/model/field.hpp>
#include <gungnir/model/fillable.hpp>
#include <gungnir/model/foreign_key.hpp>
#include <gungnir/model/primary_key.hpp>
#include <gungnir/model/relation.hpp>
#include <gungnir/model/table.hpp>

namespace gungnir {

template <typename Derived>
class Model {
public:
    using model_type = Derived;

    [[nodiscard]] static constexpr std::string_view table_name() noexcept {
        if constexpr (requires { Derived::table.name(); }) {
            return Derived::table.name();
        }

        return {};
    }

    [[nodiscard]] static constexpr bool is_fillable(
        std::string_view attribute
    ) noexcept {
        if constexpr (requires { Derived::fillable.contains(attribute); }) {
            return Derived::fillable.contains(attribute);
        }

        return false;
    }

    [[nodiscard]] static constexpr std::span<const std::string_view>
    fillable_fields() noexcept {
        if constexpr (requires { Derived::fillable.values(); }) {
            return Derived::fillable.values();
        }

        return {};
    }

    [[nodiscard]] bool exists() const noexcept {
        return exists_;
    }

    [[nodiscard]] bool was_recently_created() const noexcept {
        return recently_created_;
    }

    void mark_persisted(bool newly_created = false) noexcept {
        exists_ = true;
        recently_created_ = newly_created;
    }

    void mark_missing() noexcept {
        exists_ = false;
        recently_created_ = false;
    }

    void clear_recently_created() noexcept {
        recently_created_ = false;
    }

protected:
    Model() = default;
    ~Model() = default;

private:
    bool exists_{false};
    bool recently_created_{false};
};

} // namespace gungnir
