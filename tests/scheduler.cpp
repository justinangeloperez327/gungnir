#include <cassert>
#include <chrono>
#include <gungnir/scheduler/scheduling.hpp>

class TestClock final : public gungnir::scheduler::Clock {
public:
    TimePoint current{};
    [[nodiscard]] TimePoint now() const override { return current; }
};

int main() {
    using namespace std::chrono_literals;
    TestClock clock;
    gungnir::scheduler::Scheduler scheduler{clock};
    int runs = 0;
    scheduler.every("work", 10s, [&] { ++runs; });
    scheduler.run_due();
    assert(runs == 1);
    scheduler.run_due();
    assert(runs == 1);
    clock.current += 10s;
    scheduler.run_due();
    assert(runs == 2);
}
