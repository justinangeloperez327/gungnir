#pragma once

#include <atomic>
#include <chrono>
#include <functional>
#include <thread>

namespace gungnir::production {

class SignalWatcher {
public:
    using Handler =
        std::function<void(int)>;

    explicit SignalWatcher(
        Handler handler,
        std::chrono::milliseconds
            poll_interval =
                std::chrono::milliseconds{10}
    );

    ~SignalWatcher();

    SignalWatcher(
        const SignalWatcher&
    ) = delete;

    SignalWatcher& operator=(
        const SignalWatcher&
    ) = delete;

    void stop()
        noexcept;

    [[nodiscard]]
    int last_signal()
        const noexcept;

    [[nodiscard]]
    bool running()
        const noexcept;

private:
    void loop()
        noexcept;

    Handler handler_;

    std::chrono::milliseconds
        poll_interval_;

    std::atomic_bool stop_requested_{
        false
    };

    std::atomic_bool running_{
        false
    };

    std::atomic_int last_signal_{
        0
    };

    std::thread thread_;
};

} // namespace gungnir::production
