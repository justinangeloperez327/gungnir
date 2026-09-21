#include <gungnir/view/engine.hpp>

#include <algorithm>
#include <cctype>
#include <fstream>
#include <iterator>
#include <stdexcept>
#include <string>

namespace gungnir::view {

namespace {

std::string_view trim(std::string_view value) {
    while (
        !value.empty() &&
        std::isspace(static_cast<unsigned char>(value.front())) != 0
    ) {
        value.remove_prefix(1);
    }

    while (
        !value.empty() &&
        std::isspace(static_cast<unsigned char>(value.back())) != 0
    ) {
        value.remove_suffix(1);
    }

    return value;
}

String escape_html(std::string_view value) {
    String result;
    result.reserve(value.size());

    for (const char character : value) {
        switch (character) {
        case '&':
            result += "&amp;";
            break;
        case '<':
            result += "&lt;";
            break;
        case '>':
            result += "&gt;";
            break;
        case '"':
            result += "&quot;";
            break;
        case '\'':
            result += "&#39;";
            break;
        default:
            result.push_back(character);
            break;
        }
    }

    return result;
}

const Value* resolve_path(
    std::string_view path,
    const Data& data,
    const Value* current
) {
    path = trim(path);

    if (path == "this") {
        return current;
    }

    auto resolve_from = [&](const Value* root) -> const Value* {
        if (!root) {
            return nullptr;
        }

        const Value* value = root;
        std::size_t start = 0;

        while (start <= path.size()) {
            const auto separator = path.find('.', start);
            const auto part = separator == std::string_view::npos
                ? path.substr(start)
                : path.substr(start, separator - start);

            if (part.empty()) {
                return nullptr;
            }

            value = value->get(part);
            if (!value) {
                return nullptr;
            }

            if (separator == std::string_view::npos) {
                return value;
            }

            start = separator + 1;
        }

        return value;
    };

    if (current && current->is_object()) {
        if (const auto* found = resolve_from(current)) {
            return found;
        }
    }

    const auto first_separator = path.find('.');
    const auto root_name = first_separator == std::string_view::npos
        ? path
        : path.substr(0, first_separator);

    const auto* root = data.get(root_name);
    if (!root) {
        return nullptr;
    }

    if (first_separator == std::string_view::npos) {
        return root;
    }

    auto remaining = path.substr(first_separator + 1);
    const Value* value = root;

    while (!remaining.empty()) {
        const auto separator = remaining.find('.');
        const auto part = separator == std::string_view::npos
            ? remaining
            : remaining.substr(0, separator);

        value = value->get(part);
        if (!value) {
            return nullptr;
        }

        if (separator == std::string_view::npos) {
            break;
        }

        remaining.remove_prefix(separator + 1);
    }

    return value;
}

std::size_t find_matching_each(
    std::string_view source,
    std::size_t open_end
) {
    std::size_t depth = 1;
    std::size_t cursor = open_end;

    while (cursor < source.size()) {
        const auto next_open = source.find("{{#each ", cursor);
        const auto next_close = source.find("{{/each}}", cursor);

        if (next_close == std::string_view::npos) {
            return std::string_view::npos;
        }

        if (
            next_open != std::string_view::npos &&
            next_open < next_close
        ) {
            ++depth;
            cursor = next_open + 8;
            continue;
        }

        --depth;
        if (depth == 0) {
            return next_close;
        }

        cursor = next_close + 9;
    }

    return std::string_view::npos;
}

String render_block(
    std::string_view source,
    const Data& data,
    const Value* current
) {
    String working{source};

    while (true) {
        const auto open = working.find("{{#each ");
        if (open == String::npos) {
            break;
        }

        const auto header_end = working.find("}}", open);
        if (header_end == String::npos) {
            throw std::runtime_error(
                "Gungnir view each block is missing '}}'"
            );
        }

        const auto path = trim(
            std::string_view{working}.substr(
                open + 8,
                header_end - (open + 8)
            )
        );

        const auto close = find_matching_each(
            working,
            header_end + 2
        );

        if (close == String::npos) {
            throw std::runtime_error(
                "Gungnir view each block is missing '{{/each}}'"
            );
        }

        const auto body_start = header_end + 2;
        const auto body = std::string_view{working}.substr(
            body_start,
            close - body_start
        );

        String replacement;
        const auto* collection = resolve_path(path, data, current);

        if (collection && collection->is_array()) {
            for (const auto& item : collection->as_array()) {
                replacement += render_block(body, data, &item);
            }
        }

        working.replace(
            open,
            close + 9 - open,
            replacement
        );
    }

    // Raw interpolation: {{{ value }}}
    while (true) {
        const auto open = working.find("{{{");
        if (open == String::npos) {
            break;
        }

        const auto close = working.find("}}}", open + 3);
        if (close == String::npos) {
            throw std::runtime_error(
                "Gungnir raw view expression is missing '}}}'"
            );
        }

        const auto path = trim(
            std::string_view{working}.substr(
                open + 3,
                close - (open + 3)
            )
        );

        const auto* value = resolve_path(path, data, current);
        const auto replacement = value ? value->string() : String{};

        working.replace(
            open,
            close + 3 - open,
            replacement
        );
    }

    // Escaped interpolation: {{ value }}
    std::size_t cursor = 0;
    while (true) {
        const auto open = working.find("{{", cursor);
        if (open == String::npos) {
            break;
        }

        const auto close = working.find("}}", open + 2);
        if (close == String::npos) {
            throw std::runtime_error(
                "Gungnir view expression is missing '}}'"
            );
        }

        const auto path = trim(
            std::string_view{working}.substr(
                open + 2,
                close - (open + 2)
            )
        );

        const auto* value = resolve_path(path, data, current);
        const auto replacement = value
            ? escape_html(value->string())
            : String{};

        working.replace(
            open,
            close + 2 - open,
            replacement
        );

        cursor = open + replacement.size();
    }

    return working;
}

} // namespace

Engine::Engine(std::filesystem::path root)
    : root_(std::move(root)) {}

Engine& Engine::root(std::filesystem::path value) {
    root_ = std::move(value);
    return *this;
}

const std::filesystem::path& Engine::root() const noexcept {
    return root_;
}

String Engine::render(
    std::string_view name,
    const Data& data
) const {
    const std::filesystem::path relative{name};

    if (
        relative.is_absolute() ||
        std::find(
            relative.begin(),
            relative.end(),
            std::filesystem::path{".."}
        ) != relative.end()
    ) {
        throw std::invalid_argument(
            "Gungnir view name must stay inside the configured view root"
        );
    }

    auto path = root_ / relative;

    if (!path.has_extension()) {
        path += ".html";
    }

    std::ifstream input{path, std::ios::binary};
    if (!input) {
        throw std::runtime_error(
            "Gungnir view not found: " + path.string()
        );
    }

    const String source{
        std::istreambuf_iterator<char>{input},
        std::istreambuf_iterator<char>{}
    };

    return render_text(source, data);
}

String Engine::render_text(
    std::string_view source,
    const Data& data
) const {
    return render_block(source, data, nullptr);
}

} // namespace gungnir::view
