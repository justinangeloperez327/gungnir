#include <gungnir/orm/mutation.hpp>

#include <gungnir/orm/identifier.hpp>
#include <algorithm>
#include <vector>

namespace gungnir::orm {

namespace {

String quote_identifier(database::Backend backend, std::string_view value) {
    return detail::quote_identifier(backend, value);
}

String placeholder(database::Backend backend, std::size_t index) {
    if (backend == database::Backend::postgresql) {
        return "$" + std::to_string(index + 1);
    }

    return "?";
}

String mongo_field(std::string_view value) {
    return value == "id"
        ? String{"_id"}
        : detail::json_field_identifier(value);
}

std::vector<String> sorted_keys(const model::AttributeMap& attributes) {
    std::vector<String> keys;
    keys.reserve(attributes.size());

    for (const auto& [key, value] : attributes) {
        static_cast<void>(value);
        keys.push_back(key);
    }

    std::sort(keys.begin(), keys.end());
    return keys;
}

CompiledQuery compile_mongo_insert(
    std::string_view table,
    const model::AttributeMap& attributes
) {
    CompiledQuery result;
    result.kind = CompiledQueryKind::mongodb;

    const auto keys = sorted_keys(attributes);
    result.text = "{\"insert\":\"" + detail::json_identifier(table) +
                  "\",\"documents\":[{";

    for (std::size_t index = 0; index < keys.size(); ++index) {
        if (index != 0) {
            result.text += ",";
        }

        result.text += "\"" + mongo_field(keys[index]) + "\":{\"$bind\":" +
                       std::to_string(result.bindings.size()) + "}";
        result.bindings.push_back(attributes.at(keys[index]));
    }

    result.text += "}]}";
    return result;
}

CompiledQuery compile_mongo_update(
    std::string_view table,
    const model::AttributeMap& attributes,
    std::string_view key,
    model::AttributeValue key_value
) {
    CompiledQuery result;
    result.kind = CompiledQueryKind::mongodb;
    const auto keys = sorted_keys(attributes);

    result.text = "{\"update\":\"" + detail::json_identifier(table) +
                  "\",\"updates\":[{\"q\":{\"" + mongo_field(key) +
                  "\":{\"$bind\":0}},\"u\":{\"$set\":{";

    result.bindings.push_back(std::move(key_value));

    for (std::size_t index = 0; index < keys.size(); ++index) {
        if (index != 0) {
            result.text += ",";
        }

        result.text += "\"" + mongo_field(keys[index]) + "\":{\"$bind\":" +
                       std::to_string(result.bindings.size()) + "}";
        result.bindings.push_back(attributes.at(keys[index]));
    }

    result.text += "}},\"multi\":false}]}";
    return result;
}

CompiledQuery compile_mongo_delete(
    std::string_view table,
    std::string_view key,
    model::AttributeValue key_value
) {
    CompiledQuery result;
    result.kind = CompiledQueryKind::mongodb;
    result.text = "{\"delete\":\"" + detail::json_identifier(table) +
                  "\",\"deletes\":[{\"q\":{\"" + mongo_field(key) +
                  "\":{\"$bind\":0}},\"limit\":1}]}";
    result.bindings.push_back(std::move(key_value));
    return result;
}

} // namespace

CompiledQuery compile_insert(
    std::string_view table,
    const model::AttributeMap& attributes,
    database::Backend backend
) {
    return compile_insert(table, attributes, backend, {});
}

CompiledQuery compile_insert(
    std::string_view table,
    const model::AttributeMap& attributes,
    database::Backend backend,
    std::string_view generated_key
) {
    if (backend == database::Backend::mongodb) {
        return compile_mongo_insert(table, attributes);
    }

    CompiledQuery result;
    result.kind = CompiledQueryKind::sql;
    const auto keys = sorted_keys(attributes);

    if (keys.empty()) {
        result.text = "INSERT INTO " + detail::quote_path(backend, table);
        if (backend == database::Backend::mssql && !generated_key.empty())
            result.text += " OUTPUT INSERTED." + quote_identifier(backend, generated_key) + " AS [id]";
        result.text += backend == database::Backend::mysql ? " () VALUES ()" : " DEFAULT VALUES";
        if (backend == database::Backend::postgresql && !generated_key.empty())
            result.text += " RETURNING " + quote_identifier(backend, generated_key) + " AS \"id\"";
        result.text += ";";
        return result;
    }

    result.text = "INSERT INTO " + detail::quote_path(backend, table) + " (";

    for (std::size_t index = 0; index < keys.size(); ++index) {
        if (index != 0) {
            result.text += ", ";
        }
        result.text += quote_identifier(backend, keys[index]);
    }

    result.text += ")";
    if (backend == database::Backend::mssql && !generated_key.empty()) {
        result.text += " OUTPUT INSERTED." + quote_identifier(backend, generated_key) + " AS [id]";
    }
    result.text += " VALUES (";

    for (std::size_t index = 0; index < keys.size(); ++index) {
        if (index != 0) {
            result.text += ", ";
        }

        result.text += placeholder(backend, result.bindings.size());
        result.bindings.push_back(attributes.at(keys[index]));
    }

    result.text += ")";
    if (backend == database::Backend::postgresql && !generated_key.empty()) {
        result.text += " RETURNING " + quote_identifier(backend, generated_key) + " AS \"id\"";
    }
    result.text += ";";
    return result;
}

CompiledQuery compile_update(
    std::string_view table,
    const model::AttributeMap& attributes,
    std::string_view key,
    model::AttributeValue key_value,
    database::Backend backend
) {
    if (backend == database::Backend::mongodb) {
        return compile_mongo_update(
            table,
            attributes,
            key,
            std::move(key_value)
        );
    }

    CompiledQuery result;
    result.kind = CompiledQueryKind::sql;
    const auto keys = sorted_keys(attributes);

    result.text = "UPDATE " + detail::quote_path(backend, table) + " SET ";

    for (std::size_t index = 0; index < keys.size(); ++index) {
        if (index != 0) {
            result.text += ", ";
        }

        result.text += quote_identifier(backend, keys[index]) + " = " +
                       placeholder(backend, result.bindings.size());
        result.bindings.push_back(attributes.at(keys[index]));
    }

    result.text += " WHERE " + quote_identifier(backend, key) + " = " +
                   placeholder(backend, result.bindings.size()) + ";";
    result.bindings.push_back(std::move(key_value));

    return result;
}

CompiledQuery compile_delete(
    std::string_view table,
    std::string_view key,
    model::AttributeValue key_value,
    database::Backend backend
) {
    if (backend == database::Backend::mongodb) {
        return compile_mongo_delete(
            table,
            key,
            std::move(key_value)
        );
    }

    CompiledQuery result;
    result.kind = CompiledQueryKind::sql;
    result.text = "DELETE FROM " + detail::quote_path(backend, table) +
                  " WHERE " + quote_identifier(backend, key) + " = " +
                  placeholder(backend, 0) + ";";
    result.bindings.push_back(std::move(key_value));
    return result;
}

} // namespace gungnir::orm
