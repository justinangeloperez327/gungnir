#pragma once

#include <functional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>

#include <gungnir/queue/driver.hpp>

namespace gungnir::queue {

class Worker {
public:
    using Handler = std::function<void(std::string_view)>;

    explicit Worker(Driver& driver) : driver_(&driver) {}

    Worker& handle(std::string name, Handler handler) {
        handlers_.insert_or_assign(std::move(name), std::move(handler));
        return *this;
    }

    [[nodiscard]] bool run_one() {
        auto job = driver_->pop();
        if (!job) return false;

        const auto found = handlers_.find(job->name);
        if (found == handlers_.end()) {
            driver_->fail(*job);
            return true;
        }

        ++job->attempts;
        try {
            found->second(job->payload);
            driver_->acknowledge(*job);
        } catch (...) {
            if (job->attempts < job->max_attempts) {
                driver_->release(std::move(*job));
            } else {
                driver_->fail(*job);
            }
        }
        return true;
    }

private:
    Driver* driver_;
    std::unordered_map<std::string, Handler> handlers_;
};

} // namespace gungnir::queue
