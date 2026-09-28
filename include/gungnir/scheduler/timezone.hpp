#pragma once

#include <chrono>
#include <stdexcept>
#include <string>
#include <utility>

#include <gungnir/scheduler/clock.hpp>

namespace gungnir::scheduler {

struct LocalDateTime {
    int year{1970};
    unsigned month{1};
    unsigned day{1};
    int hour{0};
    int minute{0};
    int weekday{4};
    int offset_minutes{0};
    bool daylight{false};
};

struct WallMinute {
    int year{1970};
    unsigned month{1};
    unsigned day{1};
    int hour{0};
    int minute{0};

    [[nodiscard]]
    friend bool operator==(
        const WallMinute&,
        const WallMinute&
    ) = default;
};

[[nodiscard]]
inline WallMinute wall_minute(
    const LocalDateTime& value
) noexcept {
    return {
        value.year,
        value.month,
        value.day,
        value.hour,
        value.minute
    };
}

class TimeZone {
public:
    virtual ~TimeZone() = default;

    [[nodiscard]]
    virtual const std::string& name()
        const noexcept = 0;

    [[nodiscard]]
    virtual LocalDateTime to_local(
        Clock::TimePoint point
    ) const = 0;
};

namespace detail {

[[nodiscard]]
inline LocalDateTime local_from_offset(
    Clock::TimePoint point,
    std::chrono::minutes offset,
    bool daylight
) {
    using namespace std::chrono;

    const auto local =
        point + offset;

    const auto local_days =
        floor<days>(local);

    const year_month_day date{
        local_days
    };

    const hh_mm_ss time{
        floor<seconds>(local - local_days)
    };

    const weekday week_day{
        local_days
    };

    return {
        static_cast<int>(
            date.year()
        ),
        static_cast<unsigned>(
            date.month()
        ),
        static_cast<unsigned>(
            date.day()
        ),
        static_cast<int>(
            time.hours().count()
        ),
        static_cast<int>(
            time.minutes().count()
        ),
        static_cast<int>(
            week_day.c_encoding()
        ),
        static_cast<int>(
            offset.count()
        ),
        daylight
    };
}

} // namespace detail

class UtcTimeZone final :
    public TimeZone {
public:
    [[nodiscard]]
    static const UtcTimeZone& instance()
        noexcept {
        static UtcTimeZone zone;
        return zone;
    }

    [[nodiscard]]
    const std::string& name()
        const noexcept override {
        return name_;
    }

    [[nodiscard]]
    LocalDateTime to_local(
        Clock::TimePoint point
    ) const override {
        return
            detail::local_from_offset(
                point,
                std::chrono::minutes{0},
                false
            );
    }

private:
    UtcTimeZone() = default;

    std::string name_{"UTC"};
};

class FixedOffsetTimeZone final :
    public TimeZone {
public:
    FixedOffsetTimeZone(
        std::string name,
        std::chrono::minutes offset
    )
        : name_(
            std::move(name)
          ),
          offset_(offset) {
        if (name_.empty()) {
            throw std::invalid_argument(
                "Timezone name must not be empty"
            );
        }

        validate_offset(offset_);
    }

    [[nodiscard]]
    const std::string& name()
        const noexcept override {
        return name_;
    }

    [[nodiscard]]
    LocalDateTime to_local(
        Clock::TimePoint point
    ) const override {
        return
            detail::local_from_offset(
                point,
                offset_,
                false
            );
    }

    [[nodiscard]]
    std::chrono::minutes offset()
        const noexcept {
        return offset_;
    }

private:
    static void validate_offset(
        std::chrono::minutes offset
    ) {
        constexpr auto maximum =
            std::chrono::hours{24};

        if (
            offset <= -maximum ||
            offset >= maximum
        ) {
            throw std::invalid_argument(
                "Timezone offset must be between -24 and +24 hours"
            );
        }
    }

    std::string name_;
    std::chrono::minutes offset_;
};

struct TransitionRule {
    unsigned month{1};
    unsigned occurrence{1};
    unsigned weekday{0};
    std::chrono::minutes at{0};
};

class RecurringTimeZone final :
    public TimeZone {
public:
    RecurringTimeZone(
        std::string name,
        std::chrono::minutes standard_offset,
        std::chrono::minutes daylight_offset,
        TransitionRule daylight_start,
        TransitionRule daylight_end
    )
        : name_(
            std::move(name)
          ),
          standard_offset_(
            standard_offset
          ),
          daylight_offset_(
            daylight_offset
          ),
          daylight_start_(
            daylight_start
          ),
          daylight_end_(
            daylight_end
          ) {
        if (name_.empty()) {
            throw std::invalid_argument(
                "Timezone name must not be empty"
            );
        }

        validate_offset(
            standard_offset_
        );

        validate_offset(
            daylight_offset_
        );

        validate_rule(
            daylight_start_
        );

        validate_rule(
            daylight_end_
        );
    }

    [[nodiscard]]
    const std::string& name()
        const noexcept override {
        return name_;
    }

    [[nodiscard]]
    LocalDateTime to_local(
        Clock::TimePoint point
    ) const override {
        const auto daylight =
            daylight_at(point);

        return
            detail::local_from_offset(
                point,
                daylight
                    ? daylight_offset_
                    : standard_offset_,
                daylight
            );
    }

    [[nodiscard]]
    std::chrono::minutes standard_offset()
        const noexcept {
        return standard_offset_;
    }

    [[nodiscard]]
    std::chrono::minutes daylight_offset()
        const noexcept {
        return daylight_offset_;
    }

private:
    static void validate_offset(
        std::chrono::minutes offset
    ) {
        constexpr auto maximum =
            std::chrono::hours{24};

        if (
            offset <= -maximum ||
            offset >= maximum
        ) {
            throw std::invalid_argument(
                "Timezone offset must be between -24 and +24 hours"
            );
        }
    }

    static void validate_rule(
        const TransitionRule& rule
    ) {
        if (
            rule.month < 1 ||
            rule.month > 12
        ) {
            throw std::invalid_argument(
                "Timezone transition month must be between 1 and 12"
            );
        }

        if (
            rule.occurrence < 1 ||
            rule.occurrence > 5
        ) {
            throw std::invalid_argument(
                "Timezone transition occurrence must be between 1 and 5"
            );
        }

        if (rule.weekday > 6) {
            throw std::invalid_argument(
                "Timezone transition weekday must be between 0 and 6"
            );
        }

        if (
            rule.at <
                std::chrono::minutes{0} ||
            rule.at >=
                std::chrono::hours{24}
        ) {
            throw std::invalid_argument(
                "Timezone transition time must be within one local day"
            );
        }
    }

    [[nodiscard]]
    static std::chrono::sys_days
    transition_day(
        std::chrono::year year,
        const TransitionRule& rule
    ) {
        using namespace std::chrono;

        const auto month =
            std::chrono::month{
                rule.month
            };

        if (rule.occurrence == 5) {
            const year_month_day last{
                year /
                month /
                std::chrono::last
            };

            const auto last_day =
                sys_days{
                    last
                };

            const weekday actual{
                last_day
            };

            const auto delta =
                (
                    actual.c_encoding() +
                    7 -
                    rule.weekday
                ) %
                7;

            return
                last_day -
                days{
                    delta
                };
        }

        const auto first_day =
            sys_days{
                year /
                month /
                std::chrono::day{1}
            };

        const weekday first_weekday{
            first_day
        };

        const auto delta =
            (
                rule.weekday +
                7 -
                first_weekday.c_encoding()
            ) %
            7;

        const auto day_offset =
            delta +
            7 *
            (
                rule.occurrence -
                1
            );

        const auto candidate =
            first_day +
            days{
                day_offset
            };

        const year_month_day date{
            candidate
        };

        if (date.month() != month) {
            throw std::invalid_argument(
                "Timezone transition occurrence does not exist in the selected month"
            );
        }

        return candidate;
    }

    [[nodiscard]]
    static Clock::TimePoint
    transition_instant(
        std::chrono::year year,
        const TransitionRule& rule,
        std::chrono::minutes offset_before
    ) {
        return
            transition_day(
                year,
                rule
            ) +
            rule.at -
            offset_before;
    }

    [[nodiscard]]
    bool daylight_at(
        Clock::TimePoint point
    ) const {
        using namespace std::chrono;

        const auto standard_local =
            point +
            standard_offset_;

        const year_month_day date{
            floor<days>(
                standard_local
            )
        };

        const auto year =
            date.year();

        const auto start =
            transition_instant(
                year,
                daylight_start_,
                standard_offset_
            );

        const auto end =
            transition_instant(
                year,
                daylight_end_,
                daylight_offset_
            );

        if (start < end) {
            return
                point >= start &&
                point < end;
        }

        return
            point >= start ||
            point < end;
    }

    std::string name_;
    std::chrono::minutes
        standard_offset_;
    std::chrono::minutes
        daylight_offset_;
    TransitionRule daylight_start_;
    TransitionRule daylight_end_;
};

} // namespace gungnir::scheduler
