#pragma once
#include <optional>
#include <string>
#include <string_view>
#include <vector>
#include <gungnir/language/compiler.hpp>

namespace gungnir::language {
struct DocumentSymbol {
    std::string name, detail;
    int kind{12};
    Origin range, selection;
    std::vector<DocumentSymbol> children;
};
struct SymbolInfo { std::string name, detail; Origin definition, selection; };
struct Completion { std::string label, detail; int kind{6}; };
// Compiler-backed editor snapshot. Offsets are bytes; the protocol adapter
// converts them to/from negotiated (UTF-16) document positions.
class LanguageServer {
public:
    void update(std::vector<SourceFile> sources);
    [[nodiscard]] const std::vector<Diagnostic>& diagnostics() const noexcept { return result_.diagnostics; }
    [[nodiscard]] std::vector<Diagnostic> diagnostics(std::string_view source,std::string file="<memory>") const;
    [[nodiscard]] std::vector<DocumentSymbol> symbols(std::string_view file) const;
    [[nodiscard]] std::optional<SymbolInfo> symbol_at(std::string_view file,std::size_t offset) const;
    [[nodiscard]] std::vector<Completion> completions(std::string_view file,std::size_t offset) const;
private:
    std::vector<SourceFile> sources_;
    CompilationResult result_;
    [[nodiscard]] Origin selection(const Origin& origin,std::string_view name,bool last=false) const;
    [[nodiscard]] std::optional<Origin> definition(SymbolId id) const;
    [[nodiscard]] std::string detail(SymbolId id) const;
};
}
