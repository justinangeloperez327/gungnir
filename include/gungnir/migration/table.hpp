#pragma once

#include <functional>

#include <gungnir/core/types.hpp>
#include <gungnir/migration/column.hpp>

namespace gungnir::migration {

class Table {
public:
    using Definition = std::function<void(Column&)>;

    static void create(String name, Definition definition);
    static void alter(String name, Definition definition);
    static void rename(String from, String to);
    static void drop(String name);
    static void drop_if_exists(String name);
};

} // namespace gungnir::migration
