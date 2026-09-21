#pragma once

#include <vector>

#include <gungnir/core/types.hpp>
#include <gungnir/database/backend.hpp>
#include <gungnir/migration/definition.hpp>

namespace gungnir::database {

enum class StatementKind {
    sql,
    mongodb_command
};

struct Statement {
    StatementKind kind{StatementKind::sql};
    String text;
};

struct Compilation {
    std::vector<Statement> statements;
    std::vector<String> warnings;

    [[nodiscard]] Boolean empty() const noexcept {
        return statements.empty();
    }

    [[nodiscard]] Boolean has_warnings() const noexcept {
        return !warnings.empty();
    }
};

[[nodiscard]] Compilation compile(
    const migration::Plan& plan,
    Backend backend
);

} // namespace gungnir::database
