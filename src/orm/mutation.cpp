#include <gungnir/orm/mutation.hpp>

#include <algorithm>
#include <vector>

namespace gungnir::orm {

namespace {

String quote_identifier(database::Backend backend, std::string_view value) {
    if (backend == database::Backend::mysql) {
        return "`" + String{value} + "`";
    }

    if (backend == database::Backend::mssql) {
        return "[" + String{value} + "]";
    }

    return "\"" + String{value} + "\"";
}

String placeholder(database::Backend backend, std::size_t index) {
    if (backend == database::Backend::postgresql) {
        return "$" + std::to_string(index + 1);
    }

    return "?";
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
    result.text = "{\"insert\":\"" + String{table} +
                  "\",\"documents\":[{";

    for (std::size_t index = 0; index < keys.size(); ++index) {
        if (index != 0) {
            result.text += ",";
        }

        result.text += "\"" + keys[index] + "\":{\"$bind\":" +
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

    result.text = "{\"update\":\"" + String{table} +
                  "\",\"updates\":[{\"q\":{\"" + String{key} +
                  "\":{\"$bind\":0}},\"u\":{\"$set\":{";

    result.bindings.push_back(std::move(key_value));

    for (std::size_t index = 0; index < keys.size(); ++index) {
        if (index != 0) {
            result.text += ",";
        }

        result.text += "\"" + keys[index] + "\":{\"$bind\":" +
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
    result.text = "{\"delete\":\"" + String{table} +
                  "\",\"deletes\":[{\"q\":{\"" + String{key} +
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
    if (backend == database::Backend::mongodb) {
        return compile_mongo_insert(table, attributes);
    }

    CompiledQuery result;
    result.kind = CompiledQueryKind::sql;
    const auto keys = sorted_keys(attributes);

    result.text = "INSERT INTO " + quote_identifier(backend, table) + " (";

    for (std::size_t index = 0; index < keys.size(); ++index) {
        if (index != 0) {
            result.text += ", ";
        }
        result.text += quote_identifier(backend, keys[index]);
    }

    result.text += ") VALUES (";

    for (std::size_t index = 0; index < keys.size(); ++index) {
        if (index != 0) {
            result.text += ", ";
        }

        result.text += placeholder(backend, result.bindings.size());
        result.bindings.push_back(attributes.at(keys[index]));
    }

    result.text += ");";
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

    result.text = "UPDATE " + quote_identifier(backend, table) + " SET ";

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
    result.text = "DELETE FROM " + quote_identifier(backend, table) +
                  " WHERE " + quote_identifier(backend, key) + " = " +
                  placeholder(backend, 0) + ";";
    result.bindings.push_back(std::move(key_value));
    return result;
}

} // namespace gungnir::orm
