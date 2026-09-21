#pragma once

#include <string_view>
#include <tuple>
#include <type_traits>
#include <utility>

#include <gungnir/model/field.hpp>
#include <gungnir/model/foreign_key.hpp>
#include <gungnir/model/primary_key.hpp>
#include <gungnir/model/relation.hpp>

namespace gungnir::model {

enum class AttributeKind {
    field,
    primary_key,
    foreign_key
};

enum class RelationKind {
    has_one,
    has_many,
    belongs_to,
    belongs_to_many,
    has_one_through,
    has_many_through
};

template <typename>
struct attribute_kind;

template <typename T>
struct attribute_kind<Field<T>> {
    static constexpr AttributeKind value = AttributeKind::field;
};

template <typename T>
struct attribute_kind<PrimaryKey<T>> {
    static constexpr AttributeKind value = AttributeKind::primary_key;
};

template <typename Related, typename T>
struct attribute_kind<ForeignKey<Related, T>> {
    static constexpr AttributeKind value = AttributeKind::foreign_key;
};

template <typename>
struct relation_kind;

template <typename Related>
struct relation_kind<HasOne<Related>> {
    static constexpr RelationKind value = RelationKind::has_one;
};

template <typename Related>
struct relation_kind<HasMany<Related>> {
    static constexpr RelationKind value = RelationKind::has_many;
};

template <typename Related>
struct relation_kind<BelongsTo<Related>> {
    static constexpr RelationKind value = RelationKind::belongs_to;
};

template <typename Related>
struct relation_kind<BelongsToMany<Related>> {
    static constexpr RelationKind value = RelationKind::belongs_to_many;
};

template <typename Related, typename Through>
struct relation_kind<HasOneThrough<Related, Through>> {
    static constexpr RelationKind value = RelationKind::has_one_through;
};

template <typename Related, typename Through>
struct relation_kind<HasManyThrough<Related, Through>> {
    static constexpr RelationKind value = RelationKind::has_many_through;
};

template <typename Owner, typename Member>
struct AttributeDescriptor {
    std::string_view name;
    Member Owner::* member;
    AttributeKind kind;
};

template <typename Owner, typename Member>
[[nodiscard]] constexpr auto attribute(
    std::string_view name,
    Member Owner::* member
) noexcept {
    return AttributeDescriptor<Owner, Member>{
        name,
        member,
        attribute_kind<Member>::value
    };
}

template <typename Owner, typename Member>
struct RelationDescriptor {
    std::string_view name;
    Member Owner::* member;
    RelationKind kind;
};

template <typename Owner, typename Member>
[[nodiscard]] constexpr auto relation(
    std::string_view name,
    Member Owner::* member
) noexcept {
    return RelationDescriptor<Owner, Member>{
        name,
        member,
        relation_kind<Member>::value
    };
}

// This template is the stable hook used by generated Gungnir model metadata.
// Application models do not need to author these tuples manually; the
// generator can specialize this type without changing the model declaration.
template <typename Model>
struct Generated {
    inline static constexpr auto attributes = std::tuple{};
    inline static constexpr auto relations = std::tuple{};
};

template <typename Model, typename Callback>
constexpr void for_each_attribute(Callback&& callback) {
    std::apply(
        [&](const auto&... descriptor) {
            (callback(descriptor), ...);
        },
        Generated<Model>::attributes
    );
}

template <typename Model, typename Callback>
constexpr void for_each_relation(Callback&& callback) {
    std::apply(
        [&](const auto&... descriptor) {
            (callback(descriptor), ...);
        },
        Generated<Model>::relations
    );
}

} // namespace gungnir::model
