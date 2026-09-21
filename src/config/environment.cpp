#include <gungnir/config/environment.hpp>

#include <algorithm>
#include <cctype>
#include <charconv>
#include <cstdlib>
#include <fstream>
#include <stdexcept>
#include <string>
#include <utility>

namespace gungnir::config {

namespace {

std::string_view trim(std::string_view value) {
    while (
        !value.empty() &&
        std::isspace(
            static_cast<unsigned char>(value.front())
        ) != 0
    ) {
        value.remove_prefix(1);
    }

    while (
        !value.empty() &&
        std::isspace(
            static_cast<unsigned char>(value.back())
        ) != 0
    ) {
        value.remove_suffix(1);
    }

    return value;
}

bool valid_key(std::string_view key) {
    if (key.empty()) {
        return false;
    }

    const auto first =
        static_cast<unsigned char>(key.front());

    if (
        std::isalpha(first) == 0 &&
        key.front() != '_'
    ) {
        return false;
    }

    return std::all_of(
        key.begin() + 1,
        key.end(),
        [](char character) {
            const auto value =
                static_cast<unsigned char>(character);

            return
                std::isalnum(value) != 0 ||
                character == '_';
        }
    );
}

String decode_double_quoted(
    std::string_view value
) {
    String output;
    output.reserve(value.size());

    for (
        std::size_t index = 0;
        index < value.size();
        ++index
    ) {
        const char character = value[index];

        if (character != '\\') {
            output.push_back(character);
            continue;
        }

        if (index + 1 >= value.size()) {
            throw std::invalid_argument(
                "Environment value ends with an incomplete escape"
            );
        }

        const char escaped = value[++index];

        switch (escaped) {
        case 'n':
            output.push_back('\n');
            break;
        case 'r':
            output.push_back('\r');
            break;
        case 't':
            output.push_back('\t');
            break;
        case '\\':
            output.push_back('\\');
            break;
        case '"':
            output.push_back('"');
            break;
        default:
            output.push_back(escaped);
            break;
        }
    }

    return output;
}

String parse_value(std::string_view value) {
    value = trim(value);

    if (value.empty()) {
        return {};
    }

    if (
        value.front() == '\'' ||
        value.front() == '"'
    ) {
        const char quote = value.front();
        std::size_t close = 1;
        bool escaped = false;

        for (; close < value.size(); ++close) {
            if (
                quote == '"' &&
                value[close] == '\\' &&
                !escaped
            ) {
                escaped = true;
                continue;
            }

            if (
                value[close] == quote &&
                !escaped
            ) {
                break;
            }

            escaped = false;
        }

        if (close >= value.size()) {
            throw std::invalid_argument(
                "Environment quoted value is missing its closing quote"
            );
        }

        const auto trailing = trim(
            value.substr(close + 1)
        );

        if (
            !trailing.empty() &&
            trailing.front() != '#'
        ) {
            throw std::invalid_argument(
                "Unexpected data after environment quoted value"
            );
        }

        const auto body = value.substr(
            1,
            close - 1
        );

        if (quote == '\'') {
            return String{body};
        }

        return decode_double_quoted(body);
    }

    std::size_t comment =
        std::string_view::npos;

    for (
        std::size_t index = 0;
        index < value.size();
        ++index
    ) {
        if (
            value[index] == '#' &&
            (
                index == 0 ||
                std::isspace(
                    static_cast<unsigned char>(
                        value[index - 1]
                    )
                ) != 0
            )
        ) {
            comment = index;
            break;
        }
    }

    if (comment != std::string_view::npos) {
        value = value.substr(0, comment);
    }

    return String{trim(value)};
}

String lower(String value) {
    std::transform(
        value.begin(),
        value.end(),
        value.begin(),
        [](unsigned char character) {
            return static_cast<char>(
                std::tolower(character)
            );
        }
    );

    return value;
}

} // namespace

Environment& Environment::load(
    const std::filesystem::path& path,
    bool optional,
    bool overwrite
) {
    std::ifstream input{path};

    if (!input) {
        if (optional) {
            return *this;
        }

        throw std::runtime_error(
            "Unable to open environment file: " +
            path.string()
        );
    }

    String line;
    std::size_t line_number = 0;

    while (std::getline(input, line)) {
        ++line_number;

        if (
            !line.empty() &&
            line.back() == '\r'
        ) {
            line.pop_back();
        }

        auto content = trim(line);

        if (
            content.empty() ||
            content.front() == '#'
        ) {
            continue;
        }

        constexpr std::string_view export_prefix{
            "export "
        };

        if (
            content.starts_with(export_prefix)
        ) {
            content = trim(
                content.substr(
                    export_prefix.size()
                )
            );
        }

        const auto separator =
            content.find('=');

        if (separator == std::string_view::npos) {
            throw std::invalid_argument(
                "Invalid environment entry at " +
                path.string() +
                ":" +
                std::to_string(line_number)
            );
        }

        const auto key = trim(
            content.substr(0, separator)
        );

        if (!valid_key(key)) {
            throw std::invalid_argument(
                "Invalid environment key at " +
                path.string() +
                ":" +
                std::to_string(line_number)
            );
        }

        auto value = parse_value(
            content.substr(separator + 1)
        );

        const auto found =
            values_.find(String{key});

        if (
            found == values_.end() ||
            overwrite
        ) {
            values_.insert_or_assign(
                String{key},
                std::move(value)
            );
        }
    }

    return *this;
}

Environment& Environment::set(
    String key,
    String value
) {
    if (!valid_key(key)) {
        throw std::invalid_argument(
            "Invalid environment key '" +
            key +
            "'"
        );
    }

    values_.insert_or_assign(
        std::move(key),
        std::move(value)
    );

    return *this;
}

bool Environment::has(
    std::string_view key
) const {
    return find(key).has_value();
}

std::optional<String> Environment::find(
    std::string_view key
) const {
    const String owned_key{key};

    if (
        const char* process_value =
            std::getenv(owned_key.c_str())
    ) {
        return String{process_value};
    }

    const auto found =
        values_.find(owned_key);

    if (found == values_.end()) {
        return std::nullopt;
    }

    return found->second;
}

String Environment::get(
    std::string_view key,
    String fallback
) const {
    const auto value = find(key);

    return value
        ? *value
        : std::move(fallback);
}

Int64 Environment::integer(
    std::string_view key,
    Int64 fallback
) const {
    const auto value = find(key);

    if (!value) {
        return fallback;
    }

    Int64 parsed = 0;

    const auto result = std::from_chars(
        value->data(),
        value->data() + value->size(),
        parsed
    );

    if (
        result.ec != std::errc{} ||
        result.ptr != value->data() +
            value->size()
    ) {
        throw std::invalid_argument(
            "Environment value '" +
            String{key} +
            "' must be an integer"
        );
    }

    return parsed;
}

Boolean Environment::boolean(
    std::string_view key,
    Boolean fallback
) const {
    const auto value = find(key);

    if (!value) {
        return fallback;
    }

    const auto normalized =
        lower(*value);

    if (
        normalized == "true" ||
        normalized == "1" ||
        normalized == "yes" ||
        normalized == "on"
    ) {
        return true;
    }

    if (
        normalized == "false" ||
        normalized == "0" ||
        normalized == "no" ||
        normalized == "off"
    ) {
        return false;
    }

    throw std::invalid_argument(
        "Environment value '" +
        String{key} +
        "' must be boolean"
    );
}

const std::unordered_map<String, String>&
Environment::loaded() const noexcept {
    return values_;
}

} // namespace gungnir::config
