#pragma once

#include <chrono>
#include <deque>
#include <exception>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <utility>

#include <gungnir/core/cancellation.hpp>
#include <gungnir/core/task.hpp>
#include <gungnir/core/timer.hpp>

namespace gungnir::http {

class RequestBodyStream {
public:
    using Chunk =
        std::optional<std::string>;

    explicit RequestBodyStream(
        CancellationToken cancellation = {}
    )
        : state_(
            std::make_shared<State>()
          ),
          cancellation_(
            std::move(cancellation)
          ) {}

    void feed(
        std::string chunk
    ) {
        if (chunk.empty()) {
            return;
        }

        std::lock_guard lock{
            state_->mutex
        };

        if (state_->closed) {
            throw std::logic_error(
                "Cannot feed a closed request body stream"
            );
        }

        state_->chunks.push_back(
            std::move(chunk)
        );
    }

    void close()
        noexcept {
        std::lock_guard lock{
            state_->mutex
        };

        state_->closed = true;
    }

    void fail(
        std::exception_ptr error
    ) noexcept {
        std::lock_guard lock{
            state_->mutex
        };

        state_->error =
            std::move(error);

        state_->closed = true;
    }

    [[nodiscard]]
    bool closed()
        const noexcept {
        std::lock_guard lock{
            state_->mutex
        };

        return
            state_->closed &&
            state_->chunks.empty();
    }

    [[nodiscard]]
    Task<Chunk> next() {
        while (true) {
            cancellation_
                .throw_if_cancelled();

            {
                std::lock_guard lock{
                    state_->mutex
                };

                if (
                    !state_
                        ->chunks
                        .empty()
                ) {
                    auto chunk =
                        std::move(
                            state_
                                ->chunks
                                .front()
                        );

                    state_->chunks
                        .pop_front();

                    co_return chunk;
                }

                if (state_->error) {
                    std::rethrow_exception(
                        state_->error
                    );
                }

                if (state_->closed) {
                    co_return
                        std::nullopt;
                }
            }

            co_await sleep_for(
                std::chrono::
                    milliseconds{1}
            );
        }
    }

private:
    struct State {
        mutable std::mutex mutex;
        std::deque<std::string>
            chunks;
        std::exception_ptr error;
        bool closed{false};
    };

    std::shared_ptr<State> state_;
    CancellationToken cancellation_;
};

} // namespace gungnir::http
