#pragma once
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>

namespace gungnir::language {

enum class TypeKind { unknown, null_, boolean, integer, decimal, string, optional, list, map, named };

struct Type {
    TypeKind kind{TypeKind::unknown};
    std::string name;
    bool nullable{false};

    [[nodiscard]] bool known() const noexcept { return kind != TypeKind::unknown; }
};

class TypeSystem {
public:
    [[nodiscard]] Type infer_literal(std::string_view value) const {
        if (value == "null") return {TypeKind::null_, "null", true};
        if (value == "true" || value == "false") return {TypeKind::boolean, "bool", false};
        if (value.size() >= 2 && value.front() == '"' && value.back() == '"') return {TypeKind::string, "string", false};
        bool digit = !value.empty();
        bool decimal = false;
        for (const char c : value) {
            if (c == '.') { decimal = true; continue; }
            if (c < '0' || c > '9') { digit = false; break; }
        }
        if (digit) return {decimal ? TypeKind::decimal : TypeKind::integer, decimal ? "decimal" : "int", false};
        return {};
    }

    [[nodiscard]] bool assignable(const Type& target, const Type& value) const noexcept {
        if (!target.known() || !value.known()) return true;
        if (value.kind == TypeKind::null_) return target.nullable || target.kind == TypeKind::optional;
        if (target.kind == value.kind) return true;
        return target.kind == TypeKind::decimal && value.kind == TypeKind::integer;
    }
};

} // namespace gungnir::language
