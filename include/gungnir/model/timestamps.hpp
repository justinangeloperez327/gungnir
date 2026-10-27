#pragma once

#include <chrono>
#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <string_view>
#include <cctype>

#include <gungnir/core/types.hpp>
#include <gungnir/model/value.hpp>

namespace gungnir::model {

[[nodiscard]] inline String timestamp_now(std::string_view format = "Y-m-d H:i:s") {
    using namespace std::chrono;
    const auto now = floor<microseconds>(system_clock::now());
    const auto day = floor<days>(now);
    const year_month_day date{day};
    const hh_mm_ss time{now - day};
    std::ostringstream output;
    output << std::setfill('0');
    bool escaped = false;
    for (const auto c : format) {
        if (escaped) { output << c; escaped = false; continue; }
        if (c == '\\') { escaped = true; continue; }
        switch (c) {
        case 'Y': output << std::setw(4) << static_cast<int>(date.year()); break;
        case 'y': output << std::setw(2) << static_cast<int>(date.year()) % 100; break;
        case 'm': output << std::setw(2) << static_cast<unsigned>(date.month()); break;
        case 'd': output << std::setw(2) << static_cast<unsigned>(date.day()); break;
        case 'H': output << std::setw(2) << time.hours().count(); break;
        case 'i': output << std::setw(2) << time.minutes().count(); break;
        case 's': output << std::setw(2) << time.seconds().count(); break;
        case 'u': output << std::setw(6) << time.subseconds().count(); break;
        case 'v': output << std::setw(3) << time.subseconds().count() / 1000; break;
        case 'U': output << floor<seconds>(now.time_since_epoch()).count(); break;
        default:
            if (std::isalpha(static_cast<unsigned char>(c)))
                throw std::invalid_argument("Unsupported model dateFormat token");
            output << c;
        }
    }
    if (escaped || format.empty()) throw std::invalid_argument("Invalid model dateFormat");
    return output.str();
}

template <typename Model>
[[nodiscard]] AttributeMap soft_delete_attributes(bool restore) {
    const auto now = timestamp_now(Model::timestamp_format());
    AttributeMap values{{String{Model::deleted_at_column()},restore ? AttributeValue{nullptr} : AttributeValue{now}}};
    if constexpr (Model::uses_timestamps())
        if (Model::has_attribute(Model::updated_at_column())) values.emplace(String{Model::updated_at_column()},now);
    return values;
}

} // namespace gungnir::model
