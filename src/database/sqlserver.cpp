#include <gungnir/database/sqlserver.hpp>

#include <sql.h>
#include <sqlext.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <cerrno>
#include <charconv>
#include <cstdlib>
#include <limits>
#include <memory>
#include <stdexcept>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

namespace gungnir::database {

namespace {

struct EnvironmentHandle {
    SQLHENV value{SQL_NULL_HENV};

    ~EnvironmentHandle() {
        if (value != SQL_NULL_HENV) {
            SQLFreeHandle(
                SQL_HANDLE_ENV,
                value
            );
        }
    }

    EnvironmentHandle(
        const EnvironmentHandle&
    ) = delete;

    EnvironmentHandle& operator=(
        const EnvironmentHandle&
    ) = delete;

    EnvironmentHandle() = default;
};

struct ConnectionHandle {
    SQLHDBC value{SQL_NULL_HDBC};

    ~ConnectionHandle() {
        if (value != SQL_NULL_HDBC) {
            SQLDisconnect(value);
            SQLFreeHandle(
                SQL_HANDLE_DBC,
                value
            );
        }
    }

    ConnectionHandle(
        const ConnectionHandle&
    ) = delete;

    ConnectionHandle& operator=(
        const ConnectionHandle&
    ) = delete;

    ConnectionHandle() = default;
};

struct StatementHandle {
    SQLHSTMT value{SQL_NULL_HSTMT};

    ~StatementHandle() {
        if (value != SQL_NULL_HSTMT) {
            SQLFreeHandle(
                SQL_HANDLE_STMT,
                value
            );
        }
    }

    StatementHandle(
        const StatementHandle&
    ) = delete;

    StatementHandle& operator=(
        const StatementHandle&
    ) = delete;

    StatementHandle(
        StatementHandle&& other
    ) noexcept
        : value(
            std::exchange(
                other.value,
                SQL_NULL_HSTMT
            )
          ) {}

    StatementHandle& operator=(
        StatementHandle&& other
    ) noexcept {
        if (this == &other) {
            return *this;
        }

        if (value != SQL_NULL_HSTMT) {
            SQLFreeHandle(
                SQL_HANDLE_STMT,
                value
            );
        }

        value = std::exchange(
            other.value,
            SQL_NULL_HSTMT
        );

        return *this;
    }

    StatementHandle() = default;
};

[[nodiscard]] bool succeeded(
    SQLRETURN result
) noexcept {
    return
        result == SQL_SUCCESS ||
        result == SQL_SUCCESS_WITH_INFO;
}

String diagnostics(
    SQLSMALLINT handle_type,
    SQLHANDLE handle
) {
    String message;
    SQLSMALLINT record = 1;

    while (true) {
        std::array<SQLCHAR, 7> state{};
        SQLINTEGER native_error = 0;
        std::array<SQLCHAR, 1024> text{};
        SQLSMALLINT text_length = 0;

        const auto result =
            SQLGetDiagRecA(
                handle_type,
                handle,
                record,
                state.data(),
                &native_error,
                text.data(),
                static_cast<SQLSMALLINT>(
                    text.size()
                ),
                &text_length
            );

        if (result == SQL_NO_DATA) {
            break;
        }

        if (!succeeded(result)) {
            break;
        }

        if (!message.empty()) {
            message += " | ";
        }

        message +=
            reinterpret_cast<
                const char*
            >(state.data());

        message += ": ";

        const auto length =
            text_length < 0
                ? std::size_t{0}
                : std::min<std::size_t>(
                    static_cast<std::size_t>(
                        text_length
                    ),
                    text.size() - 1
                );

        message.append(
            reinterpret_cast<
                const char*
            >(text.data()),
            length
        );

        ++record;
    }

    return
        message.empty()
            ? String{
                "Unknown SQL Server ODBC error"
              }
            : message;
}

[[noreturn]] void throw_odbc(
    String operation,
    SQLSMALLINT handle_type,
    SQLHANDLE handle
) {
    throw std::runtime_error(
        std::move(operation) +
        ": " +
        diagnostics(
            handle_type,
            handle
        )
    );
}

String braced(
    std::string_view value
) {
    String result{"{"};

    for (const char character : value) {
        result.push_back(character);

        if (character == '}') {
            result.push_back('}');
        }
    }

    result.push_back('}');
    return result;
}

String connection_string(
    const Settings& settings
) {
    String result =
        "Driver=" +
        braced(
            "ODBC Driver 18 for SQL Server"
        ) +
        ";Server=" +
        braced(
            settings.host +
            "," +
            std::to_string(
                settings.port
            )
        ) +
        ";Database=" +
        braced(settings.database) +
        ";UID=" +
        braced(settings.username) +
        ";PWD=" +
        braced(settings.password) +
        ";Encrypt=yes;";

    if (!settings.options.empty()) {
        result += settings.options;

        if (result.back() != ';') {
            result.push_back(';');
        }
    }

    return result;
}

void connect(
    EnvironmentHandle& environment,
    ConnectionHandle& connection,
    const Settings& settings
) {
    if (
        !succeeded(
            SQLAllocHandle(
                SQL_HANDLE_ENV,
                SQL_NULL_HANDLE,
                &environment.value
            )
        )
    ) {
        throw std::runtime_error(
            "Unable to allocate ODBC environment"
        );
    }

    if (
        !succeeded(
            SQLSetEnvAttr(
                environment.value,
                SQL_ATTR_ODBC_VERSION,
                reinterpret_cast<
                    SQLPOINTER
                >(SQL_OV_ODBC3),
                0
            )
        )
    ) {
        throw_odbc(
            "Unable to configure ODBC environment",
            SQL_HANDLE_ENV,
            environment.value
        );
    }

    if (
        !succeeded(
            SQLAllocHandle(
                SQL_HANDLE_DBC,
                environment.value,
                &connection.value
            )
        )
    ) {
        throw_odbc(
            "Unable to allocate SQL Server connection",
            SQL_HANDLE_ENV,
            environment.value
        );
    }

    const auto value =
        connection_string(settings);

    if (
        value.size() >
        static_cast<std::size_t>(
            std::numeric_limits<
                SQLSMALLINT
            >::max()
        )
    ) {
        throw std::length_error(
            "SQL Server connection string is too large"
        );
    }

    const auto result =
        SQLDriverConnectA(
            connection.value,
            nullptr,
            reinterpret_cast<SQLCHAR*>(
                const_cast<char*>(
                    value.c_str()
                )
            ),
            static_cast<SQLSMALLINT>(
                value.size()
            ),
            nullptr,
            0,
            nullptr,
            SQL_DRIVER_NOPROMPT
        );

    if (!succeeded(result)) {
        throw_odbc(
            "SQL Server connection failed",
            SQL_HANDLE_DBC,
            connection.value
        );
    }
}

StatementHandle statement(
    SQLHDBC connection
) {
    StatementHandle result;

    if (
        !succeeded(
            SQLAllocHandle(
                SQL_HANDLE_STMT,
                connection,
                &result.value
            )
        )
    ) {
        throw_odbc(
            "Unable to allocate SQL Server statement",
            SQL_HANDLE_DBC,
            connection
        );
    }

    return result;
}

struct ParameterStorage {
    SQLLEN indicator{0};
    unsigned char boolean_value{0};
    Int64 integer{0};
    Double number{0.0};
    String string;
};

void bind_parameters(
    SQLHSTMT statement_handle,
    const std::vector<
        model::AttributeValue
    >& values,
    std::vector<
        ParameterStorage
    >& storage
) {
    storage.clear();
    storage.resize(values.size());

    for (
        std::size_t index = 0;
        index < values.size();
        ++index
    ) {
        auto& item = storage[index];
        const auto& value = values[index];

        SQLSMALLINT value_type = SQL_C_CHAR;
        SQLSMALLINT parameter_type = SQL_VARCHAR;
        SQLULEN column_size = 1;
        SQLPOINTER pointer = nullptr;
        SQLLEN buffer_length = 0;

        if (
            std::holds_alternative<
                std::monostate
            >(value) ||
            std::holds_alternative<
                std::nullptr_t
            >(value)
        ) {
            item.indicator = SQL_NULL_DATA;
        } else if (
            const auto* boolean =
                std::get_if<Boolean>(
                    &value
                )
        ) {
            item.boolean_value =
                *boolean ? 1 : 0;
            item.indicator =
                static_cast<SQLLEN>(
                    sizeof(
                        item.boolean_value
                    )
                );
            value_type = SQL_C_BIT;
            parameter_type = SQL_BIT;
            pointer =
                &item.boolean_value;
            buffer_length =
                static_cast<SQLLEN>(
                    sizeof(
                        item.boolean_value
                    )
                );
        } else if (
            const auto* integer =
                std::get_if<Int64>(
                    &value
                )
        ) {
            item.integer = *integer;
            item.indicator =
                static_cast<SQLLEN>(
                    sizeof(item.integer)
                );
            value_type = SQL_C_SBIGINT;
            parameter_type = SQL_BIGINT;
            pointer = &item.integer;
            buffer_length =
                static_cast<SQLLEN>(
                    sizeof(item.integer)
                );
        } else if (
            const auto* integer =
                std::get_if<UInt64>(
                    &value
                )
        ) {
            item.string =
                std::to_string(
                    *integer
                );
            item.indicator =
                static_cast<SQLLEN>(
                    item.string.size()
                );
            value_type = SQL_C_CHAR;
            parameter_type = SQL_VARCHAR;
            column_size =
                std::max<SQLULEN>(
                    1,
                    static_cast<SQLULEN>(
                        item.string.size()
                    )
                );
            pointer =
                item.string.data();
            buffer_length =
                static_cast<SQLLEN>(
                    item.string.size()
                );
        } else if (
            const auto* number =
                std::get_if<Double>(
                    &value
                )
        ) {
            item.number = *number;
            item.indicator =
                static_cast<SQLLEN>(
                    sizeof(item.number)
                );
            value_type = SQL_C_DOUBLE;
            parameter_type = SQL_DOUBLE;
            pointer = &item.number;
            buffer_length =
                static_cast<SQLLEN>(
                    sizeof(item.number)
                );
        } else if (
            const auto* string =
                std::get_if<String>(
                    &value
                )
        ) {
            if (
                string->size() >
                static_cast<std::size_t>(
                    std::numeric_limits<
                        SQLLEN
                    >::max()
                )
            ) {
                throw std::length_error(
                    "SQL Server string binding is too large"
                );
            }

            item.string = *string;
            item.indicator =
                static_cast<SQLLEN>(
                    item.string.size()
                );
            value_type = SQL_C_CHAR;
            parameter_type =
                item.string.size() > 8000
                    ? SQL_LONGVARCHAR
                    : SQL_VARCHAR;
            column_size =
                std::max<SQLULEN>(
                    1,
                    static_cast<SQLULEN>(
                        item.string.size()
                    )
                );
            pointer =
                item.string.data();
            buffer_length =
                static_cast<SQLLEN>(
                    item.string.size()
                );
        }

        const auto result =
            SQLBindParameter(
                statement_handle,
                static_cast<
                    SQLUSMALLINT
                >(index + 1),
                SQL_PARAM_INPUT,
                value_type,
                parameter_type,
                column_size,
                0,
                pointer,
                buffer_length,
                &item.indicator
            );

        if (!succeeded(result)) {
            throw_odbc(
                "Unable to bind SQL Server parameter",
                SQL_HANDLE_STMT,
                statement_handle
            );
        }
    }
}

model::AttributeValue read_integer(
    SQLHSTMT statement_handle,
    SQLUSMALLINT column
) {
    Int64 value = 0;
    SQLLEN indicator = 0;

    const auto result =
        SQLGetData(
            statement_handle,
            column,
            SQL_C_SBIGINT,
            &value,
            static_cast<SQLLEN>(
                sizeof(value)
            ),
            &indicator
        );

    if (!succeeded(result)) {
        throw_odbc(
            "Unable to read SQL Server integer",
            SQL_HANDLE_STMT,
            statement_handle
        );
    }

    if (indicator == SQL_NULL_DATA) {
        return nullptr;
    }

    return value;
}

model::AttributeValue read_number(
    SQLHSTMT statement_handle,
    SQLUSMALLINT column
) {
    Double value = 0.0;
    SQLLEN indicator = 0;

    const auto result =
        SQLGetData(
            statement_handle,
            column,
            SQL_C_DOUBLE,
            &value,
            static_cast<SQLLEN>(
                sizeof(value)
            ),
            &indicator
        );

    if (!succeeded(result)) {
        throw_odbc(
            "Unable to read SQL Server number",
            SQL_HANDLE_STMT,
            statement_handle
        );
    }

    if (indicator == SQL_NULL_DATA) {
        return nullptr;
    }

    return value;
}

model::AttributeValue read_boolean(
    SQLHSTMT statement_handle,
    SQLUSMALLINT column
) {
    unsigned char value = 0;
    SQLLEN indicator = 0;

    const auto result =
        SQLGetData(
            statement_handle,
            column,
            SQL_C_BIT,
            &value,
            static_cast<SQLLEN>(
                sizeof(value)
            ),
            &indicator
        );

    if (!succeeded(result)) {
        throw_odbc(
            "Unable to read SQL Server boolean",
            SQL_HANDLE_STMT,
            statement_handle
        );
    }

    if (indicator == SQL_NULL_DATA) {
        return nullptr;
    }

    return Boolean{
        value != 0
    };
}

model::AttributeValue read_string(
    SQLHSTMT statement_handle,
    SQLUSMALLINT column
) {
    String value;

    while (true) {
        std::array<char, 4096> buffer{};
        SQLLEN indicator = 0;

        const auto result =
            SQLGetData(
                statement_handle,
                column,
                SQL_C_CHAR,
                buffer.data(),
                static_cast<SQLLEN>(
                    buffer.size()
                ),
                &indicator
            );

        if (result == SQL_NO_DATA) {
            break;
        }

        if (!succeeded(result)) {
            throw_odbc(
                "Unable to read SQL Server text",
                SQL_HANDLE_STMT,
                statement_handle
            );
        }

        if (indicator == SQL_NULL_DATA) {
            return nullptr;
        }

        std::size_t length = 0;

        while (
            length < buffer.size() &&
            buffer[length] != '\0'
        ) {
            ++length;
        }

        value.append(
            buffer.data(),
            length
        );

        if (result == SQL_SUCCESS) {
            break;
        }
    }

    return value;
}

struct Column {
    String name;
    SQLSMALLINT type{SQL_UNKNOWN_TYPE};
};

std::vector<Column> columns(
    SQLHSTMT statement_handle
) {
    SQLSMALLINT count = 0;

    if (
        !succeeded(
            SQLNumResultCols(
                statement_handle,
                &count
            )
        )
    ) {
        throw_odbc(
            "Unable to inspect SQL Server result",
            SQL_HANDLE_STMT,
            statement_handle
        );
    }

    std::vector<Column> result;
    result.reserve(
        static_cast<std::size_t>(
            count
        )
    );

    for (
        SQLSMALLINT index = 1;
        index <= count;
        ++index
    ) {
        std::array<SQLCHAR, 256> name{};
        SQLSMALLINT name_length = 0;
        SQLSMALLINT type = SQL_UNKNOWN_TYPE;
        SQLULEN column_size = 0;
        SQLSMALLINT decimal_digits = 0;
        SQLSMALLINT nullable = 0;

        const auto status =
            SQLDescribeColA(
                statement_handle,
                static_cast<
                    SQLUSMALLINT
                >(index),
                name.data(),
                static_cast<SQLSMALLINT>(
                    name.size()
                ),
                &name_length,
                &type,
                &column_size,
                &decimal_digits,
                &nullable
            );

        if (!succeeded(status)) {
            throw_odbc(
                "Unable to describe SQL Server result column",
                SQL_HANDLE_STMT,
                statement_handle
            );
        }

        const auto length =
            name_length < 0
                ? std::size_t{0}
                : std::min<std::size_t>(
                    static_cast<std::size_t>(
                        name_length
                    ),
                    name.size() - 1
                );

        result.push_back(
            Column{
                .name = String{
                    reinterpret_cast<
                        const char*
                    >(name.data()),
                    length
                },
                .type = type
            }
        );
    }

    return result;
}

model::AttributeValue value_at(
    SQLHSTMT statement_handle,
    SQLUSMALLINT column,
    SQLSMALLINT type
) {
    switch (type) {
    case SQL_BIT:
        return read_boolean(
            statement_handle,
            column
        );

    case SQL_TINYINT:
    case SQL_SMALLINT:
    case SQL_INTEGER:
    case SQL_BIGINT:
        return read_integer(
            statement_handle,
            column
        );

    case SQL_REAL:
    case SQL_FLOAT:
    case SQL_DOUBLE:
    case SQL_DECIMAL:
    case SQL_NUMERIC:
        return read_number(
            statement_handle,
            column
        );

    default:
        return read_string(
            statement_handle,
            column
        );
    }
}

bool starts_with_insert(
    std::string_view statement
) {
    while (
        !statement.empty() &&
        std::isspace(
            static_cast<unsigned char>(
                statement.front()
            )
        ) != 0
    ) {
        statement.remove_prefix(1);
    }

    constexpr std::string_view insert{
        "insert"
    };

    if (
        statement.size() <
        insert.size()
    ) {
        return false;
    }

    for (
        std::size_t index = 0;
        index < insert.size();
        ++index
    ) {
        if (
            std::tolower(
                static_cast<unsigned char>(
                    statement[index]
                )
            ) != insert[index]
        ) {
            return false;
        }
    }

    return true;
}

Result read_result(
    SQLHSTMT statement_handle,
    std::string_view sql
) {
    Result result;

    SQLLEN affected = 0;

    if (
        succeeded(
            SQLRowCount(
                statement_handle,
                &affected
            )
        ) &&
        affected > 0
    ) {
        result.affected_rows =
            static_cast<std::size_t>(
                affected
            );
    }

    const auto metadata =
        columns(statement_handle);

    if (metadata.empty()) {
        return result;
    }

    while (true) {
        const auto status =
            SQLFetch(
                statement_handle
            );

        if (status == SQL_NO_DATA) {
            break;
        }

        if (!succeeded(status)) {
            throw_odbc(
                "Unable to fetch SQL Server row",
                SQL_HANDLE_STMT,
                statement_handle
            );
        }

        model::AttributeMap row;

        for (
            std::size_t index = 0;
            index < metadata.size();
            ++index
        ) {
            row.emplace(
                metadata[index].name,
                value_at(
                    statement_handle,
                    static_cast<
                        SQLUSMALLINT
                    >(index + 1),
                    metadata[index].type
                )
            );
        }

        result.rows.push_back(
            std::move(row)
        );
    }

    if (
        starts_with_insert(sql) &&
        !result.rows.empty()
    ) {
        const auto found =
            result.rows.front().find("id");

        if (
            found !=
            result.rows.front().end()
        ) {
            result.inserted_id =
                found->second;
        }
    }

    return result;
}

class SQLServerDriver final :
    public Driver {
public:
    explicit SQLServerDriver(
        Settings settings
    )
        : settings_(
            std::move(settings)
          ) {
        connect(
            environment_,
            connection_,
            settings_
        );
    }

    [[nodiscard]]
    Backend backend()
        const noexcept override {
        return Backend::mssql;
    }

    [[nodiscard]]
    bool supports_savepoints()
        const noexcept override {
        return true;
    }

    Result execute(
        const String& sql,
        const std::vector<
            model::AttributeValue
        >& values
    ) override {
        auto prepared =
            statement(
                connection_.value
            );

        if (
            sql.size() >
            static_cast<std::size_t>(
                std::numeric_limits<
                    SQLINTEGER
                >::max()
            )
        ) {
            throw std::length_error(
                "SQL Server statement is too large"
            );
        }

        if (
            !succeeded(
                SQLPrepareA(
                    prepared.value,
                    reinterpret_cast<
                        SQLCHAR*
                    >(
                        const_cast<char*>(
                            sql.c_str()
                        )
                    ),
                    static_cast<SQLINTEGER>(
                        sql.size()
                    )
                )
            )
        ) {
            throw_odbc(
                "Unable to prepare SQL Server statement",
                SQL_HANDLE_STMT,
                prepared.value
            );
        }

        SQLSMALLINT expected_parameters = 0;

        if (
            !succeeded(
                SQLNumParams(
                    prepared.value,
                    &expected_parameters
                )
            )
        ) {
            throw_odbc(
                "Unable to inspect SQL Server parameter count",
                SQL_HANDLE_STMT,
                prepared.value
            );
        }

        if (
            expected_parameters < 0 ||
            static_cast<std::size_t>(
                expected_parameters
            ) != values.size()
        ) {
            throw std::invalid_argument(
                "SQL Server binding count does not match statement parameter count"
            );
        }

        std::vector<
            ParameterStorage
        > storage;

        bind_parameters(
            prepared.value,
            values,
            storage
        );

        if (
            !succeeded(
                SQLExecute(
                    prepared.value
                )
            )
        ) {
            throw_odbc(
                "SQL Server execution failed",
                SQL_HANDLE_STMT,
                prepared.value
            );
        }

        return read_result(
            prepared.value,
            sql
        );
    }

    void begin() override {
        set_autocommit(false);
    }

    void commit() override {
        finish_transaction(
            SQL_COMMIT
        );
    }

    void rollback() override {
        finish_transaction(
            SQL_ROLLBACK
        );
    }

    [[nodiscard]]
    bool ping() override {
        try {
            auto ping_statement =
                statement(
                    connection_.value
                );

            const auto sql =
                String{"SELECT 1"};

            return succeeded(
                SQLExecDirectA(
                    ping_statement.value,
                    reinterpret_cast<
                        SQLCHAR*
                    >(
                        const_cast<char*>(
                            sql.c_str()
                        )
                    ),
                    SQL_NTS
                )
            );
        } catch (...) {
            return false;
        }
    }

private:
    void set_autocommit(
        bool enabled
    ) {
        const auto mode =
            enabled
                ? SQL_AUTOCOMMIT_ON
                : SQL_AUTOCOMMIT_OFF;

        if (
            !succeeded(
                SQLSetConnectAttr(
                    connection_.value,
                    SQL_ATTR_AUTOCOMMIT,
                    reinterpret_cast<
                        SQLPOINTER
                    >(mode),
                    0
                )
            )
        ) {
            throw_odbc(
                "Unable to change SQL Server autocommit mode",
                SQL_HANDLE_DBC,
                connection_.value
            );
        }
    }

    void finish_transaction(
        SQLSMALLINT completion
    ) {
        if (
            !succeeded(
                SQLEndTran(
                    SQL_HANDLE_DBC,
                    connection_.value,
                    completion
                )
            )
        ) {
            throw_odbc(
                "Unable to finish SQL Server transaction",
                SQL_HANDLE_DBC,
                connection_.value
            );
        }

        set_autocommit(true);
    }

    Settings settings_;
    EnvironmentHandle environment_;
    ConnectionHandle connection_;
};

} // namespace

std::shared_ptr<Driver>
make_sqlserver_driver(
    const Settings& settings
) {
    if (
        settings.backend !=
        Backend::mssql
    ) {
        throw std::invalid_argument(
            "SQL Server driver requires mssql settings"
        );
    }

    return std::make_shared<
        SQLServerDriver
    >(settings);
}

DriverRegistry& register_sqlserver(
    DriverRegistry& registry
) {
    return registry.add(
        Backend::mssql,
        [](const Settings& settings) {
            return make_sqlserver_driver(
                settings
            );
        }
    );
}

} // namespace gungnir::database
