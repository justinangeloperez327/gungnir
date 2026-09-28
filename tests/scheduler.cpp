#include <cassert>
#include <chrono>
#include <stdexcept>
#include <string>

#include <gungnir/scheduler/scheduling.hpp>

class TestClock final :
    public gungnir::scheduler::Clock {
public:
    TimePoint current{};

    [[nodiscard]]
    TimePoint now()
        const override {
        return current;
    }
};

namespace {

gungnir::scheduler::Clock::TimePoint
utc(
    int year,
    unsigned month,
    unsigned day,
    int hour,
    int minute
) {
    using namespace std::chrono;

    return
        sys_days{
            std::chrono::year{year} /
            std::chrono::month{month} /
            std::chrono::day{day}
        } +
        hours{hour} +
        minutes{minute};
}

} // namespace

int main() {
    using namespace std::chrono_literals;
    using gungnir::scheduler::CronExpression;
    using gungnir::scheduler::Scheduler;

    TestClock clock;

    Scheduler scheduler{
        clock
    };

    int interval_runs = 0;

    scheduler.every(
        "work",
        10s,
        [&] {
            ++interval_runs;
        }
    );

    assert(
        scheduler.run_due() == 1
    );

    assert(interval_runs == 1);

    assert(
        scheduler.run_due() == 0
    );

    clock.current += 10s;

    assert(
        scheduler.run_due() == 1
    );

    assert(interval_runs == 2);

    CronExpression office{
        "*/15 9-17 * JAN,MAR MON-FRI"
    };

    assert(
        office.matches_utc(
            utc(
                2026,
                1,
                5,
                9,
                30
            )
        )
    );

    assert(
        !office.matches_utc(
            utc(
                2026,
                1,
                5,
                9,
                31
            )
        )
    );

    assert(
        !office.matches_utc(
            utc(
                2026,
                1,
                4,
                9,
                30
            )
        )
    );

    CronExpression sunday{
        "0 0 * * 5-7"
    };

    assert(
        sunday.matches_utc(
            utc(
                2026,
                1,
                4,
                0,
                0
            )
        )
    );

    CronExpression dom_or_dow{
        "0 0 1 * MON"
    };

    assert(
        dom_or_dow.matches_utc(
            utc(
                2026,
                2,
                1,
                0,
                0
            )
        )
    );

    assert(
        dom_or_dow.matches_utc(
            utc(
                2026,
                2,
                2,
                0,
                0
            )
        )
    );

    assert(
        !dom_or_dow.matches_utc(
            utc(
                2026,
                2,
                3,
                0,
                0
            )
        )
    );

    const auto next_quarter =
        office.next_after_utc(
            utc(
                2026,
                1,
                5,
                9,
                31
            )
        );

    assert(
        next_quarter ==
        utc(
            2026,
            1,
            5,
            9,
            45
        )
    );

    bool invalid_rejected = false;

    try {
        CronExpression invalid{
            "0 0 * *"
        };

        static_cast<void>(
            invalid
        );
    } catch (
        const std::invalid_argument&
    ) {
        invalid_rejected = true;
    }

    assert(invalid_rejected);

    TestClock cron_clock;

    cron_clock.current =
        utc(
            2026,
            9,
            28,
            10,
            0
        );

    Scheduler cron_scheduler{
        cron_clock
    };

    int cron_runs = 0;

    cron_scheduler.cron(
        "five-minute",
        "*/5 * * * *",
        [&] {
            ++cron_runs;
        }
    );

    assert(
        cron_scheduler.run_due() == 1
    );

    assert(cron_runs == 1);

    assert(
        cron_scheduler.run_due() == 0
    );

    cron_clock.current +=
        4min;

    assert(
        cron_scheduler.run_due() == 0
    );

    cron_clock.current +=
        1min;

    assert(
        cron_scheduler.run_due() == 1
    );

    assert(cron_runs == 2);

    TestClock convenience_clock;

    convenience_clock.current =
        utc(
            2026,
            9,
            28,
            0,
            0
        );

    Scheduler convenience{
        convenience_clock
    };

    int daily_runs = 0;

    convenience.daily(
        "daily",
        [&] {
            ++daily_runs;
        }
    );

    assert(
        convenience.run_due() == 1
    );

    assert(daily_runs == 1);

    convenience_clock.current +=
        1min;

    assert(
        convenience.run_due() == 0
    );

    bool duplicate_rejected = false;

    try {
        convenience.every(
            "daily",
            1s,
            [] {}
        );
    } catch (
        const std::logic_error&
    ) {
        duplicate_rejected = true;
    }

    assert(duplicate_rejected);

    return 0;
}
