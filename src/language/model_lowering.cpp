#include <gungnir/language/model_lowering.hpp>

#include <algorithm>
#include <cctype>
#include <optional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include <gungnir/language/ast.hpp>
#include <gungnir/language/lexer.hpp>

namespace gungnir::language {

namespace {

struct FieldInfo {
    SourceSpan type_span;
    std::string name;
    std::string column;
    std::string cpp_type;
    bool nullable{false};
    bool primary_key{false};
    std::optional<std::string> related_type;
};

struct RelationInfo {
    SourceSpan method_span;
    std::string name;
    std::string backing_name;
    std::string cpp_type;
    std::vector<std::string> constructor_args;
    std::optional<std::string> belongs_to_type;
    std::string belongs_to_foreign_key;
};

struct ModelInfo {
    std::string name;
    std::string table;
    std::string connection{"default"};
    bool soft_deletes{false};
    bool timestamps{true};
    std::size_t body_open_end{0};
    std::size_t body_close_offset{0};
    std::size_t metadata_offset{0};
    bool needs_semicolon{false};
    std::vector<FieldInfo> fields;
    std::vector<RelationInfo> relations;
};

std::optional<std::size_t> next_significant(
    const std::vector<Token>& tokens,
    std::size_t index
) {
    for (auto cursor = index + 1; cursor < tokens.size(); ++cursor) {
        if (!tokens[cursor].trivia() && tokens[cursor].kind != TokenKind::end) {
            return cursor;
        }
    }

    return std::nullopt;
}

std::optional<std::size_t> previous_significant(
    const std::vector<Token>& tokens,
    std::size_t index
) {
    if (index == 0) {
        return std::nullopt;
    }

    auto cursor = index;
    while (cursor > 0) {
        --cursor;
        if (!tokens[cursor].trivia() && tokens[cursor].kind != TokenKind::end) {
            return cursor;
        }
    }

    return std::nullopt;
}

std::optional<std::size_t> matching_symbol(
    const std::vector<Token>& tokens,
    std::size_t opening,
    std::string_view open,
    std::string_view close
) {
    std::size_t depth = 0;

    for (auto index = opening; index < tokens.size(); ++index) {
        if (tokens[index].trivia()) {
            continue;
        }

        if (tokens[index].lexeme == open) {
            ++depth;
            continue;
        }

        if (tokens[index].lexeme == close) {
            if (depth == 0) {
                return std::nullopt;
            }

            --depth;
            if (depth == 0) {
                return index;
            }
        }
    }

    return std::nullopt;
}

std::string snake_case(std::string_view value) {
    std::string result;

    for (std::size_t index = 0; index < value.size(); ++index) {
        const char current = value[index];

        if (
            std::isupper(static_cast<unsigned char>(current)) != 0 &&
            !result.empty() &&
            result.back() != '_'
        ) {
            const bool previous_lower =
                std::islower(static_cast<unsigned char>(value[index - 1])) != 0 ||
                std::isdigit(static_cast<unsigned char>(value[index - 1])) != 0;
            const bool next_lower =
                index + 1 < value.size() &&
                std::islower(static_cast<unsigned char>(value[index + 1])) != 0;

            if (previous_lower || next_lower) {
                result.push_back('_');
            }
        }

        if (current == '-') {
            result.push_back('_');
        } else {
            result.push_back(
                static_cast<char>(
                    std::tolower(static_cast<unsigned char>(current))
                )
            );
        }
    }

    return result;
}

std::string pluralize(std::string value) {
    if (value.empty()) {
        return value;
    }

    const auto ends_with = [&](std::string_view suffix) {
        return value.size() >= suffix.size() &&
               value.compare(
                   value.size() - suffix.size(),
                   suffix.size(),
                   suffix
               ) == 0;
    };

    if (
        value.size() > 1 &&
        value.back() == 'y' &&
        value[value.size() - 2] != 'a' &&
        value[value.size() - 2] != 'e' &&
        value[value.size() - 2] != 'i' &&
        value[value.size() - 2] != 'o' &&
        value[value.size() - 2] != 'u'
    ) {
        value.pop_back();
        return value + "ies";
    }

    if (
        ends_with("s") ||
        ends_with("x") ||
        ends_with("z") ||
        ends_with("ch") ||
        ends_with("sh")
    ) {
        return value + "es";
    }

    return value + "s";
}

std::string unquote(std::string_view value) {
    if (
        value.size() >= 2 &&
        ((value.front() == '"' && value.back() == '"') ||
         (value.front() == '\'' && value.back() == '\''))
    ) {
        return std::string{value.substr(1, value.size() - 2)};
    }

    return std::string{value};
}

std::optional<std::string> cpp_scalar(std::string_view type) {
    if (type == "string") {
        return "gungnir::String";
    }
    if (type == "int" || type == "integer") {
        return "gungnir::Integer";
    }
    if (type == "int64") {
        return "gungnir::Int64";
    }
    if (type == "uint64") {
        return "gungnir::UInt64";
    }
    if (type == "bool" || type == "boolean") {
        return "gungnir::Boolean";
    }
    if (type == "float") {
        return "gungnir::Float";
    }
    if (type == "double") {
        return "gungnir::Double";
    }

    return std::nullopt;
}

std::string quoted(std::string_view value) {
    return "\"" + std::string{value} + "\"";
}

std::string join_quoted(const std::vector<std::string>& values) {
    std::string result;

    for (std::size_t index = 0; index < values.size(); ++index) {
        if (index != 0) {
            result += ", ";
        }

        result += quoted(values[index]);
    }

    return result;
}

std::string relation_factory_type(
    std::string_view factory,
    std::string_view related,
    std::string_view through = {}
) {
    if (factory == "hasOne") {
        return "gungnir::HasOne<" + std::string{related} + ">";
    }
    if (factory == "hasMany") {
        return "gungnir::HasMany<" + std::string{related} + ">";
    }
    if (factory == "belongsTo") {
        return "gungnir::BelongsTo<" + std::string{related} + ">";
    }
    if (factory == "belongsToMany") {
        return "gungnir::BelongsToMany<" + std::string{related} + ">";
    }
    if (factory == "hasOneThrough") {
        return "gungnir::HasOneThrough<" + std::string{related} + ", " +
               std::string{through} + ">";
    }
    if (factory == "hasManyThrough") {
        return "gungnir::HasManyThrough<" + std::string{related} + ", " +
               std::string{through} + ">";
    }

    return {};
}

std::vector<std::string> default_relation_args(
    std::string_view factory,
    std::string_view owner,
    std::string_view related,
    std::string_view through
) {
    const auto owner_key = snake_case(owner) + "_id";
    const auto related_key = snake_case(related) + "_id";
    const auto through_key = snake_case(through) + "_id";

    if (factory == "hasOne" || factory == "hasMany") {
        return {owner_key, "id"};
    }

    if (factory == "belongsTo") {
        return {related_key, "id"};
    }

    if (factory == "belongsToMany") {
        auto first = snake_case(owner);
        auto second = snake_case(related);

        std::string pivot;
        if (first < second) {
            pivot = first + "_" + second;
        } else {
            pivot = second + "_" + first;
        }

        return {
            pivot,
            owner_key,
            related_key,
            "id",
            "id"
        };
    }

    if (factory == "hasOneThrough" || factory == "hasManyThrough") {
        return {
            owner_key,
            through_key,
            "id",
            "id"
        };
    }

    return {};
}

void overlay_args(
    std::vector<std::string>& defaults,
    const std::vector<std::string>& provided
) {
    const auto count = std::min(defaults.size(), provided.size());
    for (std::size_t index = 0; index < count; ++index) {
        defaults[index] = provided[index];
    }
}

std::string model_prelude(const ModelInfo& model) {
    std::vector<std::string> fillable;

    for (const auto& field : model.fields) {
        if (
            !field.primary_key &&
            field.column != "created_at" &&
            field.column != "updated_at" &&
            field.column != "deleted_at"
        ) {
            fillable.push_back(field.column);
        }
    }

    std::string result;
    result += "\npublic:\n";
    result += "    inline static constexpr gungnir::Table table{" +
              quoted(model.table) + "};\n";
    result += "    inline static constexpr auto fillable = gungnir::Fillable{";

    for (std::size_t index = 0; index < fillable.size(); ++index) {
        if (index != 0) {
            result += ", ";
        }
        result += quoted(fillable[index]);
    }

    result += "};\n";
    result += "    inline static constexpr bool timestamps = ";
    result += model.timestamps ? "true;\n" : "false;\n";

    if (model.connection != "default") {
        result += "    inline static constexpr gungnir::Connection connection{" +
                  quoted(model.connection) + "};\n";
    }

    if (model.soft_deletes) {
        result +=
            "    inline static constexpr gungnir::SoftDeletes soft_deletes{"
            "\"deleted_at\"};\n";
    }

    const auto has_column = [&](std::string_view column) {
        return std::any_of(
            model.fields.begin(),
            model.fields.end(),
            [&](const FieldInfo& field) {
                return field.column == column;
            }
        );
    };

    if (!has_column("id")) {
        result += "    gungnir::PrimaryKey<gungnir::Integer> id;\n";
    }

    if (model.timestamps) {
        if (!has_column("created_at")) {
            result +=
                "    gungnir::Field<std::optional<gungnir::String>> createdAt;\n";
        }
        if (!has_column("updated_at")) {
            result +=
                "    gungnir::Field<std::optional<gungnir::String>> updatedAt;\n";
        }
    }

    if (model.soft_deletes && !has_column("deleted_at")) {
        result +=
            "    gungnir::Field<std::optional<gungnir::String>> deletedAt;\n";
    }

    return result;
}

std::string metadata_for(const ModelInfo& model) {
    std::string result;
    if (model.needs_semicolon) {
        result += ";";
    }

    result += "\n\ntemplate <>\n";
    result += "struct gungnir::model::Generated<" + model.name + "> {\n";
    result += "    inline static constexpr auto attributes = std::tuple{\n";

    bool first_attribute = true;
    auto add_attribute = [&](std::string_view column, std::string_view member) {
        if (!first_attribute) {
            result += ",\n";
        }

        result += "        gungnir::model::attribute(" +
                  quoted(column) + ", &" + model.name + "::" +
                  std::string{member} + ")";
        first_attribute = false;
    };

    const bool has_id = std::any_of(
        model.fields.begin(),
        model.fields.end(),
        [](const FieldInfo& field) {
            return field.column == "id";
        }
    );

    if (!has_id) {
        add_attribute("id", "id");
    }

    for (const auto& field : model.fields) {
        add_attribute(field.column, field.name);
    }

    const auto has_column = [&](std::string_view column) {
        return std::any_of(
            model.fields.begin(),
            model.fields.end(),
            [&](const FieldInfo& field) {
                return field.column == column;
            }
        );
    };

    if (model.timestamps) {
        if (!has_column("created_at")) {
            add_attribute("created_at", "createdAt");
        }
        if (!has_column("updated_at")) {
            add_attribute("updated_at", "updatedAt");
        }
    }

    if (model.soft_deletes && !has_column("deleted_at")) {
        add_attribute("deleted_at", "deletedAt");
    }

    if (!first_attribute) {
        result += "\n";
    }

    result += "    };\n\n";
    result += "    inline static constexpr auto relations = std::tuple{\n";

    for (std::size_t index = 0; index < model.relations.size(); ++index) {
        const auto& relation = model.relations[index];

        result += "        gungnir::model::relation(" +
                  quoted(relation.name) + ", &" + model.name + "::" +
                  relation.backing_name + ")";

        if (index + 1 != model.relations.size()) {
            result += ",";
        }

        result += "\n";
    }

    result += "    };\n";
    result += "};\n";

    return result;
}

std::string relation_replacement(
    const RelationInfo& relation,
    std::size_t column
) {
    const std::string indent(column > 0 ? column - 1 : 0, ' ');

    std::string result;
    result += relation.cpp_type + " " + relation.backing_name +
              "{" + join_quoted(relation.constructor_args) + "};\n";
    result += indent + "[[nodiscard]] " + relation.cpp_type + "& " +
              relation.name + "() noexcept { return " +
              relation.backing_name + "; }\n";
    result += indent + "[[nodiscard]] const " + relation.cpp_type + "& " +
              relation.name + "() const noexcept { return " +
              relation.backing_name + "; }";

    return result;
}

void add_diagnostic(
    ModelLoweringResult& result,
    const std::string& file,
    const Token& token,
    std::string message
) {
    result.diagnostics.push_back(Diagnostic{
        DiagnosticLevel::error,
        SourceLocation{file, token.line, token.column},
        std::move(message)
    });
}

} // namespace

ModelLoweringResult ModelLowerer::lower(
    std::string_view source,
    std::string source_name
) const {
    ModelLoweringResult result;
    Lexer lexer{source};
    const auto tokens = lexer.tokenize();

    std::vector<ModelInfo> models;

    for (std::size_t index = 0; index < tokens.size(); ++index) {
        if (tokens[index].trivia() || tokens[index].lexeme != "class") {
            continue;
        }

        const auto name_index = next_significant(tokens, index);
        if (
            !name_index ||
            tokens[*name_index].kind != TokenKind::identifier
        ) {
            continue;
        }

        const auto colon = next_significant(tokens, *name_index);
        if (!colon || tokens[*colon].lexeme != ":") {
            continue;
        }

        const auto base = next_significant(tokens, *colon);
        if (!base || tokens[*base].lexeme != "Model") {
            continue;
        }

        const auto body_open = next_significant(tokens, *base);
        if (!body_open || tokens[*body_open].lexeme != "{") {
            add_diagnostic(
                result,
                source_name,
                tokens[*base],
                "Model declaration requires a body"
            );
            continue;
        }

        const auto body_close = matching_symbol(tokens, *body_open, "{", "}");
        if (!body_close) {
            add_diagnostic(
                result,
                source_name,
                tokens[*body_open],
                "Model body is missing a closing brace"
            );
            continue;
        }

        ModelInfo model;
        model.name = tokens[*name_index].lexeme;
        model.table = pluralize(snake_case(model.name));
        model.body_open_end =
            tokens[*body_open].offset + tokens[*body_open].lexeme.size();
        model.body_close_offset = tokens[*body_close].offset;

        const auto after_close = next_significant(tokens, *body_close);
        if (after_close && tokens[*after_close].lexeme == ";") {
            model.metadata_offset =
                tokens[*after_close].offset + tokens[*after_close].lexeme.size();
        } else {
            model.metadata_offset =
                tokens[*body_close].offset + tokens[*body_close].lexeme.size();
            model.needs_semicolon = true;
        }

        std::unordered_set<std::string> field_names;
        std::size_t cursor = *body_open + 1;
        std::size_t nested_braces = 0;

        while (cursor < *body_close) {
            const auto& token = tokens[cursor];

            if (token.trivia()) {
                ++cursor;
                continue;
            }

            if (token.lexeme == "{") {
                ++nested_braces;
                ++cursor;
                continue;
            }

            if (token.lexeme == "}") {
                if (nested_braces > 0) {
                    --nested_braces;
                }
                ++cursor;
                continue;
            }

            if (nested_braces != 0) {
                ++cursor;
                continue;
            }

            // Model configuration: table, connection, timestamps, softDeletes.
            if (
                token.kind == TokenKind::identifier &&
                (
                    token.lexeme == "table" ||
                    token.lexeme == "connection" ||
                    token.lexeme == "timestamps" ||
                    token.lexeme == "softDeletes"
                )
            ) {
                const auto equals = next_significant(tokens, cursor);
                const auto value = equals
                    ? next_significant(tokens, *equals)
                    : std::nullopt;
                const auto semicolon = value
                    ? next_significant(tokens, *value)
                    : std::nullopt;

                if (
                    equals &&
                    value &&
                    semicolon &&
                    tokens[*equals].lexeme == "=" &&
                    tokens[*semicolon].lexeme == ";"
                ) {
                    if (
                        token.lexeme == "table" ||
                        token.lexeme == "connection"
                    ) {
                        if (tokens[*value].kind != TokenKind::string_literal) {
                            add_diagnostic(
                                result,
                                source_name,
                                tokens[*value],
                                token.lexeme + " must be a string"
                            );
                        } else if (token.lexeme == "table") {
                            model.table = unquote(tokens[*value].lexeme);
                        } else {
                            model.connection = unquote(tokens[*value].lexeme);
                        }
                    } else {
                        const bool enabled = tokens[*value].lexeme == "true";
                        const bool disabled = tokens[*value].lexeme == "false";

                        if (!enabled && !disabled) {
                            add_diagnostic(
                                result,
                                source_name,
                                tokens[*value],
                                token.lexeme + " must be true or false"
                            );
                        } else if (token.lexeme == "timestamps") {
                            model.timestamps = enabled;
                        } else {
                            model.soft_deletes = enabled;
                        }
                    }

                    result.edits.push_back(SourceEdit{
                        token.offset,
                        tokens[*semicolon].offset +
                            tokens[*semicolon].lexeme.size(),
                        {}
                    });

                    cursor = *semicolon + 1;
                    continue;
                }
            }

            // Conventional primary key shorthand.
            if (token.lexeme == "id") {
                const auto semicolon = next_significant(tokens, cursor);
                if (semicolon && tokens[*semicolon].lexeme == ";") {
                    if (field_names.contains("id")) {
                        add_diagnostic(
                            result,
                            source_name,
                            token,
                            "Duplicate model field 'id'"
                        );
                    } else {
                        field_names.insert("id");
                        model.fields.push_back(FieldInfo{
                            SourceSpan{
                                token.offset,
                                token.offset + token.lexeme.size(),
                                token.line,
                                token.column
                            },
                            "id",
                            "id",
                            "gungnir::Integer",
                            false,
                            true,
                            std::nullopt
                        });

                        result.edits.push_back(SourceEdit{
                            token.offset,
                            token.offset + token.lexeme.size(),
                            "gungnir::PrimaryKey<gungnir::Integer> id"
                        });
                    }

                    cursor = *semicolon + 1;
                    continue;
                }
            }

            // Typed model field.
            if (const auto scalar = cpp_scalar(token.lexeme)) {
                auto next = next_significant(tokens, cursor);
                bool nullable = false;
                std::size_t type_end =
                    token.offset + token.lexeme.size();

                if (next && tokens[*next].lexeme == "?") {
                    nullable = true;
                    type_end =
                        tokens[*next].offset + tokens[*next].lexeme.size();
                    next = next_significant(tokens, *next);
                }

                if (next && tokens[*next].kind == TokenKind::identifier) {
                    const auto field_name = tokens[*next].lexeme;
                    const auto marker = next_significant(tokens, *next);

                    if (
                        marker &&
                        (
                            tokens[*marker].lexeme == ";" ||
                            tokens[*marker].lexeme == "="
                        )
                    ) {
                        if (field_names.contains(field_name)) {
                            add_diagnostic(
                                result,
                                source_name,
                                tokens[*next],
                                "Duplicate model field '" + field_name + "'"
                            );
                        } else {
                            field_names.insert(field_name);

                            FieldInfo field{
                                SourceSpan{
                                    token.offset,
                                    tokens[*next].offset,
                                    token.line,
                                    token.column
                                },
                                field_name,
                                snake_case(field_name),
                                *scalar,
                                nullable,
                                field_name == "id",
                                std::nullopt
                            };

                            model.fields.push_back(std::move(field));
                        }

                        cursor = *next + 1;
                        continue;
                    }
                }
            }

            // Relationship method:
            // posts() { return hasMany<Post>(); }
            if (token.kind == TokenKind::identifier) {
                const auto open_paren = next_significant(tokens, cursor);
                const auto close_paren = open_paren
                    ? next_significant(tokens, *open_paren)
                    : std::nullopt;
                const auto method_open = close_paren
                    ? next_significant(tokens, *close_paren)
                    : std::nullopt;

                if (
                    open_paren &&
                    close_paren &&
                    method_open &&
                    tokens[*open_paren].lexeme == "(" &&
                    tokens[*close_paren].lexeme == ")" &&
                    tokens[*method_open].lexeme == "{"
                ) {
                    const auto method_close = matching_symbol(
                        tokens,
                        *method_open,
                        "{",
                        "}"
                    );

                    if (method_close && *method_close < *body_close) {
                        auto returned = next_significant(tokens, *method_open);

                        if (
                            returned &&
                            tokens[*returned].lexeme == "return"
                        ) {
                            const auto factory = next_significant(tokens, *returned);
                            const auto angle_open = factory
                                ? next_significant(tokens, *factory)
                                : std::nullopt;
                            const auto related = angle_open
                                ? next_significant(tokens, *angle_open)
                                : std::nullopt;

                            if (
                                factory &&
                                angle_open &&
                                related &&
                                tokens[*angle_open].lexeme == "<" &&
                                tokens[*related].kind == TokenKind::identifier
                            ) {
                                const auto factory_name = tokens[*factory].lexeme;
                                const bool known_factory =
                                    factory_name == "hasOne" ||
                                    factory_name == "hasMany" ||
                                    factory_name == "belongsTo" ||
                                    factory_name == "belongsToMany" ||
                                    factory_name == "hasOneThrough" ||
                                    factory_name == "hasManyThrough";

                                if (known_factory) {
                                    std::string through_type;
                                    auto angle_close =
                                        next_significant(tokens, *related);

                                    if (
                                        angle_close &&
                                        tokens[*angle_close].lexeme == ","
                                    ) {
                                        const auto through =
                                            next_significant(tokens, *angle_close);
                                        if (
                                            through &&
                                            tokens[*through].kind ==
                                                TokenKind::identifier
                                        ) {
                                            through_type =
                                                tokens[*through].lexeme;
                                            angle_close =
                                                next_significant(tokens, *through);
                                        }
                                    }

                                    if (
                                        angle_close &&
                                        tokens[*angle_close].lexeme == ">"
                                    ) {
                                        const auto args_open =
                                            next_significant(tokens, *angle_close);
                                        const auto args_close = args_open
                                            ? matching_symbol(
                                                tokens,
                                                *args_open,
                                                "(",
                                                ")"
                                            )
                                            : std::nullopt;

                                        if (
                                            args_open &&
                                            args_close &&
                                            tokens[*args_open].lexeme == "("
                                        ) {
                                            std::vector<std::string> provided_args;

                                            for (
                                                auto arg = *args_open + 1;
                                                arg < *args_close;
                                                ++arg
                                            ) {
                                                if (tokens[arg].trivia()) {
                                                    continue;
                                                }

                                                if (
                                                    tokens[arg].kind ==
                                                    TokenKind::string_literal
                                                ) {
                                                    provided_args.push_back(
                                                        unquote(tokens[arg].lexeme)
                                                    );
                                                }
                                            }

                                            auto defaults = default_relation_args(
                                                factory_name,
                                                model.name,
                                                tokens[*related].lexeme,
                                                through_type
                                            );
                                            overlay_args(defaults, provided_args);

                                            RelationInfo relation;
                                            relation.method_span = SourceSpan{
                                                token.offset,
                                                tokens[*method_close].offset +
                                                    tokens[*method_close].lexeme.size(),
                                                token.line,
                                                token.column
                                            };
                                            relation.name = token.lexeme;
                                            relation.backing_name =
                                                "__gungnir_relation_" +
                                                token.lexeme;
                                            relation.cpp_type =
                                                relation_factory_type(
                                                    factory_name,
                                                    tokens[*related].lexeme,
                                                    through_type
                                                );
                                            relation.constructor_args =
                                                std::move(defaults);

                                            if (factory_name == "belongsTo") {
                                                relation.belongs_to_type =
                                                    tokens[*related].lexeme;
                                                relation.belongs_to_foreign_key =
                                                    relation.constructor_args.front();
                                            }

                                            model.relations.push_back(
                                                std::move(relation)
                                            );

                                            cursor = *method_close + 1;
                                            continue;
                                        }
                                    }
                                }
                            }
                        }

                        cursor = *method_close + 1;
                        continue;
                    }
                }
            }

            ++cursor;
        }

        // Mark matching belongsTo fields as ForeignKey<Related>.
        for (auto& relation : model.relations) {
            if (!relation.belongs_to_type) {
                continue;
            }

            for (auto& field : model.fields) {
                if (field.column == relation.belongs_to_foreign_key) {
                    field.related_type = relation.belongs_to_type;
                }
            }
        }

        models.push_back(std::move(model));
        index = *body_close;
    }

    for (const auto& model : models) {
        result.edits.push_back(SourceEdit{
            model.body_open_end,
            model.body_open_end,
            model_prelude(model)
        });

        for (const auto& field : model.fields) {
            std::string type = field.cpp_type;
            if (field.nullable) {
                type = "std::optional<" + type + ">";
            }

            std::string replacement;

            if (field.primary_key) {
                replacement = "gungnir::PrimaryKey<" + type + "> ";
            } else if (field.related_type) {
                replacement = "gungnir::ForeignKey<" +
                              *field.related_type + ", " + type + "> ";
            } else {
                replacement = "gungnir::Field<" + type + "> ";
            }

            // Bare id shorthand already has its complete replacement.
            if (
                field.primary_key &&
                field.type_span.begin + 2 == field.type_span.end &&
                source.substr(
                    field.type_span.begin,
                    field.type_span.end - field.type_span.begin
                ) == "id"
            ) {
                continue;
            }

            result.edits.push_back(SourceEdit{
                field.type_span.begin,
                field.type_span.end,
                std::move(replacement)
            });
        }

        for (const auto& relation : model.relations) {
            result.edits.push_back(SourceEdit{
                relation.method_span.begin,
                relation.method_span.end,
                relation_replacement(
                    relation,
                    relation.method_span.column
                )
            });
        }

        result.edits.push_back(SourceEdit{
            model.metadata_offset,
            model.metadata_offset,
            metadata_for(model)
        });
    }

    // Laravel-style ORM vocabulary in Gungnir source. Runtime stays snake_case.
    static const std::unordered_map<std::string, std::string> aliases{
        {"findOrFail", "find_or_fail"},
        {"findMany", "find_many"},
        {"createMany", "create_many"},
        {"firstOrCreate", "first_or_create"},
        {"updateOrCreate", "update_or_create"},
        {"firstOrFail", "first_or_fail"},
        {"whereIn", "where_in"},
        {"whereNotIn", "where_not_in"},
        {"whereBetween", "where_between"},
        {"whereNotBetween", "where_not_between"},
        {"whereNull", "where_null"},
        {"whereNotNull", "where_not_null"},
        {"whereColumn", "where_column"},
        {"orWhere", "or_where"},
        {"orderBy", "order_by"},
        {"orderByDesc", "order_by_desc"},
        {"leftJoin", "left_join"},
        {"rightJoin", "right_join"},
        {"crossJoin", "cross_join"},
        {"groupBy", "group_by"},
        {"lockForUpdate", "lock_for_update"},
        {"sharedLock", "shared_lock"},
        {"withDeleted", "with_deleted"},
        {"onlyDeleted", "only_deleted"},
        {"withTrashed", "with_deleted"},
        {"onlyTrashed", "only_deleted"},
        {"insertMany", "insert_many"},
        {"insertOrIgnore", "insert_or_ignore"},
        {"forceDelete", "force_remove"},
        {"delete", "remove"},
        {"isDirty", "is_dirty"},
        {"dirtyFields", "dirty_fields"},
        {"dirtyAttributes", "dirty_attributes"},
        {"wasRecentlyCreated", "was_recently_created"},
        {"relationLoaded", "relation_loaded"}
    };

    for (std::size_t index = 0; index < tokens.size(); ++index) {
        if (tokens[index].kind != TokenKind::identifier) {
            continue;
        }

        const auto found = aliases.find(tokens[index].lexeme);
        if (found == aliases.end()) {
            continue;
        }

        const auto previous = previous_significant(tokens, index);
        const auto next = next_significant(tokens, index);

        if (
            previous &&
            next &&
            (
                tokens[*previous].lexeme == "." ||
                tokens[*previous].lexeme == "::"
            ) &&
            tokens[*next].lexeme == "("
        ) {
            result.edits.push_back(SourceEdit{
                tokens[index].offset,
                tokens[index].offset + tokens[index].lexeme.size(),
                found->second
            });
        }
    }

    return result;
}

} // namespace gungnir::language
