#pragma once

#include <mutex>
#include <vector>

#include <gungnir/observability/metrics.hpp>

namespace gungnir::observability {

class MemoryMetricSink final :
    public MetricSink {
public:
    void export_metric(
        const MetricPoint& point
    ) override {
        std::lock_guard lock{
            mutex_
        };

        points_.push_back(point);
    }

    [[nodiscard]]
    std::vector<MetricPoint>
    points()
        const {
        std::lock_guard lock{
            mutex_
        };

        return points_;
    }

    void clear() {
        std::lock_guard lock{
            mutex_
        };

        points_.clear();
    }

private:
    mutable std::mutex mutex_;
    std::vector<MetricPoint>
        points_;
};

} // namespace gungnir::observability
