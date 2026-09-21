#pragma once

#include <gungnir/migration/column.hpp>
#include <gungnir/migration/definition.hpp>
#include <gungnir/migration/table.hpp>

namespace gungnir {

class Migration {
public:
    virtual ~Migration() = default;

    virtual void up() = 0;
    virtual void down() = 0;

    [[nodiscard]] migration::Plan plan_up();
    [[nodiscard]] migration::Plan plan_down();

protected:
    using Table = migration::Table;
    using Column = migration::Column;
};

} // namespace gungnir
