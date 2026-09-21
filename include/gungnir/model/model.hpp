#pragma once

#include <optional>
#include <span>
#include <string_view>
#include <utility>
#include <vector>

#include <gungnir/core/types.hpp>
#include <gungnir/model/connection.hpp>
#include <gungnir/model/errors.hpp>
#include <gungnir/model/field.hpp>
#include <gungnir/model/fillable.hpp>
#include <gungnir/model/foreign_key.hpp>
#include <gungnir/model/metadata.hpp>
#include <gungnir/model/primary_key.hpp>
#include <gungnir/model/relation.hpp>
#include <gungnir/model/table.hpp>
#include <gungnir/model/value.hpp>

namespace gungnir {

namespace orm {
template <typename ModelType>
class Query;
}

template <typename Derived>
class Model {
public:
    using model_type = Derived;
    using AttributeMap = model::AttributeMap;
    using AttributeValue = model::AttributeValue;

    [[nodiscard]] static orm::Query<Derived> query();

    [[nodiscard]] static orm::Query<Derived> where_(
        String column,
        model::AttributeValue value
    );

    [[nodiscard]] static orm::Query<Derived> with(String relation);

    [[nodiscard]] static constexpr std::string_view table_name() noexcept {
        if constexpr (requires { Derived::table.name(); }) {
            return Derived::table.name();
        }

        return {};
    }

    [[nodiscard]] static constexpr std::string_view connection_name() noexcept {
        if constexpr (requires { Derived::connection.name(); }) {
            return Derived::connection.name();
        }

        return "default";
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

    [[nodiscard]] static std::string_view primary_key_name() {
        std::string_view result;

        model::for_each_attribute<Derived>([&](const auto& descriptor) {
            if (descriptor.kind == model::AttributeKind::primary_key) {
                result = descriptor.name;
            }
        });

        if (result.empty()) {
            throw ModelMetadataError{
                "Model metadata does not define a primary key"
            };
        }

        return result;
    }

    [[nodiscard]] static bool primary_key_incrementing() {
        bool found = false;
        bool incrementing = false;

        model::for_each_attribute<Derived>([&](const auto& descriptor) {
            using Member = std::remove_cvref_t<
                decltype(std::declval<Derived>().*(descriptor.member))
            >;

            if constexpr (
                requires(const Member& member) {
                    { member.incrementing() } -> std::convertible_to<bool>;
                }
            ) {
                if (descriptor.kind == model::AttributeKind::primary_key) {
                    Derived probe;
                    incrementing = (probe.*(descriptor.member)).incrementing();
                    found = true;
                }
            }
        });

        if (!found) {
            throw ModelMetadataError{
                "Model metadata does not define a primary key"
            };
        }

        return incrementing;
    }

    [[nodiscard]] bool exists() const noexcept {
        return exists_;
    }

    [[nodiscard]] bool was_recently_created() const noexcept {
        return recently_created_;
    }

    [[nodiscard]] bool dirty() const {
        bool result = false;

        model::for_each_attribute<Derived>([&](const auto& descriptor) {
            const auto& member = derived().*(descriptor.member);
            result = result || member.dirty();
        });

        return result;
    }

    [[nodiscard]] bool is_dirty(std::string_view attribute) const {
        bool found = false;
        bool result = false;

        model::for_each_attribute<Derived>([&](const auto& descriptor) {
            if (descriptor.name == attribute) {
                found = true;
                result = (derived().*(descriptor.member)).dirty();
            }
        });

        return found && result;
    }

    [[nodiscard]] std::vector<std::string_view> dirty_fields() const {
        std::vector<std::string_view> result;

        model::for_each_attribute<Derived>([&](const auto& descriptor) {
            if ((derived().*(descriptor.member)).dirty()) {
                result.push_back(descriptor.name);
            }
        });

        return result;
    }

    [[nodiscard]] AttributeMap attributes() const {
        AttributeMap result;

        model::for_each_attribute<Derived>([&](const auto& descriptor) {
            const auto& member = derived().*(descriptor.member);

            if (member.initialized()) {
                result.emplace(
                    String{descriptor.name},
                    model::to_value(member.get())
                );
            }
        });

        return result;
    }

    [[nodiscard]] AttributeMap dirty_attributes() const {
        AttributeMap result;

        model::for_each_attribute<Derived>([&](const auto& descriptor) {
            const auto& member = derived().*(descriptor.member);

            if (member.dirty()) {
                result.emplace(
                    String{descriptor.name},
                    model::to_value(member.get())
                );
            }
        });

        return result;
    }

    Derived& fill(const AttributeMap& values) {
        for (const auto& [name, value] : values) {
            if (!is_fillable(name)) {
                throw MassAssignmentError{name};
            }

            if (!assign_attribute(name, value, false)) {
                throw ModelMetadataError{
                    "Fillable attribute '" + name +
                    "' is missing from generated model metadata"
                };
            }
        }

        return derived();
    }

    Derived& force_fill(const AttributeMap& values) {
        for (const auto& [name, value] : values) {
            if (!assign_attribute(name, value, false)) {
                throw ModelMetadataError{
                    "Attribute '" + name +
                    "' is missing from generated model metadata"
                };
            }
        }

        return derived();
    }

    [[nodiscard]] static Derived hydrate(const AttributeMap& values) {
        Derived instance;

        for (const auto& [name, value] : values) {
            instance.assign_attribute(name, value, true);
        }

        instance.mark_persisted(false);
        return instance;
    }

    void clean() {
        model::for_each_attribute<Derived>([&](const auto& descriptor) {
            (derived().*(descriptor.member)).sync_original();
        });
    }

    [[nodiscard]] std::vector<std::string_view> relation_names() const {
        std::vector<std::string_view> result;

        model::for_each_relation<Derived>([&](const auto& descriptor) {
            result.push_back(descriptor.name);
        });

        return result;
    }

    [[nodiscard]] bool relation_loaded(std::string_view name) const {
        bool found = false;
        bool loaded = false;

        model::for_each_relation<Derived>([&](const auto& descriptor) {
            if (descriptor.name == name) {
                found = true;
                loaded = (derived().*(descriptor.member)).loaded();
            }
        });

        return found && loaded;
    }

    void unload_relations() noexcept {
        model::for_each_relation<Derived>([&](const auto& descriptor) {
            (derived().*(descriptor.member)).unload();
        });
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
    [[nodiscard]] Derived& derived() noexcept {
        return static_cast<Derived&>(*this);
    }

    [[nodiscard]] const Derived& derived() const noexcept {
        return static_cast<const Derived&>(*this);
    }

    bool assign_attribute(
        std::string_view name,
        const AttributeValue& value,
        bool hydrated
    ) {
        bool found = false;

        model::for_each_attribute<Derived>([&](const auto& descriptor) {
            if (found || descriptor.name != name) {
                return;
            }

            auto& member = derived().*(descriptor.member);
            using Value = typename std::remove_cvref_t<decltype(member)>::value_type;

            member = model::value_cast<Value>(value);

            if (hydrated) {
                member.sync_original();
            }

            found = true;
        });

        return found;
    }

    bool exists_{false};
    bool recently_created_{false};
};

} // namespace gungnir
