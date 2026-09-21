#pragma once

#include <stdexcept>
#include <utility>
#include <vector>

#include <gungnir/orm/orm.hpp>

namespace gungnir {

template <typename Derived>
Derived Model<Derived>::find_or_fail(model::AttributeValue key) {
    return query()
        .where_key(std::move(key))
        .first_or_fail();
}

template <typename Derived>
orm::Collection<Derived> Model<Derived>::find_many(
    std::vector<model::AttributeValue> keys
) {
    if (keys.empty()) {
        return {};
    }

    return query()
        .where_key_in(std::move(keys))
        .get();
}

template <typename Derived>
orm::Collection<Derived> Model<Derived>::create_many(
    const std::vector<AttributeMap>& rows
) {
    orm::Collection<Derived> result;

    for (const auto& row : rows) {
        result.push(create(row));
    }

    return result;
}

template <typename Derived>
Derived Model<Derived>::first_or_create(
    const AttributeMap& search,
    const AttributeMap& values
) {
    auto query_builder = query();

    for (const auto& [name, value] : search) {
        query_builder.where(name, value);
    }

    if (auto found = query_builder.first()) {
        return std::move(*found);
    }

    auto attributes = search;
    for (const auto& [name, value] : values) {
        attributes.insert_or_assign(name, value);
    }

    return create(attributes);
}

template <typename Derived>
Derived Model<Derived>::update_or_create(
    const AttributeMap& search,
    const AttributeMap& values
) {
    auto query_builder = query();

    for (const auto& [name, value] : search) {
        query_builder.where(name, value);
    }

    if (auto found = query_builder.first()) {
        found->update(values);
        return std::move(*found);
    }

    auto attributes = search;
    for (const auto& [name, value] : values) {
        attributes.insert_or_assign(name, value);
    }

    return create(attributes);
}

} // namespace gungnir
