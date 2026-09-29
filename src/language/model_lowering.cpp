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
    ModelRelationshipKind kind,
    std::string_view related,
    std::string_view through = {}
) {
    switch (kind) {
    case ModelRelationshipKind::has_one:
        return "gungnir::HasOne<" + std::string{related} + ">";
    case ModelRelationshipKind::has_many:
        return "gungnir::HasMany<" + std::string{related} + ">";
    case ModelRelationshipKind::belongs_to:
        return "gungnir::BelongsTo<" + std::string{related} + ">";
    case ModelRelationshipKind::belongs_to_many:
        return "gungnir::BelongsToMany<" + std::string{related} + ">";
    case ModelRelationshipKind::has_one_through:
        return "gungnir::HasOneThrough<" + std::string{related} + ", " +
               std::string{through} + ">";
    case ModelRelationshipKind::has_many_through:
        return "gungnir::HasManyThrough<" + std::string{related} + ", " +
               std::string{through} + ">";
    }

    return {};
}

std::vector<std::string> default_relation_args(
    ModelRelationshipKind kind,
    std::string_view owner,
    std::string_view related,
    std::string_view through
) {
    const auto owner_key = snake_case(owner) + "_id";
    const auto related_key = snake_case(related) + "_id";
    const auto through_key = snake_case(through) + "_id";

    switch (kind) {
    case ModelRelationshipKind::has_one:
    case ModelRelationshipKind::has_many:
        return {owner_key, "id"};
    case ModelRelationshipKind::belongs_to:
        return {related_key, "id"};
    case ModelRelationshipKind::belongs_to_many: {
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
    case ModelRelationshipKind::has_one_through:
    case ModelRelationshipKind::has_many_through:
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



} // namespace

ModelLoweringResult ModelLowerer::lower(
    std::string_view source,
    const Program& program,
    std::string source_name
) const {
    ModelLoweringResult result;
    (void) source_name;
    Lexer lexer{source};
    const auto tokens = lexer.tokenize();

    std::vector<ModelInfo> models;

    for (const auto& node : program.nodes) {
        const auto* base = std::get_if<FrameworkBase>(&node);
        const auto* framework = std::get_if<FrameworkDeclaration>(&node);
        const auto kind = base ? base->kind : framework
            ? framework->kind : FrameworkBaseKind::controller;
        if (kind != FrameworkBaseKind::model || (!base && !framework)) {
            continue;
        }
        const auto& members = base ? base->members : framework->members;
        const auto& name = base ? base->class_name : framework->class_name;
        const auto span = base ? base->declaration_span : framework->span;
        const auto body = base ? base->body_open_span
            : framework->body_open_span;
        const auto body_offset = body.begin;
        if (body.end == 0 || body_offset >= span.end || span.end == 0) {
            continue;
        }
        const auto body_close_offset = span.end - 1;
        ModelInfo model;
        model.name = name;
        model.table = pluralize(snake_case(model.name));
        model.body_open_end = body_offset + 1;
        model.body_close_offset = body_close_offset;
        model.metadata_offset = span.end;
        const auto after = std::find_if(tokens.begin(), tokens.end(),
            [&](const Token& token) {
                return token.offset >= span.end && !token.trivia();
            });
        if (after != tokens.end() && after->lexeme == ";") {
            model.metadata_offset = after->offset + after->lexeme.size();
        } else {
            model.needs_semicolon = true;
        }

        for (const auto member_index : members) {
            const auto& node = program.nodes[member_index];
            if (
                const auto* configuration =
                    std::get_if<ModelConfiguration>(&node)
            ) {

                switch (configuration->kind) {
                case ModelConfigurationKind::table:
                    model.table = configuration->value;
                    break;
                case ModelConfigurationKind::connection:
                    model.connection = configuration->value;
                    break;
                case ModelConfigurationKind::timestamps:
                    model.timestamps = configuration->enabled;
                    break;
                case ModelConfigurationKind::soft_deletes:
                    model.soft_deletes = configuration->enabled;
                    break;
                }

                result.edits.push_back(SourceEdit{
                    configuration->span.begin,
                    configuration->span.end,
                    {}
                });

                continue;
            }

            if (
                const auto* relationship =
                    std::get_if<ModelRelationship>(&node)
            ) {

                auto constructor_args = default_relation_args(
                    relationship->kind,
                    model.name,
                    relationship->related_type,
                    relationship->through_type
                );
                overlay_args(
                    constructor_args,
                    relationship->arguments
                );

                RelationInfo relation;
                relation.method_span = relationship->span;
                relation.name = relationship->name;
                relation.backing_name =
                    "__gungnir_relation_" + relationship->name;
                relation.cpp_type = relation_factory_type(
                    relationship->kind,
                    relationship->related_type,
                    relationship->through_type
                );
                relation.constructor_args =
                    std::move(constructor_args);

                if (
                    relationship->kind ==
                    ModelRelationshipKind::belongs_to
                ) {
                    relation.belongs_to_type =
                        relationship->related_type;
                    relation.belongs_to_foreign_key =
                        relation.constructor_args.front();
                }

                model.relations.push_back(std::move(relation));
                continue;
            }

            const auto* field = std::get_if<ModelField>(&node);
            if (!field) {
                continue;
            }

            const auto scalar = cpp_scalar(field->type_name);
            if (!scalar) {
                continue;
            }

            model.fields.push_back(FieldInfo{
                field->type_span,
                field->name,
                snake_case(field->name),
                *scalar,
                field->nullable,
                field->primary_key,
                std::nullopt
            });

            if (field->shorthand) {
                result.edits.push_back(SourceEdit{
                    field->type_span.begin,
                    field->type_span.end,
                    "gungnir::PrimaryKey<gungnir::Integer> id"
                });
            }
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

        bool qualified_access = false;
        if (previous) {
            qualified_access = tokens[*previous].lexeme == ".";

            if (tokens[*previous].lexeme == ":") {
                const auto before_previous =
                    previous_significant(tokens, *previous);

                qualified_access =
                    before_previous &&
                    tokens[*before_previous].lexeme == ":";

                if (qualified_access) {
                    const auto qualifier =
                        previous_significant(
                            tokens,
                            *before_previous
                        );

                    if (
                        qualifier &&
                        tokens[*qualifier].lexeme == "Route"
                    ) {
                        continue;
                    }
                }
            }
        }

        if (
            previous &&
            next &&
            qualified_access &&
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
