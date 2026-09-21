#pragma once

#include <chrono>
#include <iomanip>
#include <sstream>

#include <gungnir/core/types.hpp>

namespace gungnir::model {

[[nodiscard]] inline String timestamp_now() {
    using namespace std::chrono;

    const auto now = floor<seconds>(system_clock::now());
    const auto day = floor<days>(now);
    const year_month_day date{day};
    const hh_mm_ss time{now - day};

    std::ostringstream output;
    output
        << std::setfill('0')
        << std::setw(4) << static_cast<int>(date.year()) << '-'
        << std::setw(2) << static_cast<unsigned>(date.month()) << '-'
        << std::setw(2) << static_cast<unsigned>(date.day()) << ' '
        << std::setw(2) << time.hours().count() << ':'
        << std::setw(2) << time.minutes().count() << ':'
        << std::setw(2) << time.seconds().count();

    return output.str();
}

} // namespace gungnir::model
