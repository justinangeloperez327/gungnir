#pragma once

#include <mutex>
#include <vector>

#include <gungnir/observability/trace.hpp>

namespace gungnir::observability {

class MemorySpanSink final :
    public SpanSink {
public:
    void export_span(
        const SpanRecord& span
    ) override {
        std::lock_guard lock{
            mutex_
        };

        spans_.push_back(span);
    }

    [[nodiscard]]
    std::vector<SpanRecord>
    spans()
        const {
        std::lock_guard lock{
            mutex_
        };

        return spans_;
    }

    void clear() {
        std::lock_guard lock{
            mutex_
        };

        spans_.clear();
    }

private:
    mutable std::mutex mutex_;
    std::vector<SpanRecord>
        spans_;
};

} // namespace gungnir::observability
