#pragma once

#include <functional>
#include <optional>
#include <string>
#include <utility>

#include <gungnir/core/cancellation.hpp>
#include <gungnir/core/task.hpp>

namespace gungnir::http {

class BodyStream {
public:
    using Chunk =
        std::optional<std::string>;

    // Kept for source compatibility with existing producers.
    using Producer =
        std::function<
            Task<Chunk>()
        >;

    using CancellableProducer =
        std::function<
            Task<Chunk>(
                CancellationToken
            )
        >;

    using SyncProducer =
        std::function<Chunk()>;

    using CancellableSyncProducer =
        std::function<
            Chunk(
                CancellationToken
            )
        >;

    BodyStream() = default;

    explicit BodyStream(
        Producer producer
    )
        : producer_(
            [
                producer =
                    std::move(producer)
            ](
                CancellationToken
            ) mutable
                -> Task<Chunk> {
                if (!producer) {
                    co_return
                        std::nullopt;
                }

                co_return
                    co_await producer();
            }
          ) {}

    explicit BodyStream(
        CancellableProducer producer
    )
        : producer_(
            std::move(producer)
          ) {}

    explicit BodyStream(
        SyncProducer producer
    )
        : producer_(
            [
                producer =
                    std::move(producer)
            ](
                CancellationToken
            ) mutable
                -> Task<Chunk> {
                if (!producer) {
                    co_return
                        std::nullopt;
                }

                co_return producer();
            }
          ) {}

    explicit BodyStream(
        CancellableSyncProducer producer
    )
        : producer_(
            [
                producer =
                    std::move(producer)
            ](
                CancellationToken cancellation
            ) mutable
                -> Task<Chunk> {
                cancellation
                    .throw_if_cancelled();

                if (!producer) {
                    co_return
                        std::nullopt;
                }

                co_return producer(
                    std::move(
                        cancellation
                    )
                );
            }
          ) {}

    [[nodiscard]]
    bool valid()
        const noexcept {
        return
            static_cast<bool>(
                producer_
            );
    }

    [[nodiscard]]
    Task<Chunk> next(
        CancellationToken cancellation = {}
    ) {
        cancellation
            .throw_if_cancelled();

        if (!producer_) {
            co_return
                std::nullopt;
        }

        co_return
            co_await producer_(
                std::move(
                    cancellation
                )
            );
    }

private:
    CancellableProducer producer_;
};

} // namespace gungnir::http
