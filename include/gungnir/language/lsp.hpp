#pragma once

#include <string>
#include <string_view>
#include <vector>

#include <gungnir/language/diagnostic.hpp>
#include <gungnir/language/transpiler.hpp>

namespace gungnir::language {

class LanguageServer {
public:
    [[nodiscard]] std::vector<Diagnostic> diagnostics(std::string_view source,std::string file="<memory>") const {
        return transpiler_.transpile(source,std::move(file)).diagnostics;
    }
    [[nodiscard]] std::string hover(std::string_view symbol) const {
        if(symbol=="Model")return "Gungnir persistent model";
        if(symbol=="Controller")return "Gungnir HTTP controller";
        if(symbol=="Route")return "Gungnir route registration API";
        return {};
    }
private:
    Transpiler transpiler_;
};

} // namespace gungnir::language
