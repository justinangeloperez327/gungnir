#pragma once
#include <memory>
#include <gungnir/queue/driver.hpp>
#include <gungnir/database/runtime.hpp>
#include <gungnir/security/random.hpp>

namespace gungnir::queue {
class Dispatcher {
public:
    explicit Dispatcher(std::shared_ptr<Driver> driver) : driver_(std::move(driver)) {
        if (!driver_) throw std::invalid_argument("Queue dispatcher requires a driver");
    }
    std::string dispatch(const Job& job, unsigned max_attempts = 1, bool after_commit = true) const {
        return dispatch_later(job, std::chrono::milliseconds::zero(), max_attempts, after_commit);
    }
    std::string dispatch_later(const Job& job, std::chrono::milliseconds delay, unsigned max_attempts = 1, bool after_commit = true) const {
        if (delay.count() < 0) throw std::invalid_argument("Job delay must not be negative");
        if (!max_attempts) throw std::invalid_argument("Job attempts must be positive");
        Envelope envelope{.id = security::random_token(), .name = std::string{job.name()},
            .payload = job.payload(), .max_attempts = max_attempts};
        inherit_trace(envelope);
        auto id = envelope.id;
        auto publish = [driver = driver_, envelope = std::move(envelope), delay]() mutable { driver->push_later(std::move(envelope), delay); };
        if (auto connection = database::runtime::current(); after_commit && connection && connection->in_transaction())
            connection->after_commit(std::move(publish));
        else publish();
        return id;
    }
private:
    std::shared_ptr<Driver> driver_;
};
}
