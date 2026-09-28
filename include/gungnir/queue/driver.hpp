#pragma once

#include <chrono>
#include <optional>
#include <stdexcept>
#include <string_view>
#include <vector>
#include <utility>

#include <gungnir/queue/job.hpp>

namespace gungnir::queue {

class Driver {
public:
    virtual ~Driver() = default;

    virtual void push(
        Envelope job
    ) = 0;

    virtual void push_later(
        Envelope job,
        std::chrono::milliseconds delay
    ) {
        if (delay.count() <= 0) {
            push(
                std::move(job)
            );

            return;
        }

        throw std::logic_error(
            "Queue driver does not support delayed jobs"
        );
    }

    [[nodiscard]]
    virtual std::optional<Envelope>
    pop() = 0;

    virtual void acknowledge(
        const Envelope& job
    ) = 0;

    virtual void release(
        Envelope job
    ) = 0;

    virtual void release_after(
        Envelope job,
        std::chrono::milliseconds delay
    ) {
        if (delay.count() <= 0) {
            release(
                std::move(job)
            );

            return;
        }

        throw std::logic_error(
            "Queue driver does not support delayed release"
        );
    }

    [[nodiscard]]
    virtual bool renew(
        const Envelope&
    ) {
        return false;
    }

    virtual void fail(
        const Envelope& job
    ) = 0;

    [[nodiscard]]
    virtual std::vector<Envelope>
    failed_jobs() {
        return {};
    }

    [[nodiscard]]
    virtual std::optional<Envelope>
    failed_job(
        std::string_view
    ) {
        return std::nullopt;
    }

    virtual bool retry_failed(
        std::string_view
    ) {
        return false;
    }

    virtual bool forget_failed(
        std::string_view
    ) {
        return false;
    }
};

} // namespace gungnir::queue
