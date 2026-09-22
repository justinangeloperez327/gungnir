#pragma once

#include <optional>

#include <gungnir/queue/job.hpp>

namespace gungnir::queue {

class Driver {
public:
    virtual ~Driver() = default;
    virtual void push(Envelope job) = 0;
    [[nodiscard]] virtual std::optional<Envelope> pop() = 0;
    virtual void acknowledge(const Envelope& job) = 0;
    virtual void release(Envelope job) = 0;
    virtual void fail(const Envelope& job) = 0;
};

} // namespace gungnir::queue
