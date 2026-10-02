#include <gungnir/language/compiler.hpp>

#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <string_view>

using namespace gungnir::language;

extern "C" int LLVMFuzzerTestOneInput(
    const std::uint8_t* data,
    std::size_t size
) {
    if (size > 1024 * 1024) {
        return 0;
    }

    const std::string_view source{
        reinterpret_cast<const char*>(data),
        size
    };

    const auto parsed = SyntaxParser{}.parse(
        source,
        "fuzz.gnr"
    );
    for (const auto& diagnostic : parsed.diagnostics) {
        if (
            diagnostic.location.line == 0 ||
            diagnostic.location.column == 0
        ) {
            std::abort();
        }
    }

    CompilerOptions options;
    options.emit_line_directives = false;
    options.validate_only = true;

    const auto result = Compiler{}.compile(
        source,
        "fuzz.gnr",
        options
    );

    if (result.success()) {
        if (!result.validated.has_value() || !result.code.empty()) {
            std::abort();
        }
    } else if (result.validated.has_value() || !result.code.empty()) {
        std::abort();
    }

    for (const auto& diagnostic : result.diagnostics) {
        if (
            diagnostic.location.line == 0 ||
            diagnostic.location.column == 0
        ) {
            std::abort();
        }
        if (
            diagnostic.span.valid &&
            (
                diagnostic.span.begin_offset > size ||
                diagnostic.span.end_offset > size ||
                diagnostic.span.end_offset <
                    diagnostic.span.begin_offset
            )
        ) {
            std::abort();
        }
    }

    return 0;
}
