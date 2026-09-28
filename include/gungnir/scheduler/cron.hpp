#pragma once

#include <algorithm>
#include <array>
#include <charconv>
#include <chrono>
#include <cctype>
#include <ctime>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace gungnir::scheduler {

class CronExpression {
public:
    explicit CronExpression(
        std::string expression
    )
        : expression_(
            std::move(expression)
          ) {
        parse();
    }

    [[nodiscard]]
    const std::string& expression()
        const noexcept {
        return expression_;
    }

    [[nodiscard]]
    bool matches_utc(
        std::chrono::system_clock::time_point point
    ) const {
        const auto minute_point =
            std::chrono::floor<
                std::chrono::minutes
            >(point);

        const auto time =
            std::chrono::system_clock::
                to_time_t(minute_point);

        std::tm calendar{};

#ifdef _WIN32
        if (
            gmtime_s(
                &calendar,
                &time
            ) != 0
        ) {
            throw std::runtime_error(
                "Unable to convert cron time to UTC"
            );
        }
#else
        if (
            gmtime_r(
                &time,
                &calendar
            ) == nullptr
        ) {
            throw std::runtime_error(
                "Unable to convert cron time to UTC"
            );
        }
#endif

        if (
            !minute_.contains(
                calendar.tm_min
            ) ||
            !hour_.contains(
                calendar.tm_hour
            ) ||
            !month_.contains(
                calendar.tm_mon + 1
            )
        ) {
            return false;
        }

        const auto day_of_month =
            day_of_month_.contains(
                calendar.tm_mday
            );

        const auto day_of_week =
            day_of_week_.contains(
                calendar.tm_wday
            ) ||
            (
                calendar.tm_wday == 0 &&
                day_of_week_.contains(7)
            );

        if (
            day_of_month_.wildcard &&
            day_of_week_.wildcard
        ) {
            return true;
        }

        if (day_of_month_.wildcard) {
            return day_of_week;
        }

        if (day_of_week_.wildcard) {
            return day_of_month;
        }

        return
            day_of_month ||
            day_of_week;
    }

    [[nodiscard]]
    std::chrono::system_clock::time_point
    next_after_utc(
        std::chrono::system_clock::time_point point
    ) const {
        auto candidate =
            std::chrono::floor<
                std::chrono::minutes
            >(point) +
            std::chrono::minutes{1};

        constexpr auto limit =
            8LL * 366LL * 24LL * 60LL;

        for (
            long long attempt = 0;
            attempt < limit;
            ++attempt
        ) {
            if (matches_utc(candidate)) {
                return candidate;
            }

            candidate +=
                std::chrono::minutes{1};
        }

        throw std::runtime_error(
            "Cron expression has no occurrence within the search horizon"
        );
    }

private:
    struct Field {
        int minimum{0};
        int maximum{0};
        bool wildcard{false};
        std::vector<bool> values;

        [[nodiscard]]
        bool contains(
            int value
        ) const noexcept {
            if (
                value < minimum ||
                value > maximum
            ) {
                return false;
            }

            return values[
                static_cast<std::size_t>(
                    value - minimum
                )
            ];
        }
    };

    static std::string uppercase(
        std::string_view value
    ) {
        std::string result{
            value
        };

        for (auto& character : result) {
            character =
                static_cast<char>(
                    std::toupper(
                        static_cast<
                            unsigned char
                        >(character)
                    )
                );
        }

        return result;
    }

    static int parse_number(
        std::string_view value,
        int minimum,
        int maximum,
        const std::array<
            std::string_view,
            12
        >* names = nullptr,
        bool weekday = false
    ) {
        const auto normalized =
            uppercase(value);

        if (names != nullptr) {
            for (
                std::size_t index = 0;
                index < names->size();
                ++index
            ) {
                if (
                    !(*names)[index].empty() &&
                    normalized ==
                        (*names)[index]
                ) {
                    return
                        minimum +
                        static_cast<int>(
                            index
                        );
                }
            }
        }

        int output = 0;

        const auto parsed =
            std::from_chars(
                normalized.data(),
                normalized.data() +
                    normalized.size(),
                output
            );

        if (
            parsed.ec !=
                std::errc{} ||
            parsed.ptr !=
                normalized.data() +
                    normalized.size()
        ) {
            throw std::invalid_argument(
                "Cron field contains an invalid value: " +
                std::string{value}
            );
        }

        if (
            output < minimum ||
            output > maximum
        ) {
            throw std::invalid_argument(
                "Cron field value is outside its allowed range: " +
                std::string{value}
            );
        }

        return output;
    }

    static int parse_step(
        std::string_view value
    ) {
        if (value.empty()) {
            throw std::invalid_argument(
                "Cron step cannot be empty"
            );
        }

        int step = 0;

        const auto parsed =
            std::from_chars(
                value.data(),
                value.data() +
                    value.size(),
                step
            );

        if (
            parsed.ec !=
                std::errc{} ||
            parsed.ptr !=
                value.data() +
                    value.size() ||
            step <= 0
        ) {
            throw std::invalid_argument(
                "Cron step must be a positive integer"
            );
        }

        return step;
    }

    static void enable_range(
        Field& field,
        int first,
        int last,
        int step
    ) {
        if (first > last) {
            throw std::invalid_argument(
                "Cron ranges must be ascending"
            );
        }

        for (
            int value = first;
            value <= last;
            value += step
        ) {
            field.values[
                static_cast<std::size_t>(
                    value -
                    field.minimum
                )
            ] = true;
        }
    }

    static Field parse_field(
        std::string_view source,
        int minimum,
        int maximum,
        const std::array<
            std::string_view,
            12
        >* names = nullptr,
        bool weekday = false
    ) {
        Field field;
        field.minimum = minimum;
        field.maximum = maximum;
        field.values.resize(
            static_cast<std::size_t>(
                maximum -
                minimum +
                1
            ),
            false
        );

        field.wildcard =
            source == "*";

        std::size_t cursor = 0;

        while (cursor < source.size()) {
            const auto comma =
                source.find(
                    ',',
                    cursor
                );

            const auto end =
                comma ==
                    std::string_view::npos
                ? source.size()
                : comma;

            auto token =
                source.substr(
                    cursor,
                    end - cursor
                );

            if (token.empty()) {
                throw std::invalid_argument(
                    "Cron lists cannot contain empty entries"
                );
            }

            int step = 1;

            const auto slash =
                token.find('/');

            if (
                slash !=
                std::string_view::npos
            ) {
                if (
                    token.find(
                        '/',
                        slash + 1
                    ) !=
                    std::string_view::npos
                ) {
                    throw std::invalid_argument(
                        "Cron field contains multiple step separators"
                    );
                }

                step =
                    parse_step(
                        token.substr(
                            slash + 1
                        )
                    );

                token =
                    token.substr(
                        0,
                        slash
                    );
            }

            if (token == "*") {
                enable_range(
                    field,
                    minimum,
                    maximum,
                    step
                );
            } else {
                const auto dash =
                    token.find('-');

                if (
                    dash ==
                    std::string_view::npos
                ) {
                    const auto value =
                        parse_number(
                            token,
                            minimum,
                            maximum,
                            names,
                            weekday
                        );

                    field.values[
                        static_cast<
                            std::size_t
                        >(
                            value -
                            minimum
                        )
                    ] = true;
                } else {
                    if (
                        token.find(
                            '-',
                            dash + 1
                        ) !=
                        std::string_view::npos
                    ) {
                        throw std::invalid_argument(
                            "Cron field contains multiple range separators"
                        );
                    }

                    const auto first =
                        parse_number(
                            token.substr(
                                0,
                                dash
                            ),
                            minimum,
                            maximum,
                            names,
                            weekday
                        );

                    const auto last =
                        parse_number(
                            token.substr(
                                dash + 1
                            ),
                            minimum,
                            maximum,
                            names,
                            weekday
                        );

                    enable_range(
                        field,
                        first,
                        last,
                        step
                    );
                }
            }

            if (
                comma ==
                std::string_view::npos
            ) {
                break;
            }

            cursor =
                comma + 1;
        }

        if (
            std::none_of(
                field.values.begin(),
                field.values.end(),
                [](bool value) {
                    return value;
                }
            )
        ) {
            throw std::invalid_argument(
                "Cron field does not select any values"
            );
        }

        return field;
    }

    void parse() {
        std::vector<std::string_view>
            fields;

        const std::string_view view{
            expression_
        };

        std::size_t cursor = 0;

        while (cursor < view.size()) {
            while (
                cursor < view.size() &&
                std::isspace(
                    static_cast<
                        unsigned char
                    >(view[cursor])
                )
            ) {
                ++cursor;
            }

            if (cursor >= view.size()) {
                break;
            }

            const auto start =
                cursor;

            while (
                cursor < view.size() &&
                !std::isspace(
                    static_cast<
                        unsigned char
                    >(view[cursor])
                )
            ) {
                ++cursor;
            }

            fields.push_back(
                view.substr(
                    start,
                    cursor - start
                )
            );
        }

        if (fields.size() != 5) {
            throw std::invalid_argument(
                "Cron expressions must contain exactly five fields"
            );
        }

        static constexpr
            std::array<
                std::string_view,
                12
            >
            month_names{
                "JAN",
                "FEB",
                "MAR",
                "APR",
                "MAY",
                "JUN",
                "JUL",
                "AUG",
                "SEP",
                "OCT",
                "NOV",
                "DEC"
            };

        static constexpr
            std::array<
                std::string_view,
                12
            >
            weekday_names{
                "SUN",
                "MON",
                "TUE",
                "WED",
                "THU",
                "FRI",
                "SAT",
                "",
                "",
                "",
                "",
                ""
            };

        minute_ =
            parse_field(
                fields[0],
                0,
                59
            );

        hour_ =
            parse_field(
                fields[1],
                0,
                23
            );

        day_of_month_ =
            parse_field(
                fields[2],
                1,
                31
            );

        month_ =
            parse_field(
                fields[3],
                1,
                12,
                &month_names
            );

        day_of_week_ =
            parse_field(
                fields[4],
                0,
                7,
                &weekday_names,
                true
            );
    }

    std::string expression_;
    Field minute_;
    Field hour_;
    Field day_of_month_;
    Field month_;
    Field day_of_week_;
};

} // namespace gungnir::scheduler
