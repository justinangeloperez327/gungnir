#include <gungnir/production/signal_watcher.hpp>

#include <csignal>
#include <mutex>
#include <stdexcept>
#include <utility>

namespace gungnir::production {

namespace {

using NativeSignalHandler =
    void (*)(int);

std::mutex signal_mutex;

SignalWatcher*
    active_signal_watcher =
        nullptr;

volatile std::sig_atomic_t
    pending_signal = 0;

NativeSignalHandler
    previous_interrupt =
        SIG_DFL;

NativeSignalHandler
    previous_terminate =
        SIG_DFL;

void signal_bridge(
    int signal_number
) noexcept {
    pending_signal =
        static_cast<
            std::sig_atomic_t
        >(signal_number);
}

void restore_signal_handlers(
    SignalWatcher* watcher
) noexcept {
    std::lock_guard lock{
        signal_mutex
    };

    if (
        active_signal_watcher !=
        watcher
    ) {
        return;
    }

    static_cast<void>(
        std::signal(
            SIGINT,
            previous_interrupt
        )
    );

    static_cast<void>(
        std::signal(
            SIGTERM,
            previous_terminate
        )
    );

    pending_signal = 0;

    active_signal_watcher =
        nullptr;
}

} // namespace

SignalWatcher::SignalWatcher(
    Handler handler,
    std::chrono::milliseconds
        poll_interval
)
    : handler_(
        std::move(handler)
      ),
      poll_interval_(
        poll_interval
      ) {
    if (!handler_) {
        throw std::invalid_argument(
            "Signal watcher requires a handler"
        );
    }

    if (
        poll_interval_
            .count() <= 0
    ) {
        throw std::invalid_argument(
            "Signal watcher poll interval must be greater than zero"
        );
    }

    {
        std::lock_guard lock{
            signal_mutex
        };

        if (
            active_signal_watcher !=
            nullptr
        ) {
            throw std::logic_error(
                "Only one Gungnir signal watcher may be active"
            );
        }

        pending_signal = 0;

        previous_interrupt =
            std::signal(
                SIGINT,
                signal_bridge
            );

        if (
            previous_interrupt ==
            SIG_ERR
        ) {
            throw std::runtime_error(
                "Unable to install SIGINT handler"
            );
        }

        previous_terminate =
            std::signal(
                SIGTERM,
                signal_bridge
            );

        if (
            previous_terminate ==
            SIG_ERR
        ) {
            static_cast<void>(
                std::signal(
                    SIGINT,
                    previous_interrupt
                )
            );

            throw std::runtime_error(
                "Unable to install SIGTERM handler"
            );
        }

        active_signal_watcher =
            this;
    }

    try {
        thread_ =
            std::thread{
                [this] {
                    loop();
                }
            };
    } catch (...) {
        restore_signal_handlers(this);
        throw;
    }
}

SignalWatcher::~SignalWatcher() {
    stop();
}

void SignalWatcher::stop()
    noexcept {
    stop_requested_.store(
        true,
        std::memory_order_release
    );

    if (thread_.joinable()) {
        thread_.join();
    }

    restore_signal_handlers(this);
}

int SignalWatcher::last_signal()
    const noexcept {
    return
        last_signal_.load(
            std::memory_order_acquire
        );
}

bool SignalWatcher::running()
    const noexcept {
    return
        running_.load(
            std::memory_order_acquire
        );
}

void SignalWatcher::loop()
    noexcept {
    running_.store(
        true,
        std::memory_order_release
    );

    while (
        !stop_requested_.load(
            std::memory_order_acquire
        )
    ) {
        const auto signal_number =
            static_cast<int>(
                pending_signal
            );

        if (signal_number != 0) {
            pending_signal = 0;

            last_signal_.store(
                signal_number,
                std::memory_order_release
            );

            try {
                handler_(
                    signal_number
                );
            } catch (...) {
            }

            break;
        }

        std::this_thread::sleep_for(
            poll_interval_
        );
    }

    running_.store(
        false,
        std::memory_order_release
    );
}

} // namespace gungnir::production
