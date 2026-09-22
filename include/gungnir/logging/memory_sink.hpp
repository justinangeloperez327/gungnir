#pragma once
#include <mutex>
#include <vector>
#include <gungnir/logging/logger.hpp>

namespace gungnir::logging {

class MemorySink final : public Sink {
public:
    void write(const Record& record) override {
        std::scoped_lock lock{mutex_};
        records_.push_back(record);
    }

    [[nodiscard]] std::vector<Record> records() const {
        std::scoped_lock lock{mutex_};
        return records_;
    }

private:
    mutable std::mutex mutex_;
    std::vector<Record> records_;
};

} // namespace gungnir::logging
