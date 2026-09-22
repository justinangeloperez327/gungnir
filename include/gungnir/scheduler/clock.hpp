#pragma once
#include <chrono>

namespace gungnir::scheduler {

class Clock {
public:
    using TimePoint = std::chrono::system_clock::time_point;
    virtual ~Clock() = default;
    [[nodiscard]] virtual TimePoint now() const = 0;
};

class SystemClock final : public Clock {
public:
    [[nodiscard]] TimePoint now() const override { return std::chrono::system_clock::now(); }
};

} // namespace gungnir::scheduler
