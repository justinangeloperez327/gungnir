#pragma once
#include <charconv>
#include <cmath>
#include <stdexcept>
#include <string>
#include <string_view>

namespace gungnir::model {
// Decimal SQL values retain their exact base-ten representation and scale.
// Conversion to a binary float is deliberately explicit.
class Decimal {
public:
    Decimal() : value_("0") {}
    explicit Decimal(std::string_view value) : value_(value) {
        std::size_t i = 0, digits = 0;
        if (i < value.size() && (value[i] == '+' || value[i] == '-')) ++i;
        while (i < value.size() && value[i] >= '0' && value[i] <= '9') { ++i; ++digits; }
        if (i < value.size() && value[i] == '.') {
            ++i;
            while (i < value.size() && value[i] >= '0' && value[i] <= '9') { ++i; ++digits; }
        }
        if (!digits) throw std::invalid_argument("Invalid decimal value");
        if (i < value.size() && (value[i] == 'e' || value[i] == 'E')) {
            ++i;
            if (i < value.size() && (value[i] == '+' || value[i] == '-')) ++i;
            const auto start = i;
            while (i < value.size() && value[i] >= '0' && value[i] <= '9') ++i;
            if (i == start) throw std::invalid_argument("Invalid decimal exponent");
        }
        if (i != value.size()) throw std::invalid_argument("Invalid decimal value");
    }
    [[nodiscard]] const std::string& string() const noexcept { return value_; }
    [[nodiscard]] double to_double() const {
        double result{};
        auto text = std::string_view{value_};
        if (text.starts_with('+')) text.remove_prefix(1);
        auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), result);
        if (error != std::errc{} || end != text.data() + text.size() || !std::isfinite(result))
            throw std::out_of_range("Decimal cannot be represented as a finite double");
        return result;
    }
    bool operator==(const Decimal&) const = default;
private:
    std::string value_;
};
}
