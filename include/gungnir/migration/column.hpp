#pragma once

#include <cstddef>
#include <initializer_list>
#include <vector>

#include <gungnir/migration/definition.hpp>

namespace gungnir::migration {

class Column {
public:
    explicit Column(TableOperation& operation) noexcept;

    ColumnDefinition& id(String name = "id");
    ColumnDefinition& tiny_integer(String name);
    ColumnDefinition& small_integer(String name);
    ColumnDefinition& medium_integer(String name);
    ColumnDefinition& integer(String name);
    ColumnDefinition& big_integer(String name);
    ColumnDefinition& floating(String name);
    ColumnDefinition& double_precision(String name);
    ColumnDefinition& decimal(
        String name,
        std::size_t precision = 10,
        std::size_t scale = 2
    );
    ColumnDefinition& boolean(String name);
    ColumnDefinition& fixed_string(String name, std::size_t length = 255);
    ColumnDefinition& string(String name, std::size_t length = 255);
    ColumnDefinition& text(String name);
    ColumnDefinition& medium_text(String name);
    ColumnDefinition& long_text(String name);
    ColumnDefinition& binary(String name);
    ColumnDefinition& json(String name);
    ColumnDefinition& uuid(String name);
    ColumnDefinition& date(String name);
    ColumnDefinition& time(String name);
    ColumnDefinition& date_time(String name);
    ColumnDefinition& timestamp(String name);
    ColumnDefinition& timestamp_tz(String name);
    ColumnDefinition& enumeration(
        String name,
        std::initializer_list<String> values
    );
    ColumnDefinition& foreign_id(String name);

    void timestamps();
    void timestamps_tz();
    ColumnDefinition& soft_deletes(String name = "deleted_at");
    ColumnDefinition& soft_deletes_tz(String name = "deleted_at");

    IndexDefinition& primary(
        std::initializer_list<String> columns,
        String name = {}
    );
    IndexDefinition& unique(
        std::initializer_list<String> columns,
        String name = {}
    );
    IndexDefinition& index(
        std::initializer_list<String> columns,
        String name = {}
    );

    ForeignKeyDefinition& foreign(
        String column,
        String name = {}
    );
    ForeignKeyDefinition& foreign(
        std::initializer_list<String> columns,
        String name = {}
    );

    void drop(String name);
    void drop(std::initializer_list<String> names);
    void rename(String from, String to);

    void drop_primary(String name = {});
    void drop_unique(String name);
    void drop_index(String name);
    void rename_index(String from, String to);
    void drop_foreign(String name);
    void drop_foreign(std::initializer_list<String> columns);

    void drop_timestamps();
    void drop_soft_deletes(String name = "deleted_at");

private:
    ColumnDefinition& add(
        String name,
        ColumnType type,
        std::size_t length = 0,
        std::size_t precision = 0,
        std::size_t scale = 0
    );

    IndexDefinition& add_index(
        IndexType type,
        std::initializer_list<String> columns,
        String name
    );

    void add_command(
        AlterCommandType type,
        std::vector<String> columns = {},
        String name = {},
        String new_name = {}
    );

    TableOperation& operation_;
};

} // namespace gungnir::migration
