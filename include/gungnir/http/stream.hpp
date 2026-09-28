#pragma once

#include <functional>
#include <optional>
#include <string>
#include <utility>

#include <gungnir/core/task.hpp>

namespace gungnir::http {

class BodyStream {
public:
    using Chunk =
        std::optional<std::string>;

    using Producer =
        std::function<
            Task<Chunk>()
        >;

    using SyncProducer =
        std::function<Chunk()>;

    BodyStream() = default;

    explicit BodyStream(
        Producer producer
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
            ]() mutable
                -> Task<Chunk> {
                if (!producer) {
                    co_return
                        std::nullopt;
                }

                co_return producer();
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
    Task<Chunk> next() {
        if (!producer_) {
            co_return
                std::nullopt;
        }

        co_return
            co_await producer_();
    }

private:
    Producer producer_;
};

} // namespace gungnir::http
