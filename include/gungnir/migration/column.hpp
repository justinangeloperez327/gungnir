#pragma once

#include <cstddef>
#include <vector>

#include <gungnir/migration/definition.hpp>

namespace gungnir::migration {

class Column {
public:
    explicit Column(std::vector<ColumnDefinition>& columns) noexcept;

    ColumnDefinition& id(String name = "id");
    ColumnDefinition& integer(String name);
    ColumnDefinition& big_integer(String name);
    ColumnDefinition& string(String name, std::size_t length = 255);
    ColumnDefinition& text(String name);
    ColumnDefinition& boolean(String name);
    ColumnDefinition& decimal(String name);
    ColumnDefinition& date_time(String name);
    ColumnDefinition& timestamp(String name);
    ColumnDefinition& foreign_id(String name);

    void timestamps();

private:
    ColumnDefinition& add(
        String name,
        ColumnType type,
        std::size_t length = 0
    );

    std::vector<ColumnDefinition>& columns_;
};

} // namespace gungnir::migration
