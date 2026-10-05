#pragma once
#include <concepts>
#include <memory>
#include <gungnir/core/types.hpp>
#include <gungnir/queue/dispatcher.hpp>
#include <gungnir/scheduler/iana_timezone.hpp>
#include <gungnir/scheduler/scheduler.hpp>

namespace gungnir::scheduler {
namespace detail {
struct ServiceState {
    std::shared_ptr<Scheduler> scheduler;
    std::shared_ptr<queue::Dispatcher> queue;
    std::shared_ptr<LockStore> locks;
    void configure() const {
        if (scheduler->running()) throw std::logic_error("Configure scheduled tasks before starting the runner");
    }
};
inline std::chrono::milliseconds duration(Int64 value) {
    if (value <= 0) throw std::invalid_argument("Schedule duration must be positive");
    const auto remaining = std::chrono::duration<long double, std::milli>{
        Clock::TimePoint::max() - std::chrono::system_clock::now()}.count();
    if (static_cast<long double>(value) >= remaining) throw std::invalid_argument("Schedule duration is out of range");
    return std::chrono::milliseconds{value};
}
}
class Entry {
public:
    Entry(std::shared_ptr<detail::ServiceState> owner, Task& task) : owner_(std::move(owner)), task_(&task) {}
    Entry timezone(String name) const {
        owner_->configure();
        if (name == "UTC") task_->timezone(UtcTimeZone::instance());
        else {
            auto zone = std::make_shared<IanaTimeZone>(std::move(name));
            task_->timezone(std::move(zone));
        }
        return *this;
    }
    Entry withoutOverlapping(Int64 ttl = 86400000) const {
        require_locks(); task_->without_overlapping(detail::duration(ttl)); return *this;
    }
    Entry onOneServer(Int64 ttl = 86400000) const {
        require_locks(); task_->on_one_server(detail::duration(ttl)); return *this;
    }
private:
    void require_locks() const {
        owner_->configure();
        if (!owner_->scheduler->lock_store()) throw std::logic_error("Configure scheduler_locks in bootstrap before using schedule locks");
    }
    std::shared_ptr<detail::ServiceState> owner_;
    Task* task_;
};
class Service {
public:
    Service(const std::shared_ptr<Service>& service) : Service(checked(service)) {}
    Service(std::shared_ptr<Scheduler> scheduler, std::shared_ptr<queue::Dispatcher> queue = {}, std::shared_ptr<LockStore> locks = {})
        : owner_(std::make_shared<detail::ServiceState>(detail::ServiceState{std::move(scheduler), std::move(queue), std::move(locks)})) {
        if (!owner_->scheduler) throw std::invalid_argument("Scheduler is not configured");
        if (owner_->locks) owner_->scheduler->locks(*owner_->locks);
    }
    template<class Action> Entry every(String name, Int64 milliseconds, Action action) const {
        return {owner_, owner_->scheduler->every(std::move(name), detail::duration(milliseconds), adapt(std::move(action)))};
    }
    template<class Action> Entry cron(String name, String expression, Action action) const {
        return {owner_, owner_->scheduler->cron(std::move(name), std::move(expression), adapt(std::move(action)))};
    }
    template<class Action> Entry hourly(String name, Action action) const { return cron(std::move(name), "0 * * * *", std::move(action)); }
    template<class Action> Entry daily(String name, Action action) const { return cron(std::move(name), "0 0 * * *", std::move(action)); }
    template<class Action> Entry weekly(String name, Action action) const { return cron(std::move(name), "0 0 * * 0", std::move(action)); }
    template<class Action> Entry monthly(String name, Action action) const { return cron(std::move(name), "0 0 1 * *", std::move(action)); }
private:
    static const Service& checked(const std::shared_ptr<Service>& service) {
        if (!service) throw std::invalid_argument("Scheduler service is not configured");
        return *service;
    }
    template<class Action> Task::Action adapt(Action action) const {
        owner_->configure();
        if constexpr (std::derived_from<Action, queue::Job>) {
            auto dispatcher = owner_->queue;
            if (!dispatcher) throw std::logic_error("Scheduled jobs require a queue driver");
            return [dispatcher = std::move(dispatcher), job = std::move(action)] { dispatcher->dispatch(job); };
        } else {
            static_assert(std::same_as<std::invoke_result_t<Action&>, void>, "Schedule callbacks must return void");
            return action;
        }
    }
    std::shared_ptr<detail::ServiceState> owner_;
};
}
