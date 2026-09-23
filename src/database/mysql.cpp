#include <gungnir/database/mysql.hpp>

#include <mysql.h>

#include <cerrno>
#include <charconv>
#include <cstdlib>
#include <limits>
#include <memory>
#include <stdexcept>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

namespace gungnir::database {

namespace {

using BindBoolean =
    std::remove_pointer_t<
        decltype(
            MYSQL_BIND{}.is_null
        )
    >;

struct ConnectionDeleter {
    void operator()(MYSQL* connection)
        const noexcept {
        if (connection != nullptr) {
            mysql_close(connection);
        }
    }
};

struct StatementDeleter {
    void operator()(MYSQL_STMT* statement)
        const noexcept {
        if (statement != nullptr) {
            mysql_stmt_close(statement);
        }
    }
};

struct ResultDeleter {
    void operator()(MYSQL_RES* result)
        const noexcept {
        if (result != nullptr) {
            mysql_free_result(result);
        }
    }
};

using NativeConnection =
    std::unique_ptr<
        MYSQL,
        ConnectionDeleter
    >;

using NativeStatement =
    std::unique_ptr<
        MYSQL_STMT,
        StatementDeleter
    >;

using NativeResult =
    std::unique_ptr<
        MYSQL_RES,
        ResultDeleter
    >;

String connection_error(
    MYSQL* connection
) {
    if (connection == nullptr) {
        return "Unknown MySQL connection error";
    }

    const auto* message =
        mysql_error(connection);

    return
        message == nullptr ||
        *message == '\0'
            ? String{
                "Unknown MySQL connection error"
              }
            : String{message};
}

String statement_error(
    MYSQL_STMT* statement
) {
    if (statement == nullptr) {
        return "Unknown MySQL statement error";
    }

    const auto* message =
        mysql_stmt_error(statement);

    return
        message == nullptr ||
        *message == '\0'
            ? String{
                "Unknown MySQL statement error"
              }
            : String{message};
}

NativeConnection connect(
    const Settings& settings
) {
    NativeConnection connection{
        mysql_init(nullptr)
    };

    if (!connection) {
        throw std::runtime_error(
            "Unable to allocate MySQL connection"
        );
    }

    const char* charset = "utf8mb4";
    mysql_options(
        connection.get(),
        MYSQL_SET_CHARSET_NAME,
        charset
    );

    if (
        mysql_real_connect(
            connection.get(),
            settings.host.empty()
                ? nullptr
                : settings.host.c_str(),
            settings.username.empty()
                ? nullptr
                : settings.username.c_str(),
            settings.password.empty()
                ? nullptr
                : settings.password.c_str(),
            settings.database.empty()
                ? nullptr
                : settings.database.c_str(),
            static_cast<unsigned int>(
                settings.port
            ),
            nullptr,
            0
        ) == nullptr
    ) {
        throw std::runtime_error(
            "MySQL connection failed: " +
            connection_error(
                connection.get()
            )
        );
    }

    if (
        mysql_set_character_set(
            connection.get(),
            "utf8mb4"
        ) != 0
    ) {
        throw std::runtime_error(
            "Unable to configure MySQL utf8mb4 character set: " +
            connection_error(
                connection.get()
            )
        );
    }

    return connection;
}

struct ParameterStorage {
    MYSQL_BIND bind{};
    BindBoolean is_null{};
    signed char boolean_value{};
    Int64 signed_integer{};
    UInt64 unsigned_integer{};
    Double number{};
    String string;
    unsigned long length{};
};

std::vector<MYSQL_BIND>
encode_bindings(
    const std::vector<
        model::AttributeValue
    >& values,
    std::vector<
        ParameterStorage
    >& storage
) {
    storage.clear();
    storage.resize(
        values.size()
    );

    std::vector<MYSQL_BIND> bindings(
        values.size()
    );

    for (
        std::size_t index = 0;
        index < values.size();
        ++index
    ) {
        auto& item =
            storage[index];

        const auto& value =
            values[index];

        if (
            std::holds_alternative<
                std::monostate
            >(value) ||
            std::holds_alternative<
                std::nullptr_t
            >(value)
        ) {
            item.is_null = 1;
            item.bind.buffer_type =
                MYSQL_TYPE_NULL;
            item.bind.is_null =
                &item.is_null;
        } else if (
            const auto* boolean =
                std::get_if<
                    Boolean
                >(&value)
        ) {
            item.boolean_value =
                *boolean ? 1 : 0;
            item.bind.buffer_type =
                MYSQL_TYPE_TINY;
            item.bind.buffer =
                &item.boolean_value;
            item.bind.buffer_length =
                sizeof(
                    item.boolean_value
                );
        } else if (
            const auto* integer =
                std::get_if<
                    Int64
                >(&value)
        ) {
            item.signed_integer =
                *integer;
            item.bind.buffer_type =
                MYSQL_TYPE_LONGLONG;
            item.bind.buffer =
                &item.signed_integer;
            item.bind.buffer_length =
                sizeof(
                    item.signed_integer
                );
            item.bind.is_unsigned = 0;
        } else if (
            const auto* integer =
                std::get_if<
                    UInt64
                >(&value)
        ) {
            item.unsigned_integer =
                *integer;
            item.bind.buffer_type =
                MYSQL_TYPE_LONGLONG;
            item.bind.buffer =
                &item.unsigned_integer;
            item.bind.buffer_length =
                sizeof(
                    item.unsigned_integer
                );
            item.bind.is_unsigned = 1;
        } else if (
            const auto* number =
                std::get_if<
                    Double
                >(&value)
        ) {
            item.number = *number;
            item.bind.buffer_type =
                MYSQL_TYPE_DOUBLE;
            item.bind.buffer =
                &item.number;
            item.bind.buffer_length =
                sizeof(item.number);
        } else if (
            const auto* string =
                std::get_if<
                    String
                >(&value)
        ) {
            if (
                string->size() >
                std::numeric_limits<
                    unsigned long
                >::max()
            ) {
                throw std::length_error(
                    "MySQL string binding is too large"
                );
            }

            item.string = *string;
            item.length =
                static_cast<
                    unsigned long
                >(
                    item.string.size()
                );

            item.bind.buffer_type =
                MYSQL_TYPE_STRING;
            item.bind.buffer =
                item.string.empty()
                    ? nullptr
                    : item.string.data();
            item.bind.buffer_length =
                item.length;
            item.bind.length =
                &item.length;
        }

        bindings[index] =
            item.bind;
    }

    return bindings;
}

Int64 parse_signed(
    std::string_view value
) {
    Int64 result = 0;

    const auto parsed =
        std::from_chars(
            value.data(),
            value.data() + value.size(),
            result
        );

    if (
        parsed.ec != std::errc{} ||
        parsed.ptr !=
            value.data() + value.size()
    ) {
        throw std::runtime_error(
            "MySQL returned an invalid signed integer value"
        );
    }

    return result;
}

UInt64 parse_unsigned(
    std::string_view value
) {
    UInt64 result = 0;

    const auto parsed =
        std::from_chars(
            value.data(),
            value.data() + value.size(),
            result
        );

    if (
        parsed.ec != std::errc{} ||
        parsed.ptr !=
            value.data() + value.size()
    ) {
        throw std::runtime_error(
            "MySQL returned an invalid unsigned integer value"
        );
    }

    return result;
}

Double parse_number(
    std::string_view value
) {
    String text{value};
    char* end = nullptr;
    errno = 0;

    const auto result =
        std::strtod(
            text.c_str(),
            &end
        );

    if (
        errno == ERANGE ||
        end !=
            text.c_str() + text.size()
    ) {
        throw std::runtime_error(
            "MySQL returned an invalid numeric value"
        );
    }

    return result;
}

model::AttributeValue decode_value(
    const MYSQL_FIELD& field,
    std::string_view value
) {
    const bool is_unsigned =
        (
            field.flags &
            UNSIGNED_FLAG
        ) != 0;

    switch (field.type) {
    case MYSQL_TYPE_TINY:
        if (field.length == 1) {
            return Boolean{
                value != "0"
            };
        }

        return
            is_unsigned
                ? model::AttributeValue{
                    parse_unsigned(value)
                  }
                : model::AttributeValue{
                    parse_signed(value)
                  };

    case MYSQL_TYPE_SHORT:
    case MYSQL_TYPE_LONG:
    case MYSQL_TYPE_LONGLONG:
    case MYSQL_TYPE_INT24:
    case MYSQL_TYPE_YEAR:
        return
            is_unsigned
                ? model::AttributeValue{
                    parse_unsigned(value)
                  }
                : model::AttributeValue{
                    parse_signed(value)
                  };

    case MYSQL_TYPE_FLOAT:
    case MYSQL_TYPE_DOUBLE:
    case MYSQL_TYPE_DECIMAL:
    case MYSQL_TYPE_NEWDECIMAL:
        return parse_number(value);

    case MYSQL_TYPE_NULL:
        return nullptr;

    default:
        return String{value};
    }
}

struct ColumnStorage {
    MYSQL_BIND bind{};
    BindBoolean is_null{};
    BindBoolean error{};
    unsigned long length{};
    std::vector<char> buffer;
};

std::size_t checked_size(
    unsigned long value
) {
    if (
        value >
        std::numeric_limits<
            std::size_t
        >::max()
    ) {
        throw std::length_error(
            "MySQL result column is too large"
        );
    }

    return static_cast<
        std::size_t
    >(value);
}

std::size_t affected_rows(
    MYSQL_STMT* statement
) {
    const auto value =
        mysql_stmt_affected_rows(
            statement
        );

    if (
        value ==
        static_cast<my_ulonglong>(
            -1
        )
    ) {
        return 0;
    }

    if (
        value >
        std::numeric_limits<
            std::size_t
        >::max()
    ) {
        throw std::overflow_error(
            "MySQL affected-row count exceeds size_t"
        );
    }

    return static_cast<
        std::size_t
    >(value);
}

Result read_result(
    MYSQL_STMT* statement
) {
    Result result;
    result.affected_rows =
        affected_rows(statement);

    const auto inserted =
        mysql_stmt_insert_id(
            statement
        );

    if (inserted != 0) {
        result.inserted_id =
            static_cast<UInt64>(
                inserted
            );
    }

    NativeResult metadata{
        mysql_stmt_result_metadata(
            statement
        )
    };

    if (!metadata) {
        if (
            mysql_stmt_field_count(
                statement
            ) != 0
        ) {
            throw std::runtime_error(
                "Unable to read MySQL result metadata: " +
                statement_error(
                    statement
                )
            );
        }

        return result;
    }

    BindBoolean update_max_length = 1;

    if (
        mysql_stmt_attr_set(
            statement,
            STMT_ATTR_UPDATE_MAX_LENGTH,
            &update_max_length
        ) != 0
    ) {
        throw std::runtime_error(
            "Unable to configure MySQL result buffering: " +
            statement_error(
                statement
            )
        );
    }

    if (
        mysql_stmt_store_result(
            statement
        ) != 0
    ) {
        throw std::runtime_error(
            "Unable to buffer MySQL result: " +
            statement_error(
                statement
            )
        );
    }

    const auto column_count =
        mysql_num_fields(
            metadata.get()
        );

    auto* fields =
        mysql_fetch_fields(
            metadata.get()
        );

    if (
        column_count != 0 &&
        fields == nullptr
    ) {
        throw std::runtime_error(
            "Unable to inspect MySQL result fields"
        );
    }

    std::vector<ColumnStorage> columns(
        column_count
    );

    std::vector<MYSQL_BIND> bindings(
        column_count
    );

    for (
        unsigned int index = 0;
        index < column_count;
        ++index
    ) {
        auto& column =
            columns[index];

        const auto length =
            checked_size(
                fields[index].max_length
            );

        column.buffer.resize(
            length + 1
        );

        column.bind.buffer_type =
            MYSQL_TYPE_STRING;
        column.bind.buffer =
            column.buffer.data();
        column.bind.buffer_length =
            static_cast<
                unsigned long
            >(
                column.buffer.size()
            );
        column.bind.length =
            &column.length;
        column.bind.is_null =
            &column.is_null;
        column.bind.error =
            &column.error;

        bindings[index] =
            column.bind;
    }

    if (
        column_count != 0 &&
        mysql_stmt_bind_result(
            statement,
            bindings.data()
        ) != 0
    ) {
        throw std::runtime_error(
            "Unable to bind MySQL result columns: " +
            statement_error(
                statement
            )
        );
    }

    while (true) {
        const auto status =
            mysql_stmt_fetch(
                statement
            );

        if (status == MYSQL_NO_DATA) {
            break;
        }

        if (status == MYSQL_DATA_TRUNCATED) {
            throw std::runtime_error(
                "MySQL result exceeded its buffered column length"
            );
        }

        if (status != 0) {
            throw std::runtime_error(
                "Unable to fetch MySQL result row: " +
                statement_error(
                    statement
                )
            );
        }

        model::AttributeMap row;

        for (
            unsigned int index = 0;
            index < column_count;
            ++index
        ) {
            const auto& field =
                fields[index];

            const auto& column =
                columns[index];

            const auto* name =
                field.name;

            if (name == nullptr) {
                continue;
            }

            if (column.is_null != 0) {
                row.emplace(
                    name,
                    nullptr
                );
                continue;
            }

            const auto size =
                checked_size(
                    column.length
                );

            row.emplace(
                name,
                decode_value(
                    field,
                    std::string_view{
                        column.buffer.data(),
                        size
                    }
                )
            );
        }

        result.rows.push_back(
            std::move(row)
        );
    }

    return result;
}

class MySQLDriver final :
    public Driver {
public:
    explicit MySQLDriver(
        Settings settings
    )
        : settings_(
            std::move(settings)
          ),
          connection_(
            connect(settings_)
          ) {}

    [[nodiscard]]
    Backend backend()
        const noexcept override {
        return Backend::mysql;
    }

    [[nodiscard]]
    bool supports_savepoints()
        const noexcept override {
        return true;
    }

    Result execute(
        const String& statement,
        const std::vector<
            model::AttributeValue
        >& values
    ) override {
        NativeStatement prepared{
            mysql_stmt_init(
                connection_.get()
            )
        };

        if (!prepared) {
            throw std::runtime_error(
                "Unable to allocate MySQL prepared statement: " +
                connection_error(
                    connection_.get()
                )
            );
        }

        if (
            statement.size() >
            std::numeric_limits<
                unsigned long
            >::max()
        ) {
            throw std::length_error(
                "MySQL statement is too large"
            );
        }

        if (
            mysql_stmt_prepare(
                prepared.get(),
                statement.data(),
                static_cast<
                    unsigned long
                >(
                    statement.size()
                )
            ) != 0
        ) {
            throw std::runtime_error(
                "Unable to prepare MySQL statement: " +
                statement_error(
                    prepared.get()
                )
            );
        }

        const auto expected =
            mysql_stmt_param_count(
                prepared.get()
            );

        if (
            expected !=
            values.size()
        ) {
            throw std::invalid_argument(
                "MySQL binding count does not match statement parameter count"
            );
        }

        std::vector<
            ParameterStorage
        > storage;

        auto bindings =
            encode_bindings(
                values,
                storage
            );

        if (
            !bindings.empty() &&
            mysql_stmt_bind_param(
                prepared.get(),
                bindings.data()
            ) != 0
        ) {
            throw std::runtime_error(
                "Unable to bind MySQL statement parameters: " +
                statement_error(
                    prepared.get()
                )
            );
        }

        if (
            mysql_stmt_execute(
                prepared.get()
            ) != 0
        ) {
            throw std::runtime_error(
                "MySQL execution failed: " +
                statement_error(
                    prepared.get()
                )
            );
        }

        return read_result(
            prepared.get()
        );
    }

    void begin() override {
        execute_control(
            "START TRANSACTION"
        );
    }

    void commit() override {
        execute_control("COMMIT");
    }

    void rollback() override {
        execute_control("ROLLBACK");
    }

    [[nodiscard]]
    bool ping() override {
        return
            connection_ &&
            mysql_ping(
                connection_.get()
            ) == 0;
    }

private:
    void execute_control(
        std::string_view statement
    ) {
        if (
            mysql_real_query(
                connection_.get(),
                statement.data(),
                static_cast<
                    unsigned long
                >(
                    statement.size()
                )
            ) != 0
        ) {
            throw std::runtime_error(
                "MySQL transaction command failed: " +
                connection_error(
                    connection_.get()
                )
            );
        }
    }

    Settings settings_;
    NativeConnection connection_;
};

} // namespace

std::shared_ptr<Driver>
make_mysql_driver(
    const Settings& settings
) {
    if (
        settings.backend !=
        Backend::mysql
    ) {
        throw std::invalid_argument(
            "MySQL driver requires MySQL settings"
        );
    }

    return std::make_shared<
        MySQLDriver
    >(settings);
}

DriverRegistry& register_mysql(
    DriverRegistry& registry
) {
    return registry.add(
        Backend::mysql,
        [](const Settings& settings) {
            return make_mysql_driver(
                settings
            );
        }
    );
}

} // namespace gungnir::database
