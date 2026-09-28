#include <atomic>
#include <cassert>
#include <chrono>
#include <thread>

#include <gungnir/core/task.hpp>
#include <gungnir/core/timer.hpp>

namespace {

using namespace std::chrono_literals;

void wake(
    void* context
) noexcept {
    static_cast<
        std::atomic_int*
    >(context)->fetch_add(
        1
    );
}

gungnir::Task<int> delayed_value(
    std::chrono::milliseconds delay,
    int value
) {
    co_await gungnir::sleep_for(
        delay
    );

    co_return value;
}

template <typename Task>
void wait_until_done(
    const Task& task
) {
    for (
        int attempt = 0;
        attempt < 200 &&
            !task.done();
        ++attempt
    ) {
        std::this_thread::sleep_for(
            5ms
        );
    }

    assert(task.done());
}

} // namespace

int main() {
    using namespace gungnir;

    std::atomic_int wake_count{0};

    const auto token =
        detail::attach_timer_pump(
            &wake_count,
            &wake
        );

    wake_count.store(0);

    auto pumped =
        delayed_value(
            30ms,
            42
        );

    pumped.run_inline();

    assert(!pumped.done());
    assert(
        wake_count.load() >= 1
    );

    std::this_thread::sleep_for(
        60ms
    );

    assert(
        !pumped.done()
    );

    assert(
        detail::timer_poll_timeout(
            100ms
        ) == 0ms
    );

    detail::dispatch_due_timers();

    wait_until_done(
        pumped
    );

    auto pumped_result =
        pumped.operator co_await();

    assert(
        pumped_result.await_resume() ==
        42
    );

    detail::detach_timer_pump(
        token
    );

    auto fallback =
        delayed_value(
            20ms,
            7
        );

    fallback.run_inline();

    assert(!fallback.done());

    wait_until_done(
        fallback
    );

    auto fallback_result =
        fallback.operator co_await();

    assert(
        fallback_result.await_resume() ==
        7
    );

    return 0;
}
