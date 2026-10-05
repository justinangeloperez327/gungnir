#pragma once
#include <limits>
#include <gungnir/core/types.hpp>
#include <gungnir/http/json.hpp>
#include <gungnir/queue/dispatcher.hpp>

namespace gungnir::queue {
class Service {
public:
    Service(const std::shared_ptr<Service>& service) : Service(checked(service)) {}
    Service(std::shared_ptr<Dispatcher> dispatcher, std::shared_ptr<Driver> driver)
        : dispatcher_(std::move(dispatcher)), driver_(std::move(driver)) {
        if (!dispatcher_ || !driver_) throw std::invalid_argument("Queue is not configured");
    }
    String dispatch(const Job& job, Int64 attempts = 1, bool after_commit = true) const {
        return dispatcher_->dispatch(job, max_attempts(attempts), after_commit);
    }
    String later(const Job& job, Int64 milliseconds, Int64 attempts = 1, bool after_commit = true) const {
        return dispatcher_->dispatch_later(job, delay(milliseconds), max_attempts(attempts), after_commit);
    }
    [[nodiscard]] http::Json failed() const {
        http::Json::Array result;
        for (const auto& job : driver_->failed_jobs()) result.push_back(http::Json::object({
            {"id", job.id}, {"name", job.name}, {"attempts", static_cast<UInt64>(job.attempts)},
            {"maxAttempts", static_cast<UInt64>(job.max_attempts)}}));
        return http::Json::array(std::move(result));
    }
    bool retry(std::string_view id) const { return driver_->retry_failed(id); }
    bool forget(std::string_view id) const { return driver_->forget_failed(id); }
private:
    static const Service& checked(const std::shared_ptr<Service>& service) {
        if (!service) throw std::invalid_argument("Background service is not configured");
        return *service;
    }
    static unsigned max_attempts(Int64 value) {
        if (value <= 0 || static_cast<UInt64>(value) > std::numeric_limits<unsigned>::max())
            throw std::invalid_argument("Job attempts are out of range");
        return static_cast<unsigned>(value);
    }
    static std::chrono::milliseconds delay(Int64 value) {
        const auto remaining = std::chrono::duration<long double, std::milli>{
            std::chrono::steady_clock::time_point::max() - std::chrono::steady_clock::now()}.count();
        if (value < 0 || static_cast<long double>(value) >= remaining)
            throw std::invalid_argument("Job delay is out of range");
        return std::chrono::milliseconds{value};
    }
    std::shared_ptr<Dispatcher> dispatcher_;
    std::shared_ptr<Driver> driver_;
};
}
