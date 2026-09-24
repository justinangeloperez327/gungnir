#include <gungnir/database/mongodb.hpp>

#include <mongoc/mongoc.h>

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <limits>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string_view>
#include <utility>
#include <vector>

namespace gungnir::database {

namespace {

struct MongoRuntime {
    MongoRuntime() {
        mongoc_init();
    }

    ~MongoRuntime() {
        mongoc_cleanup();
    }
};

MongoRuntime& mongo_runtime() {
    static MongoRuntime runtime;
    return runtime;
}

struct ClientDeleter {
    void operator()(mongoc_client_t* client)
        const noexcept {
        if (client != nullptr) {
            mongoc_client_destroy(client);
        }
    }
};

struct DatabaseDeleter {
    void operator()(mongoc_database_t* database)
        const noexcept {
        if (database != nullptr) {
            mongoc_database_destroy(database);
        }
    }
};

struct CollectionDeleter {
    void operator()(mongoc_collection_t* collection)
        const noexcept {
        if (collection != nullptr) {
            mongoc_collection_destroy(collection);
        }
    }
};

struct CursorDeleter {
    void operator()(mongoc_cursor_t* cursor)
        const noexcept {
        if (cursor != nullptr) {
            mongoc_cursor_destroy(cursor);
        }
    }
};

struct BsonDeleter {
    void operator()(bson_t* value)
        const noexcept {
        if (value != nullptr) {
            bson_destroy(value);
        }
    }
};

using NativeClient =
    std::unique_ptr<
        mongoc_client_t,
        ClientDeleter
    >;

using NativeDatabase =
    std::unique_ptr<
        mongoc_database_t,
        DatabaseDeleter
    >;

using NativeCollection =
    std::unique_ptr<
        mongoc_collection_t,
        CollectionDeleter
    >;

using NativeCursor =
    std::unique_ptr<
        mongoc_cursor_t,
        CursorDeleter
    >;

using NativeBson =
    std::unique_ptr<
        bson_t,
        BsonDeleter
    >;

bool uri_unreserved(
    unsigned char value
) noexcept {
    return
        std::isalnum(value) != 0 ||
        value == '-' ||
        value == '.' ||
        value == '_' ||
        value == '~';
}

String uri_encode(
    std::string_view value
) {
    static constexpr char hex[] =
        "0123456789ABCDEF";

    String result;
    result.reserve(value.size());

    for (
        const unsigned char character :
        value
    ) {
        if (uri_unreserved(character)) {
            result.push_back(
                static_cast<char>(
                    character
                )
            );
            continue;
        }

        result.push_back('%');
        result.push_back(
            hex[
                (character >> 4U) &
                0x0FU
            ]
        );
        result.push_back(
            hex[
                character &
                0x0FU
            ]
        );
    }

    return result;
}

String uri_for(
    const Settings& settings
) {
    String result{"mongodb://"};

    if (!settings.username.empty()) {
        result += uri_encode(
            settings.username
        );

        if (!settings.password.empty()) {
            result += ":";
            result += uri_encode(
                settings.password
            );
        }

        result += "@";
    }

    if (
        settings.host.find(':') !=
            String::npos &&
        !settings.host.starts_with('[')
    ) {
        result += "[";
        result += settings.host;
        result += "]";
    } else {
        result += settings.host;
    }

    result += ":";
    result += std::to_string(
        settings.port
    );
    result += "/";
    result += uri_encode(
        settings.database
    );

    if (!settings.options.empty()) {
        result += settings.options.front() == '?'
            ? ""
            : "?";
        result += settings.options;
    }

    return result;
}

String bson_error(
    std::string_view operation,
    const bson_error_t& error
) {
    return
        String{operation} +
        ": " +
        error.message;
}

NativeBson parse_json(
    const String& statement
) {
    bson_error_t error{};

    NativeBson result{
        bson_new_from_json(
            reinterpret_cast<
                const std::uint8_t*
            >(
                statement.c_str()
            ),
            -1,
            &error
        )
    };

    if (!result) {
        throw std::runtime_error(
            bson_error(
                "Invalid MongoDB command JSON",
                error
            )
        );
    }

    return result;
}

void append_binding(
    bson_t* target,
    const char* key,
    const model::AttributeValue& value
) {
    if (
        std::holds_alternative<
            std::monostate
        >(value) ||
        std::holds_alternative<
            std::nullptr_t
        >(value)
    ) {
        BSON_APPEND_NULL(
            target,
            key
        );
        return;
    }

    if (
        const auto* boolean =
            std::get_if<Boolean>(
                &value
            )
    ) {
        BSON_APPEND_BOOL(
            target,
            key,
            *boolean
        );
        return;
    }

    if (
        const auto* integer =
            std::get_if<Int64>(
                &value
            )
    ) {
        BSON_APPEND_INT64(
            target,
            key,
            *integer
        );
        return;
    }

    if (
        const auto* integer =
            std::get_if<UInt64>(
                &value
            )
    ) {
        if (
            *integer >
            static_cast<UInt64>(
                std::numeric_limits<
                    Int64
                >::max()
            )
        ) {
            throw std::overflow_error(
                "MongoDB integer binding exceeds BSON int64 range"
            );
        }

        BSON_APPEND_INT64(
            target,
            key,
            static_cast<Int64>(
                *integer
            )
        );
        return;
    }

    if (
        const auto* number =
            std::get_if<Double>(
                &value
            )
    ) {
        BSON_APPEND_DOUBLE(
            target,
            key,
            *number
        );
        return;
    }

    const auto* string =
        std::get_if<String>(
            &value
        );

    if (string == nullptr) {
        throw std::logic_error(
            "Unsupported MongoDB binding type"
        );
    }

    if (
        string->find('\0') !=
        String::npos
    ) {
        throw std::invalid_argument(
            "MongoDB UTF-8 string bindings cannot contain embedded null bytes"
        );
    }

    BSON_APPEND_UTF8(
        target,
        key,
        string->c_str()
    );
}

bool binding_sentinel(
    const bson_t& document,
    std::size_t& index
) {
    bson_iter_t iterator{};

    if (
        !bson_iter_init(
            &iterator,
            &document
        ) ||
        !bson_iter_next(
            &iterator
        ) ||
        String{
            bson_iter_key(
                &iterator
            )
        } != "$bind"
    ) {
        return false;
    }

    std::int64_t raw = -1;

    if (
        BSON_ITER_HOLDS_INT32(
            &iterator
        )
    ) {
        raw =
            bson_iter_int32(
                &iterator
            );
    } else if (
        BSON_ITER_HOLDS_INT64(
            &iterator
        )
    ) {
        raw =
            bson_iter_int64(
                &iterator
            );
    } else {
        return false;
    }

    if (
        raw < 0 ||
        bson_iter_next(
            &iterator
        )
    ) {
        return false;
    }

    index =
        static_cast<std::size_t>(
            raw
        );

    return true;
}

void append_resolved(
    bson_t* target,
    const char* key,
    const bson_iter_t& iterator,
    const std::vector<
        model::AttributeValue
    >& bindings
);

void append_resolved_document(
    bson_t* target,
    const char* key,
    const bson_t& source,
    bool array,
    const std::vector<
        model::AttributeValue
    >& bindings
) {
    if (!array) {
        std::size_t index = 0;

        if (
            binding_sentinel(
                source,
                index
            )
        ) {
            if (
                index >=
                bindings.size()
            ) {
                throw std::out_of_range(
                    "MongoDB command references a missing binding"
                );
            }

            append_binding(
                target,
                key,
                bindings[index]
            );

            return;
        }
    }

    bson_t child;
    bson_init(&child);

    bson_iter_t iterator{};

    if (
        bson_iter_init(
            &iterator,
            &source
        )
    ) {
        while (
            bson_iter_next(
                &iterator
            )
        ) {
            append_resolved(
                &child,
                bson_iter_key(
                    &iterator
                ),
                iterator,
                bindings
            );
        }
    }

    const auto ok =
        array
            ? bson_append_array(
                target,
                key,
                -1,
                &child
              )
            : bson_append_document(
                target,
                key,
                -1,
                &child
              );

    bson_destroy(&child);

    if (!ok) {
        throw std::runtime_error(
            "Unable to construct bound MongoDB command"
        );
    }
}

void append_resolved(
    bson_t* target,
    const char* key,
    const bson_iter_t& iterator,
    const std::vector<
        model::AttributeValue
    >& bindings
) {
    if (
        BSON_ITER_HOLDS_DOCUMENT(
            &iterator
        )
    ) {
        std::uint32_t length = 0;
        const std::uint8_t* data = nullptr;

        bson_iter_document(
            &iterator,
            &length,
            &data
        );

        bson_t document;

        if (
            !bson_init_static(
                &document,
                data,
                length
            )
        ) {
            throw std::runtime_error(
                "Unable to inspect MongoDB command document"
            );
        }

        append_resolved_document(
            target,
            key,
            document,
            false,
            bindings
        );

        return;
    }

    if (
        BSON_ITER_HOLDS_ARRAY(
            &iterator
        )
    ) {
        std::uint32_t length = 0;
        const std::uint8_t* data = nullptr;

        bson_iter_array(
            &iterator,
            &length,
            &data
        );

        bson_t array;

        if (
            !bson_init_static(
                &array,
                data,
                length
            )
        ) {
            throw std::runtime_error(
                "Unable to inspect MongoDB command array"
            );
        }

        append_resolved_document(
            target,
            key,
            array,
            true,
            bindings
        );

        return;
    }

    if (
        !bson_append_value(
            target,
            key,
            -1,
            bson_iter_value(
                &iterator
            )
        )
    ) {
        throw std::runtime_error(
            "Unable to copy MongoDB command value"
        );
    }
}

NativeBson resolve_bindings(
    const bson_t& command,
    const std::vector<
        model::AttributeValue
    >& bindings
) {
    NativeBson result{
        bson_new()
    };

    if (!result) {
        throw std::bad_alloc{};
    }

    bson_iter_t iterator{};

    if (
        bson_iter_init(
            &iterator,
            &command
        )
    ) {
        while (
            bson_iter_next(
                &iterator
            )
        ) {
            append_resolved(
                result.get(),
                bson_iter_key(
                    &iterator
                ),
                iterator,
                bindings
            );
        }
    }

    return result;
}

String command_name(
    const bson_t& command
) {
    bson_iter_t iterator{};

    if (
        !bson_iter_init(
            &iterator,
            &command
        ) ||
        !bson_iter_next(
            &iterator
        )
    ) {
        throw std::invalid_argument(
            "MongoDB command is empty"
        );
    }

    return bson_iter_key(
        &iterator
    );
}

String collection_name(
    const bson_t& command,
    const char* field
) {
    bson_iter_t iterator{};

    if (
        !bson_iter_init_find(
            &iterator,
            &command,
            field
        ) ||
        !BSON_ITER_HOLDS_UTF8(
            &iterator
        )
    ) {
        throw std::invalid_argument(
            "MongoDB command is missing its collection name"
        );
    }

    std::uint32_t length = 0;
    const auto* value =
        bson_iter_utf8(
            &iterator,
            &length
        );

    return String{
        value,
        static_cast<std::size_t>(
            length
        )
    };
}

bool child_document(
    const bson_t& source,
    const char* key,
    bson_t& target,
    bool array = false
) {
    bson_iter_t iterator{};

    if (
        !bson_iter_init_find(
            &iterator,
            &source,
            key
        )
    ) {
        return false;
    }

    std::uint32_t length = 0;
    const std::uint8_t* data = nullptr;

    if (array) {
        if (
            !BSON_ITER_HOLDS_ARRAY(
                &iterator
            )
        ) {
            return false;
        }

        bson_iter_array(
            &iterator,
            &length,
            &data
        );
    } else {
        if (
            !BSON_ITER_HOLDS_DOCUMENT(
                &iterator
            )
        ) {
            return false;
        }

        bson_iter_document(
            &iterator,
            &length,
            &data
        );
    }

    return bson_init_static(
        &target,
        data,
        length
    );
}

String nested_json(
    const bson_iter_t& iterator
) {
    std::uint32_t length = 0;
    const std::uint8_t* data = nullptr;
    bson_t nested;

    if (
        BSON_ITER_HOLDS_DOCUMENT(
            &iterator
        )
    ) {
        bson_iter_document(
            &iterator,
            &length,
            &data
        );
    } else {
        bson_iter_array(
            &iterator,
            &length,
            &data
        );
    }

    if (
        !bson_init_static(
            &nested,
            data,
            length
        )
    ) {
        throw std::runtime_error(
            "Unable to inspect nested BSON value"
        );
    }

    std::size_t json_length = 0;
    char* json =
        bson_as_canonical_extended_json(
            &nested,
            &json_length
        );

    if (json == nullptr) {
        throw std::runtime_error(
            "Unable to serialize nested BSON value"
        );
    }

    String result{
        json,
        json_length
    };

    bson_free(json);
    return result;
}

model::AttributeValue bson_value(
    const bson_iter_t& iterator
) {
    if (
        BSON_ITER_HOLDS_NULL(
            &iterator
        )
    ) {
        return nullptr;
    }

    if (
        BSON_ITER_HOLDS_BOOL(
            &iterator
        )
    ) {
        return Boolean{
            bson_iter_bool(
                &iterator
            )
        };
    }

    if (
        BSON_ITER_HOLDS_INT32(
            &iterator
        )
    ) {
        return Int64{
            bson_iter_int32(
                &iterator
            )
        };
    }

    if (
        BSON_ITER_HOLDS_INT64(
            &iterator
        )
    ) {
        return Int64{
            bson_iter_int64(
                &iterator
            )
        };
    }

    if (
        BSON_ITER_HOLDS_DOUBLE(
            &iterator
        )
    ) {
        return Double{
            bson_iter_double(
                &iterator
            )
        };
    }

    if (
        BSON_ITER_HOLDS_UTF8(
            &iterator
        )
    ) {
        std::uint32_t length = 0;
        const auto* value =
            bson_iter_utf8(
                &iterator,
                &length
            );

        return String{
            value,
            static_cast<std::size_t>(
                length
            )
        };
    }

    if (
        BSON_ITER_HOLDS_OID(
            &iterator
        )
    ) {
        char value[25]{};
        bson_oid_to_string(
            bson_iter_oid(
                &iterator
            ),
            value
        );

        return String{value};
    }

    if (
        BSON_ITER_HOLDS_DATE_TIME(
            &iterator
        )
    ) {
        return Int64{
            bson_iter_date_time(
                &iterator
            )
        };
    }

    if (
        BSON_ITER_HOLDS_DECIMAL128(
            &iterator
        )
    ) {
        bson_decimal128_t value{};
        bson_iter_decimal128(
            &iterator,
            &value
        );

        char text[
            BSON_DECIMAL128_STRING
        ]{};

        bson_decimal128_to_string(
            &value,
            text
        );

        return String{text};
    }

    if (
        BSON_ITER_HOLDS_DOCUMENT(
            &iterator
        ) ||
        BSON_ITER_HOLDS_ARRAY(
            &iterator
        )
    ) {
        return nested_json(
            iterator
        );
    }

    throw std::runtime_error(
        "MongoDB returned a BSON type that Gungnir cannot hydrate yet"
    );
}

model::AttributeMap row_from(
    const bson_t& document
) {
    model::AttributeMap row;
    bson_iter_t iterator{};

    if (
        !bson_iter_init(
            &iterator,
            &document
        )
    ) {
        return row;
    }

    while (
        bson_iter_next(
            &iterator
        )
    ) {
        String key{
            bson_iter_key(
                &iterator
            )
        };

        if (key == "_id") {
            key = "id";
        }

        row.insert_or_assign(
            std::move(key),
            bson_value(
                iterator
            )
        );
    }

    return row;
}

std::size_t numeric_field(
    const bson_t& document,
    const char* field
) {
    bson_iter_t iterator{};

    if (
        !bson_iter_init_find(
            &iterator,
            &document,
            field
        )
    ) {
        return 0;
    }

    std::int64_t value = 0;

    if (
        BSON_ITER_HOLDS_INT32(
            &iterator
        )
    ) {
        value =
            bson_iter_int32(
                &iterator
            );
    } else if (
        BSON_ITER_HOLDS_INT64(
            &iterator
        )
    ) {
        value =
            bson_iter_int64(
                &iterator
            );
    } else if (
        BSON_ITER_HOLDS_DOUBLE(
            &iterator
        )
    ) {
        const auto number =
            bson_iter_double(
                &iterator
            );

        if (number > 0.0) {
            return static_cast<
                std::size_t
            >(number);
        }

        return 0;
    } else {
        return 0;
    }

    if (value <= 0) {
        return 0;
    }

    return static_cast<
        std::size_t
    >(value);
}

std::optional<std::int64_t>
reply_code(
    const bson_t& reply
) {
    bson_iter_t iterator{};

    if (
        !bson_iter_init_find(
            &iterator,
            &reply,
            "code"
        )
    ) {
        return std::nullopt;
    }

    if (
        BSON_ITER_HOLDS_INT32(
            &iterator
        )
    ) {
        return bson_iter_int32(
            &iterator
        );
    }

    if (
        BSON_ITER_HOLDS_INT64(
            &iterator
        )
    ) {
        return bson_iter_int64(
            &iterator
        );
    }

    return std::nullopt;
}

Result cursor_result(
    mongoc_cursor_t* cursor
) {
    Result result;
    const bson_t* document = nullptr;

    while (
        mongoc_cursor_next(
            cursor,
            &document
        )
    ) {
        result.rows.push_back(
            row_from(
                *document
            )
        );
    }

    bson_error_t error{};

    if (
        mongoc_cursor_error(
            cursor,
            &error
        )
    ) {
        throw std::runtime_error(
            bson_error(
                "MongoDB cursor failed",
                error
            )
        );
    }

    result.affected_rows =
        result.rows.size();

    return result;
}

NativeBson command_options(
    const bson_t& command,
    std::initializer_list<
        std::string_view
    > excluded
) {
    NativeBson result{
        bson_new()
    };

    if (!result) {
        throw std::bad_alloc{};
    }

    bson_iter_t iterator{};

    if (
        !bson_iter_init(
            &iterator,
            &command
        )
    ) {
        return result;
    }

    while (
        bson_iter_next(
            &iterator
        )
    ) {
        const auto key =
            std::string_view{
                bson_iter_key(
                    &iterator
                )
            };

        if (
            std::find(
                excluded.begin(),
                excluded.end(),
                key
            ) !=
            excluded.end()
        ) {
            continue;
        }

        if (
            !bson_append_value(
                result.get(),
                key.data(),
                static_cast<int>(
                    key.size()
                ),
                bson_iter_value(
                    &iterator
                )
            )
        ) {
            throw std::runtime_error(
                "Unable to construct MongoDB cursor options"
            );
        }
    }

    return result;
}

class MongoDBDriver final :
    public Driver {
public:
    explicit MongoDBDriver(
        Settings settings
    )
        : settings_(
            std::move(settings)
          ) {
        static_cast<void>(
            mongo_runtime()
        );

        const auto uri =
            uri_for(settings_);

        client_.reset(
            mongoc_client_new(
                uri.c_str()
            )
        );

        if (!client_) {
            throw std::runtime_error(
                "Unable to create MongoDB client from connection settings"
            );
        }

        mongoc_client_set_appname(
            client_.get(),
            "gungnir"
        );

        database_.reset(
            mongoc_client_get_database(
                client_.get(),
                settings_.database.c_str()
            )
        );

        if (!database_) {
            throw std::runtime_error(
                "Unable to open MongoDB database handle"
            );
        }
    }

    [[nodiscard]]
    Backend backend()
        const noexcept override {
        return Backend::mongodb;
    }

    [[nodiscard]]
    bool supports_transactions()
        const noexcept override {
        return false;
    }

    [[nodiscard]]
    bool supports_savepoints()
        const noexcept override {
        return false;
    }

    Result execute(
        const String& statement,
        const std::vector<
            model::AttributeValue
        >& bindings
    ) override {
        const auto parsed =
            parse_json(statement);

        auto command =
            resolve_bindings(
                *parsed,
                bindings
            );

        const auto name =
            command_name(
                *command
            );

        if (name == "find") {
            return execute_find(
                *command
            );
        }

        if (name == "aggregate") {
            return execute_aggregate(
                *command
            );
        }

        std::optional<
            model::AttributeValue
        > inserted_id;

        if (name == "insert") {
            command =
                prepare_insert(
                    *command,
                    inserted_id
                );
        }

        bson_t reply;
        bson_init(&reply);
        bson_error_t error{};

        const auto ok =
            mongoc_database_command_with_opts(
                database_.get(),
                command.get(),
                nullptr,
                nullptr,
                &reply,
                &error
            );

        if (!ok) {
            const auto code =
                reply_code(reply);

            if (
                name == "create" &&
                code &&
                *code == 48
            ) {
                bson_destroy(&reply);
                return Result{};
            }

            const auto message =
                bson_error(
                    "MongoDB command failed",
                    error
                );

            bson_destroy(&reply);
            throw std::runtime_error(
                message
            );
        }

        Result result;
        result.affected_rows =
            std::max(
                numeric_field(
                    reply,
                    "nModified"
                ),
                numeric_field(
                    reply,
                    "n"
                )
            );
        result.inserted_id =
            std::move(
                inserted_id
            );

        bson_destroy(&reply);
        return result;
    }

    void begin() override {
        throw std::logic_error(
            "MongoDB transactions require a replica-set transaction adapter"
        );
    }

    void commit() override {
        throw std::logic_error(
            "MongoDB transaction is not active"
        );
    }

    void rollback() override {
        throw std::logic_error(
            "MongoDB transaction is not active"
        );
    }

    [[nodiscard]]
    bool ping() override {
        bson_t command =
            BSON_INITIALIZER;

        BSON_APPEND_INT32(
            &command,
            "ping",
            1
        );

        bson_t reply;
        bson_init(&reply);
        bson_error_t error{};

        const auto ok =
            mongoc_database_command_with_opts(
                database_.get(),
                &command,
                nullptr,
                nullptr,
                &reply,
                &error
            );

        bson_destroy(&command);
        bson_destroy(&reply);

        return ok;
    }

private:
    Result execute_find(
        const bson_t& command
    ) {
        const auto collection_name_value =
            collection_name(
                command,
                "find"
            );

        NativeCollection collection{
            mongoc_database_get_collection(
                database_.get(),
                collection_name_value.c_str()
            )
        };

        if (!collection) {
            throw std::runtime_error(
                "Unable to open MongoDB collection"
            );
        }

        bson_t empty =
            BSON_INITIALIZER;

        bson_t filter_static;
        const auto has_filter =
            child_document(
                command,
                "filter",
                filter_static
            );

        auto options =
            command_options(
                command,
                {
                    "find",
                    "filter"
                }
            );

        NativeCursor cursor{
            mongoc_collection_find_with_opts(
                collection.get(),
                has_filter
                    ? &filter_static
                    : &empty,
                options.get(),
                nullptr
            )
        };

        bson_destroy(&empty);

        if (!cursor) {
            throw std::runtime_error(
                "Unable to create MongoDB find cursor"
            );
        }

        return cursor_result(
            cursor.get()
        );
    }

    Result execute_aggregate(
        const bson_t& command
    ) {
        const auto collection_name_value =
            collection_name(
                command,
                "aggregate"
            );

        NativeCollection collection{
            mongoc_database_get_collection(
                database_.get(),
                collection_name_value.c_str()
            )
        };

        if (!collection) {
            throw std::runtime_error(
                "Unable to open MongoDB collection"
            );
        }

        bson_t pipeline;

        if (
            !child_document(
                command,
                "pipeline",
                pipeline,
                true
            )
        ) {
            throw std::invalid_argument(
                "MongoDB aggregate command is missing its pipeline"
            );
        }

        auto options =
            command_options(
                command,
                {
                    "aggregate",
                    "pipeline",
                    "cursor"
                }
            );

        NativeCursor cursor{
            mongoc_collection_aggregate(
                collection.get(),
                MONGOC_QUERY_NONE,
                &pipeline,
                options.get(),
                nullptr
            )
        };


        if (!cursor) {
            throw std::runtime_error(
                "Unable to create MongoDB aggregate cursor"
            );
        }

        return cursor_result(
            cursor.get()
        );
    }

    Int64 next_id(
        std::string_view collection
    ) {
        bson_t command =
            BSON_INITIALIZER;

        BSON_APPEND_UTF8(
            &command,
            "findAndModify",
            "gungnir_sequences"
        );

        bson_t query;
        BSON_APPEND_DOCUMENT_BEGIN(
            &command,
            "query",
            &query
        );
        BSON_APPEND_UTF8(
            &query,
            "_id",
            String{collection}.c_str()
        );
        bson_append_document_end(
            &command,
            &query
        );

        bson_t update;
        BSON_APPEND_DOCUMENT_BEGIN(
            &command,
            "update",
            &update
        );

        bson_t increment;
        BSON_APPEND_DOCUMENT_BEGIN(
            &update,
            "$inc",
            &increment
        );
        BSON_APPEND_INT64(
            &increment,
            "value",
            1
        );
        bson_append_document_end(
            &update,
            &increment
        );
        bson_append_document_end(
            &command,
            &update
        );

        BSON_APPEND_BOOL(
            &command,
            "upsert",
            true
        );
        BSON_APPEND_BOOL(
            &command,
            "new",
            true
        );

        bson_t reply;
        bson_init(&reply);
        bson_error_t error{};

        const auto ok =
            mongoc_database_command_with_opts(
                database_.get(),
                &command,
                nullptr,
                nullptr,
                &reply,
                &error
            );

        bson_destroy(&command);

        if (!ok) {
            const auto message =
                bson_error(
                    "Unable to allocate MongoDB sequence id",
                    error
                );

            bson_destroy(&reply);
            throw std::runtime_error(
                message
            );
        }

        bson_t value;

        if (
            !child_document(
                reply,
                "value",
                value
            )
        ) {
            bson_destroy(&reply);
            throw std::runtime_error(
                "MongoDB sequence command returned no value"
            );
        }

        bson_iter_t iterator{};

        if (
            !bson_iter_init_find(
                &iterator,
                &value,
                "value"
            )
        ) {
            bson_destroy(&reply);
            throw std::runtime_error(
                "MongoDB sequence document is missing its counter"
            );
        }

        Int64 result = 0;

        if (
            BSON_ITER_HOLDS_INT32(
                &iterator
            )
        ) {
            result =
                bson_iter_int32(
                    &iterator
                );
        } else if (
            BSON_ITER_HOLDS_INT64(
                &iterator
            )
        ) {
            result =
                bson_iter_int64(
                    &iterator
                );
        } else {
            bson_destroy(&reply);
            throw std::runtime_error(
                "MongoDB sequence counter is not an integer"
            );
        }

        bson_destroy(&reply);
        return result;
    }

    NativeBson prepare_insert(
        const bson_t& command,
        std::optional<
            model::AttributeValue
        >& inserted_id
    ) {
        const auto collection =
            collection_name(
                command,
                "insert"
            );

        NativeBson result{
            bson_new()
        };

        if (!result) {
            throw std::bad_alloc{};
        }

        bson_iter_t iterator{};

        if (
            !bson_iter_init(
                &iterator,
                &command
            )
        ) {
            return result;
        }

        while (
            bson_iter_next(
                &iterator
            )
        ) {
            const auto* key =
                bson_iter_key(
                    &iterator
                );

            if (
                String{key} !=
                    "documents"
            ) {
                if (
                    !bson_append_value(
                        result.get(),
                        key,
                        -1,
                        bson_iter_value(
                            &iterator
                        )
                    )
                ) {
                    throw std::runtime_error(
                        "Unable to construct MongoDB insert command"
                    );
                }

                continue;
            }

            std::uint32_t array_length = 0;
            const std::uint8_t* array_data =
                nullptr;

            bson_iter_array(
                &iterator,
                &array_length,
                &array_data
            );

            bson_t documents;

            if (
                !bson_init_static(
                    &documents,
                    array_data,
                    array_length
                )
            ) {
                throw std::runtime_error(
                    "Unable to inspect MongoDB insert documents"
                );
            }

            bson_t output_array;
            bson_init(&output_array);

            bson_iter_t document_iterator{};
            std::size_t document_index = 0;

            if (
                bson_iter_init(
                    &document_iterator,
                    &documents
                )
            ) {
                while (
                    bson_iter_next(
                        &document_iterator
                    )
                ) {
                    std::uint32_t length = 0;
                    const std::uint8_t* data =
                        nullptr;

                    bson_iter_document(
                        &document_iterator,
                        &length,
                        &data
                    );

                    bson_t source;

                    if (
                        !bson_init_static(
                            &source,
                            data,
                            length
                        )
                    ) {
                        bson_destroy(
                            &output_array
                        );
                        throw std::runtime_error(
                            "Unable to inspect MongoDB insert document"
                        );
                    }

                    bson_t output;
                    bson_init(&output);
                    bson_concat(
                        &output,
                        &source
                    );

                    model::AttributeValue id;

                    bson_iter_t id_iterator{};

                    if (
                        bson_iter_init_find(
                            &id_iterator,
                            &source,
                            "_id"
                        )
                    ) {
                        id = bson_value(
                            id_iterator
                        );
                    } else {
                        const auto generated =
                            next_id(
                                collection
                            );

                        BSON_APPEND_INT64(
                            &output,
                            "_id",
                            generated
                        );

                        id = generated;
                    }

                    if (
                        document_index == 0
                    ) {
                        inserted_id = id;
                    }

                    const auto array_key =
                        std::to_string(
                            document_index
                        );

                    if (
                        !bson_append_document(
                            &output_array,
                            array_key.c_str(),
                            -1,
                            &output
                        )
                    ) {
                        bson_destroy(
                            &output
                        );
                        bson_destroy(
                            &output_array
                        );

                        throw std::runtime_error(
                            "Unable to append MongoDB insert document"
                        );
                    }

                    bson_destroy(
                        &output
                    );

                    ++document_index;
                }
            }

            if (
                !bson_append_array(
                    result.get(),
                    "documents",
                    -1,
                    &output_array
                )
            ) {
                bson_destroy(
                    &output_array
                );

                throw std::runtime_error(
                    "Unable to append MongoDB insert document array"
                );
            }

            bson_destroy(
                &output_array
            );
        }

        return result;
    }

    Settings settings_;
    NativeClient client_;
    NativeDatabase database_;
};

} // namespace

std::shared_ptr<Driver>
make_mongodb_driver(
    const Settings& settings
) {
    if (
        settings.backend !=
        Backend::mongodb
    ) {
        throw std::invalid_argument(
            "MongoDB driver requires MongoDB settings"
        );
    }

    return std::make_shared<
        MongoDBDriver
    >(settings);
}

DriverRegistry& register_mongodb(
    DriverRegistry& registry
) {
    return registry.add(
        Backend::mongodb,
        [](const Settings& settings) {
            return make_mongodb_driver(
                settings
            );
        }
    );
}

} // namespace gungnir::database
