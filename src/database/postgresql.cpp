#include <gungnir/database/postgresql.hpp>

#include <libpq-fe.h>

#include <cerrno>
#include <charconv>
#include <cstdlib>
#include <iomanip>
#include <limits>
#include <memory>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string_view>
#include <utility>
#include <vector>

namespace gungnir::database {

namespace {

constexpr Oid bool_oid = 16;
constexpr Oid int8_oid = 20;
constexpr Oid int2_oid = 21;
constexpr Oid int4_oid = 23;
constexpr Oid float4_oid = 700;
constexpr Oid float8_oid = 701;
constexpr Oid numeric_oid = 1700;

struct ConnectionDeleter {
    void operator()(PGconn* connection) const noexcept {
        if (connection != nullptr) {
            PQfinish(connection);
        }
    }
};

struct ResultDeleter {
    void operator()(PGresult* result) const noexcept {
        if (result != nullptr) {
            PQclear(result);
        }
    }
};

using NativeConnection =
    std::unique_ptr<PGconn, ConnectionDeleter>;

using NativeResult =
    std::unique_ptr<PGresult, ResultDeleter>;

String clean_message(const char* value) {
    String message =
        value == nullptr
            ? String{"Unknown PostgreSQL error"}
            : String{value};

    while (
        !message.empty() &&
        (
            message.back() == '\n' ||
            message.back() == '\r'
        )
    ) {
        message.pop_back();
    }

    return message;
}

NativeConnection connect(
    const Settings& settings
) {
    const auto port =
        std::to_string(settings.port);

    std::vector<const char*> keywords;
    std::vector<const char*> values;

    const auto add = [&](
        const char* keyword,
        const String& value
    ) {
        if (value.empty()) {
            return;
        }

        keywords.push_back(keyword);
        values.push_back(value.c_str());
    };

    add("host", settings.host);
    keywords.push_back("port");
    values.push_back(port.c_str());
    add("dbname", settings.database);
    add("user", settings.username);
    add("password", settings.password);
    add("options", settings.options);

    keywords.push_back(nullptr);
    values.push_back(nullptr);

    NativeConnection connection{
        PQconnectdbParams(
            keywords.data(),
            values.data(),
            0
        )
    };

    if (
        !connection ||
        PQstatus(connection.get()) !=
            CONNECTION_OK
    ) {
        throw std::runtime_error(
            "PostgreSQL connection failed: " +
            clean_message(
                connection
                    ? PQerrorMessage(
                        connection.get()
                      )
                    : nullptr
            )
        );
    }

    return connection;
}

String encode_binding(
    const model::AttributeValue& value
) {
    if (const auto* boolean =
            std::get_if<Boolean>(&value)) {
        return *boolean ? "true" : "false";
    }

    if (const auto* integer =
            std::get_if<Int64>(&value)) {
        return std::to_string(*integer);
    }

    if (const auto* integer =
            std::get_if<UInt64>(&value)) {
        return std::to_string(*integer);
    }

    if (const auto* number =
            std::get_if<Double>(&value)) {
        std::ostringstream stream;
        stream
            << std::setprecision(
                std::numeric_limits<
                    Double
                >::max_digits10
            )
            << *number;

        return stream.str();
    }

    if (const auto* string =
            std::get_if<String>(&value)) {
        if (
            string->find('\0') !=
            String::npos
        ) {
            throw std::invalid_argument(
                "PostgreSQL text bindings cannot contain embedded null bytes"
            );
        }

        return *string;
    }

    throw std::logic_error(
        "PostgreSQL null bindings are encoded separately"
    );
}

struct EncodedBindings {
    std::vector<
        std::optional<String>
    > storage;

    std::vector<const char*> values;
};

EncodedBindings encode_bindings(
    const std::vector<
        model::AttributeValue
    >& bindings
) {
    EncodedBindings encoded;
    encoded.storage.reserve(
        bindings.size()
    );

    for (const auto& binding : bindings) {
        if (
            std::holds_alternative<
                std::monostate
            >(binding) ||
            std::holds_alternative<
                std::nullptr_t
            >(binding)
        ) {
            encoded.storage.emplace_back(
                std::nullopt
            );
        } else {
            encoded.storage.emplace_back(
                encode_binding(binding)
            );
        }
    }

    encoded.values.reserve(
        encoded.storage.size()
    );

    for (const auto& value : encoded.storage) {
        encoded.values.push_back(
            value
                ? value->c_str()
                : nullptr
        );
    }

    return encoded;
}

Int64 parse_integer(
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
            "PostgreSQL returned an invalid integer value"
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
            "PostgreSQL returned an invalid numeric value"
        );
    }

    return result;
}

model::AttributeValue decode_value(
    Oid type,
    std::string_view value
) {
    switch (type) {
    case bool_oid:
        return Boolean{
            value == "t" ||
            value == "true" ||
            value == "1"
        };

    case int2_oid:
    case int4_oid:
    case int8_oid:
        return parse_integer(value);

    case float4_oid:
    case float8_oid:
    case numeric_oid:
        return parse_number(value);

    default:
        return String{value};
    }
}

std::size_t affected_rows(
    PGresult* result
) {
    const auto* text =
        PQcmdTuples(result);

    if (
        text == nullptr ||
        *text == '\0'
    ) {
        return 0;
    }

    std::size_t value = 0;
    const auto length =
        std::char_traits<char>::length(
            text
        );

    const auto parsed =
        std::from_chars(
            text,
            text + length,
            value
        );

    if (
        parsed.ec != std::errc{} ||
        parsed.ptr != text + length
    ) {
        return 0;
    }

    return value;
}

Result decode_result(
    PGresult* native
) {
    Result result;
    result.affected_rows =
        affected_rows(native);

    const auto row_count =
        PQntuples(native);

    const auto column_count =
        PQnfields(native);

    result.rows.reserve(
        static_cast<std::size_t>(
            row_count
        )
    );

    for (
        int row_index = 0;
        row_index < row_count;
        ++row_index
    ) {
        model::AttributeMap row;

        for (
            int column_index = 0;
            column_index < column_count;
            ++column_index
        ) {
            const auto* name =
                PQfname(
                    native,
                    column_index
                );

            if (name == nullptr) {
                continue;
            }

            if (
                PQgetisnull(
                    native,
                    row_index,
                    column_index
                ) != 0
            ) {
                row.emplace(
                    name,
                    nullptr
                );
                continue;
            }

            const auto* value =
                PQgetvalue(
                    native,
                    row_index,
                    column_index
                );

            const auto length =
                PQgetlength(
                    native,
                    row_index,
                    column_index
                );

            row.emplace(
                name,
                decode_value(
                    PQftype(
                        native,
                        column_index
                    ),
                    std::string_view{
                        value,
                        static_cast<
                            std::size_t
                        >(length)
                    }
                )
            );
        }

        result.rows.push_back(
            std::move(row)
        );
    }

    const auto* command =
        PQcmdStatus(native);

    if (
        command != nullptr &&
        std::string_view{command}.starts_with(
            "INSERT"
        ) &&
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

class PostgreSQLDriver final :
    public Driver {
public:
    explicit PostgreSQLDriver(
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
        return Backend::postgresql;
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
        >& bindings
    ) override {
        const auto encoded =
            encode_bindings(bindings);

        NativeResult result{
            PQexecParams(
                connection_.get(),
                statement.c_str(),
                static_cast<int>(
                    bindings.size()
                ),
                nullptr,
                encoded.values.empty()
                    ? nullptr
                    : encoded.values.data(),
                nullptr,
                nullptr,
                0
            )
        };

        if (!result) {
            throw std::runtime_error(
                "PostgreSQL execution failed: " +
                clean_message(
                    PQerrorMessage(
                        connection_.get()
                    )
                )
            );
        }

        const auto status =
            PQresultStatus(
                result.get()
            );

        if (
            status !=
                PGRES_COMMAND_OK &&
            status !=
                PGRES_TUPLES_OK
        ) {
            throw std::runtime_error(
                "PostgreSQL execution failed: " +
                clean_message(
                    PQresultErrorMessage(
                        result.get()
                    )
                )
            );
        }

        return decode_result(
            result.get()
        );
    }

    void begin() override {
        execute_control("BEGIN");
    }

    void commit() override {
        execute_control("COMMIT");
    }

    void rollback() override {
        execute_control("ROLLBACK");
    }

    [[nodiscard]]
    bool ping() override {
        if (
            !connection_ ||
            PQstatus(
                connection_.get()
            ) != CONNECTION_OK
        ) {
            return false;
        }

        NativeResult result{
            PQexec(
                connection_.get(),
                "SELECT 1"
            )
        };

        return
            result &&
            PQresultStatus(
                result.get()
            ) ==
                PGRES_TUPLES_OK;
    }

private:
    void execute_control(
        const char* statement
    ) {
        NativeResult result{
            PQexec(
                connection_.get(),
                statement
            )
        };

        if (
            !result ||
            PQresultStatus(
                result.get()
            ) != PGRES_COMMAND_OK
        ) {
            throw std::runtime_error(
                "PostgreSQL transaction command failed: " +
                clean_message(
                    result
                        ? PQresultErrorMessage(
                            result.get()
                          )
                        : PQerrorMessage(
                            connection_.get()
                          )
                )
            );
        }
    }

    Settings settings_;
    NativeConnection connection_;
};

} // namespace

std::shared_ptr<Driver>
make_postgresql_driver(
    const Settings& settings
) {
    if (
        settings.backend !=
        Backend::postgresql
    ) {
        throw std::invalid_argument(
            "PostgreSQL driver requires PostgreSQL settings"
        );
    }

    return std::make_shared<
        PostgreSQLDriver
    >(settings);
}

DriverRegistry& register_postgresql(
    DriverRegistry& registry
) {
    return registry.add(
        Backend::postgresql,
        [](const Settings& settings) {
            return make_postgresql_driver(
                settings
            );
        }
    );
}

} // namespace gungnir::database
