#include <gungnir/language/lexer.hpp>

#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <string_view>
#include <vector>

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

    for (bool single_quoted_strings : {false, true}) {
        std::vector<Diagnostic> diagnostics;
        const auto tokens = Lexer{source}.tokenize(
            &diagnostics,
            "fuzz.gnr",
            single_quoted_strings
        );

        if (tokens.empty()) {
            std::abort();
        }

        for (const auto& token : tokens) {
            if (token.offset > size) {
                std::abort();
            }
        }

        for (const auto& diagnostic : diagnostics) {
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
    }

    return 0;
}
