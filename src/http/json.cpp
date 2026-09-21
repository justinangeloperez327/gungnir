#include <gungnir/http/json.hpp>

#include <charconv>
#include <iomanip>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <string>

namespace gungnir::http {

namespace {

String escape_string(std::string_view value) {
    String output;
    output.reserve(value.size() + 8);

    for (const unsigned char character : value) {
        switch (character) {
        case '"':
            output += "\\\"";
            break;
        case '\\':
            output += "\\\\";
            break;
        case '\b':
            output += "\\b";
            break;
        case '\f':
            output += "\\f";
            break;
        case '\n':
            output += "\\n";
            break;
        case '\r':
            output += "\\r";
            break;
        case '\t':
            output += "\\t";
            break;
        default:
            if (character < 0x20U) {
                std::ostringstream escaped;
                escaped << "\\u"
                        << std::hex
                        << std::setw(4)
                        << std::setfill('0')
                        << static_cast<unsigned>(character);
                output += escaped.str();
            } else {
                output.push_back(static_cast<char>(character));
            }
            break;
        }
    }

    return output;
}

void append_utf8(String& output, std::uint32_t codepoint) {
    if (codepoint <= 0x7FU) {
        output.push_back(static_cast<char>(codepoint));
    } else if (codepoint <= 0x7FFU) {
        output.push_back(static_cast<char>(0xC0U | (codepoint >> 6U)));
        output.push_back(static_cast<char>(0x80U | (codepoint & 0x3FU)));
    } else if (codepoint <= 0xFFFFU) {
        output.push_back(static_cast<char>(0xE0U | (codepoint >> 12U)));
        output.push_back(static_cast<char>(0x80U | ((codepoint >> 6U) & 0x3FU)));
        output.push_back(static_cast<char>(0x80U | (codepoint & 0x3FU)));
    } else {
        output.push_back(static_cast<char>(0xF0U | (codepoint >> 18U)));
        output.push_back(static_cast<char>(0x80U | ((codepoint >> 12U) & 0x3FU)));
        output.push_back(static_cast<char>(0x80U | ((codepoint >> 6U) & 0x3FU)));
        output.push_back(static_cast<char>(0x80U | (codepoint & 0x3FU)));
    }
}

class Parser {
public:
    explicit Parser(std::string_view source) : source_(source) {}

    Json parse() {
        skip_space();
        auto value = parse_value();
        skip_space();

        if (cursor_ != source_.size()) {
            throw std::invalid_argument("Unexpected data after JSON value");
        }

        return value;
    }

private:
    void skip_space() {
        while (
            cursor_ < source_.size() &&
            (
                source_[cursor_] == ' ' ||
                source_[cursor_] == '\n' ||
                source_[cursor_] == '\r' ||
                source_[cursor_] == '\t'
            )
        ) {
            ++cursor_;
        }
    }

    [[nodiscard]] bool consume(std::string_view text) {
        if (source_.substr(cursor_, text.size()) != text) {
            return false;
        }

        cursor_ += text.size();
        return true;
    }

    Json parse_value() {
        skip_space();

        if (cursor_ >= source_.size()) {
            throw std::invalid_argument("Unexpected end of JSON");
        }

        const char current = source_[cursor_];

        if (current == '"') {
            return Json{parse_string()};
        }

        if (current == '{') {
            return parse_object();
        }

        if (current == '[') {
            return parse_array();
        }

        if (consume("true")) {
            return Json{true};
        }

        if (consume("false")) {
            return Json{false};
        }

        if (consume("null")) {
            return Json{nullptr};
        }

        if (current == '-' || (current >= '0' && current <= '9')) {
            return parse_number();
        }

        throw std::invalid_argument("Invalid JSON value");
    }

    String parse_string() {
        if (source_[cursor_] != '"') {
            throw std::invalid_argument("JSON string must begin with a quote");
        }

        ++cursor_;
        String output;

        while (cursor_ < source_.size()) {
            const char current = source_[cursor_++];

            if (current == '"') {
                return output;
            }

            if (current != '\\') {
                if (static_cast<unsigned char>(current) < 0x20U) {
                    throw std::invalid_argument(
                        "Invalid control character in JSON string"
                    );
                }

                output.push_back(current);
                continue;
            }

            if (cursor_ >= source_.size()) {
                throw std::invalid_argument("Incomplete JSON escape");
            }

            const char escaped = source_[cursor_++];

            switch (escaped) {
            case '"':
                output.push_back('"');
                break;
            case '\\':
                output.push_back('\\');
                break;
            case '/':
                output.push_back('/');
                break;
            case 'b':
                output.push_back('\b');
                break;
            case 'f':
                output.push_back('\f');
                break;
            case 'n':
                output.push_back('\n');
                break;
            case 'r':
                output.push_back('\r');
                break;
            case 't':
                output.push_back('\t');
                break;
            case 'u': {
                if (cursor_ + 4 > source_.size()) {
                    throw std::invalid_argument(
                        "Incomplete JSON unicode escape"
                    );
                }

                std::uint32_t codepoint = 0;

                for (int index = 0; index < 4; ++index) {
                    const char digit = source_[cursor_++];
                    codepoint <<= 4U;

                    if (digit >= '0' && digit <= '9') {
                        codepoint |= static_cast<std::uint32_t>(digit - '0');
                    } else if (digit >= 'a' && digit <= 'f') {
                        codepoint |= static_cast<std::uint32_t>(digit - 'a' + 10);
                    } else if (digit >= 'A' && digit <= 'F') {
                        codepoint |= static_cast<std::uint32_t>(digit - 'A' + 10);
                    } else {
                        throw std::invalid_argument(
                            "Invalid JSON unicode escape"
                        );
                    }
                }

                append_utf8(output, codepoint);
                break;
            }
            default:
                throw std::invalid_argument("Invalid JSON escape");
            }
        }

        throw std::invalid_argument("Unterminated JSON string");
    }

    Json parse_object() {
        ++cursor_;
        skip_space();

        Json::Object object;

        if (cursor_ < source_.size() && source_[cursor_] == '}') {
            ++cursor_;
            return Json::object(std::move(object));
        }

        while (true) {
            skip_space();

            if (cursor_ >= source_.size() || source_[cursor_] != '"') {
                throw std::invalid_argument(
                    "JSON object key must be a string"
                );
            }

            auto key = parse_string();
            skip_space();

            if (cursor_ >= source_.size() || source_[cursor_] != ':') {
                throw std::invalid_argument(
                    "JSON object is missing ':'"
                );
            }

            ++cursor_;
            auto value = parse_value();
            object.insert_or_assign(std::move(key), std::move(value));
            skip_space();

            if (cursor_ >= source_.size()) {
                throw std::invalid_argument("Unterminated JSON object");
            }

            if (source_[cursor_] == '}') {
                ++cursor_;
                break;
            }

            if (source_[cursor_] != ',') {
                throw std::invalid_argument(
                    "JSON object is missing ','"
                );
            }

            ++cursor_;
        }

        return Json::object(std::move(object));
    }

    Json parse_array() {
        ++cursor_;
        skip_space();

        Json::Array array;

        if (cursor_ < source_.size() && source_[cursor_] == ']') {
            ++cursor_;
            return Json::array(std::move(array));
        }

        while (true) {
            array.push_back(parse_value());
            skip_space();

            if (cursor_ >= source_.size()) {
                throw std::invalid_argument("Unterminated JSON array");
            }

            if (source_[cursor_] == ']') {
                ++cursor_;
                break;
            }

            if (source_[cursor_] != ',') {
                throw std::invalid_argument(
                    "JSON array is missing ','"
                );
            }

            ++cursor_;
        }

        return Json::array(std::move(array));
    }

    Json parse_number() {
        const auto start = cursor_;

        if (source_[cursor_] == '-') {
            ++cursor_;
        }

        if (cursor_ >= source_.size()) {
            throw std::invalid_argument("Invalid JSON number");
        }

        if (source_[cursor_] == '0') {
            ++cursor_;
        } else {
            if (source_[cursor_] < '1' || source_[cursor_] > '9') {
                throw std::invalid_argument("Invalid JSON number");
            }

            while (
                cursor_ < source_.size() &&
                source_[cursor_] >= '0' &&
                source_[cursor_] <= '9'
            ) {
                ++cursor_;
            }
        }

        bool floating = false;

        if (cursor_ < source_.size() && source_[cursor_] == '.') {
            floating = true;
            ++cursor_;

            const auto fraction_start = cursor_;

            while (
                cursor_ < source_.size() &&
                source_[cursor_] >= '0' &&
                source_[cursor_] <= '9'
            ) {
                ++cursor_;
            }

            if (fraction_start == cursor_) {
                throw std::invalid_argument("Invalid JSON fraction");
            }
        }

        if (
            cursor_ < source_.size() &&
            (source_[cursor_] == 'e' || source_[cursor_] == 'E')
        ) {
            floating = true;
            ++cursor_;

            if (
                cursor_ < source_.size() &&
                (source_[cursor_] == '+' || source_[cursor_] == '-')
            ) {
                ++cursor_;
            }

            const auto exponent_start = cursor_;

            while (
                cursor_ < source_.size() &&
                source_[cursor_] >= '0' &&
                source_[cursor_] <= '9'
            ) {
                ++cursor_;
            }

            if (exponent_start == cursor_) {
                throw std::invalid_argument("Invalid JSON exponent");
            }
        }

        const auto token = source_.substr(start, cursor_ - start);

        if (!floating) {
            Int64 integer = 0;
            const auto result = std::from_chars(
                token.data(),
                token.data() + token.size(),
                integer
            );

            if (
                result.ec == std::errc{} &&
                result.ptr == token.data() + token.size()
            ) {
                return Json{integer};
            }
        }

        Double number = 0.0;
        const auto result = std::from_chars(
            token.data(),
            token.data() + token.size(),
            number
        );

        if (
            result.ec != std::errc{} ||
            result.ptr != token.data() + token.size()
        ) {
            throw std::invalid_argument(
                "JSON number is out of range"
            );
        }

        return Json{number};
    }

    std::string_view source_;
    std::size_t cursor_{0};
};

String dump_json(const Json& value) {
    if (value.is_null()) {
        return "null";
    }

    if (value.is_string()) {
        return "\"" + escape_string(value.string()) + "\"";
    }

    if (value.is_boolean() || value.is_number()) {
        return value.string();
    }

    if (value.is_array()) {
        String output{"["};
        bool first = true;

        for (const auto& item : value.as_array()) {
            if (!first) {
                output += ',';
            }

            first = false;
            output += dump_json(item);
        }

        output += ']';
        return output;
    }

    String output{"{"};
    bool first = true;

    for (const auto& [key, item] : value.as_object()) {
        if (!first) {
            output += ',';
        }

        first = false;
        output += '"';
        output += escape_string(key);
        output += "\":";
        output += dump_json(item);
    }

    output += '}';
    return output;
}

} // namespace

Json::Json() noexcept : storage_(nullptr) {}
Json::Json(std::nullptr_t) noexcept : storage_(nullptr) {}
Json::Json(Boolean value) : storage_(value) {}
Json::Json(Int64 value) : storage_(value) {}
Json::Json(UInt64 value) : storage_(value) {}
Json::Json(Double value) : storage_(value) {}
Json::Json(String value) : storage_(std::move(value)) {}
Json::Json(std::string_view value) : storage_(String{value}) {}
Json::Json(const char* value) : storage_(String{value ? value : ""}) {}

Json Json::array(Array values) {
    Json result;
    result.storage_ = std::make_shared<JsonArrayStorage>(
        JsonArrayStorage{std::move(values)}
    );
    return result;
}

Json Json::object(Object values) {
    Json result;
    result.storage_ = std::make_shared<JsonObjectStorage>(
        JsonObjectStorage{std::move(values)}
    );
    return result;
}

Json Json::parse(std::string_view source) {
    return Parser{source}.parse();
}

bool Json::is_null() const noexcept {
    return std::holds_alternative<std::nullptr_t>(storage_);
}

bool Json::is_boolean() const noexcept {
    return std::holds_alternative<Boolean>(storage_);
}

bool Json::is_integer() const noexcept {
    return
        std::holds_alternative<Int64>(storage_) ||
        std::holds_alternative<UInt64>(storage_);
}

bool Json::is_number() const noexcept {
    return
        is_integer() ||
        std::holds_alternative<Double>(storage_);
}

bool Json::is_string() const noexcept {
    return std::holds_alternative<String>(storage_);
}

bool Json::is_array() const noexcept {
    return std::holds_alternative<
        std::shared_ptr<JsonArrayStorage>
    >(storage_);
}

bool Json::is_object() const noexcept {
    return std::holds_alternative<
        std::shared_ptr<JsonObjectStorage>
    >(storage_);
}

const Json::Array& Json::as_array() const {
    const auto* value = std::get_if<
        std::shared_ptr<JsonArrayStorage>
    >(&storage_);

    if (!value || !*value) {
        throw std::logic_error("JSON value is not an array");
    }

    return (*value)->values;
}

const Json::Object& Json::as_object() const {
    const auto* value = std::get_if<
        std::shared_ptr<JsonObjectStorage>
    >(&storage_);

    if (!value || !*value) {
        throw std::logic_error("JSON value is not an object");
    }

    return (*value)->values;
}

const Json* Json::get(std::string_view key) const noexcept {
    const auto* value = std::get_if<
        std::shared_ptr<JsonObjectStorage>
    >(&storage_);

    if (!value || !*value) {
        return nullptr;
    }

    const auto found = (*value)->values.find(String{key});
    if (found == (*value)->values.end()) {
        return nullptr;
    }

    return &found->second;
}

String Json::string() const {
    if (is_null()) {
        return {};
    }

    if (const auto* value = std::get_if<Boolean>(&storage_)) {
        return *value ? "true" : "false";
    }

    if (const auto* value = std::get_if<Int64>(&storage_)) {
        return std::to_string(*value);
    }

    if (const auto* value = std::get_if<UInt64>(&storage_)) {
        return std::to_string(*value);
    }

    if (const auto* value = std::get_if<Double>(&storage_)) {
        std::ostringstream output;
        output
            << std::setprecision(
                std::numeric_limits<Double>::max_digits10
            )
            << *value;
        return output.str();
    }

    if (const auto* value = std::get_if<String>(&storage_)) {
        return *value;
    }

    return dump();
}

String Json::dump() const {
    return dump_json(*this);
}

Json make_json(const Json& value) {
    return value;
}

Json make_json(Json&& value) {
    return std::move(value);
}

Json make_json(std::nullptr_t) {
    return Json{nullptr};
}

Json make_json(Boolean value) {
    return Json{value};
}

Json make_json(const String& value) {
    return Json{value};
}

Json make_json(String&& value) {
    return Json{std::move(value)};
}

Json make_json(std::string_view value) {
    return Json{value};
}

Json make_json(const char* value) {
    return Json{value};
}

Json make_json(const model::AttributeValue& value) {
    return std::visit(
        [](const auto& item) -> Json {
            using Item = std::remove_cvref_t<decltype(item)>;

            if constexpr (std::same_as<Item, std::monostate>) {
                return Json{nullptr};
            } else {
                return make_json(item);
            }
        },
        value
    );
}

} // namespace gungnir::http
