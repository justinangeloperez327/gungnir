#include <gungnir/security/security.hpp>

namespace gungnir::security {

bool constant_time_equal(
    std::string_view left,
    std::string_view right
) noexcept {
    const auto maximum =
        left.size() > right.size() ? left.size() : right.size();

    unsigned char difference =
        static_cast<unsigned char>(left.size() != right.size());

    for (std::size_t index = 0; index < maximum; ++index) {
        const auto lhs = index < left.size()
            ? static_cast<unsigned char>(left[index])
            : static_cast<unsigned char>(0);
        const auto rhs = index < right.size()
            ? static_cast<unsigned char>(right[index])
            : static_cast<unsigned char>(0);
        difference |= static_cast<unsigned char>(lhs ^ rhs);
    }

    return difference == 0;
}

bool valid_header_value(std::string_view value) noexcept {
    for (const auto character : value) {
        if (character == '\r' || character == '\n') {
            return false;
        }
    }
    return true;
}

bool valid_cookie_name(std::string_view value) noexcept {
    if (value.empty()) {
        return false;
    }

    for (const auto character : value) {
        const auto c = static_cast<unsigned char>(character);
        if (c <= 0x20 || c >= 0x7f ||
            character == '(' || character == ')' ||
            character == '<' || character == '>' ||
            character == '@' || character == ',' ||
            character == ';' || character == ':' ||
            character == '\\' || character == '"' ||
            character == '/' || character == '[' ||
            character == ']' || character == '?' ||
            character == '=' || character == '{' ||
            character == '}') {
            return false;
        }
    }
    return true;
}

} // namespace gungnir::security
