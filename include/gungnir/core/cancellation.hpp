#pragma once

#include <atomic>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <unordered_map>
#include <utility>
#include <vector>

namespace gungnir {

class OperationCancelled :
    public std::runtime_error {
public:
    OperationCancelled()
        : std::runtime_error(
            "Operation cancelled"
          ) {}
};

namespace detail {

struct CancellationState {
    std::atomic_bool cancelled{false};
    std::mutex mutex;
    std::unordered_map<
        std::uint64_t,
        std::function<void()>
    > callbacks;
    std::uint64_t next_id{1};
};

} // namespace detail

class CancellationRegistration {
public:
    CancellationRegistration() noexcept =
        default;

    CancellationRegistration(
        const CancellationRegistration&
    ) = delete;

    CancellationRegistration& operator=(
        const CancellationRegistration&
    ) = delete;

    CancellationRegistration(
        CancellationRegistration&& other
    ) noexcept
        : state_(
            std::move(
                other.state_
            )
          ),
          id_(
            std::exchange(
                other.id_,
                0
            )
          ) {}

    CancellationRegistration& operator=(
        CancellationRegistration&& other
    ) noexcept {
        if (this == &other) {
            return *this;
        }

        reset();

        state_ =
            std::move(
                other.state_
            );

        id_ =
            std::exchange(
                other.id_,
                0
            );

        return *this;
    }

    ~CancellationRegistration() {
        reset();
    }

    void reset() noexcept {
        if (id_ == 0) {
            return;
        }

        if (
            const auto state =
                state_.lock()
        ) {
            std::lock_guard lock{
                state->mutex
            };

            state->callbacks.erase(
                id_
            );
        }

        state_.reset();
        id_ = 0;
    }

    [[nodiscard]]
    explicit operator bool()
        const noexcept {
        return id_ != 0;
    }

private:
    friend class CancellationToken;

    CancellationRegistration(
        std::weak_ptr<
            detail::CancellationState
        > state,
        std::uint64_t id
    ) noexcept
        : state_(
            std::move(state)
          ),
          id_(id) {}

    std::weak_ptr<
        detail::CancellationState
    > state_;
    std::uint64_t id_{0};
};

class CancellationToken {
public:
    CancellationToken()
        : state_(
            std::make_shared<
                detail::CancellationState
            >()
          ) {}

    [[nodiscard]]
    bool cancelled()
        const noexcept {
        return
            state_->cancelled.load(
                std::memory_order_acquire
            );
    }

    void throw_if_cancelled()
        const {
        if (cancelled()) {
            throw OperationCancelled{};
        }
    }

    [[nodiscard]]
    CancellationRegistration
    on_cancel(
        std::function<void()> callback
    ) const {
        if (!callback) {
            return {};
        }

        bool invoke_now = false;
        std::uint64_t id = 0;

        {
            std::lock_guard lock{
                state_->mutex
            };

            if (
                state_->cancelled.load(
                    std::memory_order_acquire
                )
            ) {
                invoke_now = true;
            } else {
                id =
                    state_->next_id++;

                if (id == 0) {
                    id =
                        state_->next_id++;
                }

                state_->callbacks.emplace(
                    id,
                    std::move(callback)
                );
            }
        }

        if (invoke_now) {
            try {
                callback();
            } catch (...) {
            }

            return {};
        }

        return CancellationRegistration{
            state_,
            id
        };
    }

private:
    friend class CancellationSource;

    explicit CancellationToken(
        std::shared_ptr<
            detail::CancellationState
        > state
    )
        : state_(
            std::move(state)
          ) {}

    std::shared_ptr<
        detail::CancellationState
    > state_;
};

class CancellationSource {
public:
    CancellationSource()
        : state_(
            std::make_shared<
                detail::CancellationState
            >()
          ) {}

    [[nodiscard]]
    CancellationToken token()
        const noexcept {
        return CancellationToken{
            state_
        };
    }

    void cancel() noexcept {
        bool expected = false;

        if (
            !state_->cancelled
                .compare_exchange_strong(
                    expected,
                    true,
                    std::memory_order_acq_rel
                )
        ) {
            return;
        }

        std::vector<
            std::function<void()>
        > callbacks;

        {
            std::lock_guard lock{
                state_->mutex
            };

            callbacks.reserve(
                state_->callbacks.size()
            );

            for (
                auto& [id, callback] :
                state_->callbacks
            ) {
                static_cast<void>(id);

                callbacks.push_back(
                    std::move(callback)
                );
            }

            state_->callbacks.clear();
        }

        for (
            auto& callback :
            callbacks
        ) {
            try {
                callback();
            } catch (...) {
            }
        }
    }

private:
    std::shared_ptr<
        detail::CancellationState
    > state_;
};

} // namespace gungnir
