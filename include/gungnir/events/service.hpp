#pragma once
#include <concepts>
#include <memory>
#include <gungnir/events/dispatcher.hpp>

namespace gungnir::events {
class Service {
public:
    Service(const std::shared_ptr<Service>& service) : Service(checked(service)) {}
    explicit Service(std::shared_ptr<Dispatcher> dispatcher) : dispatcher_(std::move(dispatcher)) {
        if (!dispatcher_) throw std::invalid_argument("Event dispatcher is not configured");
    }
    template<std::derived_from<Event> T> void dispatch(const T& event) const { dispatcher_->dispatch(event); }
    // Take both owners before creating the lazy coroutine; temporaries and the
    // service that initiated dispatch may disappear before the first resume.
    template<std::derived_from<Event> T> [[nodiscard]] Task<void> dispatch_async(T event) const {
        return dispatch_owned(dispatcher_, std::move(event));
    }
private:
    static const Service& checked(const std::shared_ptr<Service>& service) {
        if (!service) throw std::invalid_argument("Background service is not configured");
        return *service;
    }
    template<class T> static Task<void> dispatch_owned(std::shared_ptr<Dispatcher> dispatcher, T event) {
        co_await dispatcher->dispatch_async(event);
    }
    std::shared_ptr<Dispatcher> dispatcher_;
};
}
