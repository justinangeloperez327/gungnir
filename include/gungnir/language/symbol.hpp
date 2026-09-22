#pragma once
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace gungnir::language {

enum class SymbolKind { variable, function, class_, interface_, enum_, module_, import_ };

struct Symbol {
    std::string name;
    SymbolKind kind{SymbolKind::variable};
    std::string type;
    bool immutable{false};
};

class SymbolTable {
public:
    void push_scope() { scopes_.emplace_back(); }
    void pop_scope() { if (scopes_.size() > 1) scopes_.pop_back(); }

    [[nodiscard]] bool declare(Symbol symbol) {
        if (scopes_.empty()) push_scope();
        auto& scope = scopes_.back();
        if (scope.contains(symbol.name)) return false;
        scope.emplace(symbol.name, std::move(symbol));
        return true;
    }

    [[nodiscard]] const Symbol* resolve(std::string_view name) const {
        for (auto it = scopes_.rbegin(); it != scopes_.rend(); ++it) {
            const auto found = it->find(std::string{name});
            if (found != it->end()) return &found->second;
        }
        return nullptr;
    }

private:
    std::vector<std::unordered_map<std::string, Symbol>> scopes_{{}};
};

} // namespace gungnir::language
