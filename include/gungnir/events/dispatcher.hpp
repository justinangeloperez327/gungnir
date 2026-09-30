#pragma once

#include <algorithm>
#include <cstddef>
#include <functional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

#include <gungnir/events/event.hpp>
#include <gungnir/core/task.hpp>
#include <stdexcept>

namespace gungnir::events {

class Dispatcher {
public:
    using Listener = std::function<void(const Event&)>;
    using ListenerId = std::size_t;
    using AsyncListener = std::function<Task<void>(const Event&)>;
    [[nodiscard]] ListenerId listen_async(std::string event, AsyncListener listener, int priority = 0) { const auto id = next_id_++; listeners_[std::move(event)].push_back(Registered{id,priority,{},std::move(listener)}); return id; }

    [[nodiscard]] ListenerId listen(std::string event, Listener listener, int priority = 0) {
        const auto id = next_id_++;
        listeners_[std::move(event)].push_back(Registered{id, priority, std::move(listener)});
        return id;
    }

    bool forget(std::string_view event, ListenerId id) {
        const auto found = listeners_.find(std::string{event});
        if (found == listeners_.end()) return false;
        auto& values = found->second;
        const auto before = values.size();
        std::erase_if(values, [id](const Registered& value) { return value.id == id; });
        return values.size() != before;
    }

    void dispatch(const Event& event) const {
        const auto found = listeners_.find(std::string{event.name()});
        if (found == listeners_.end()) return;
        auto values = found->second;
        std::stable_sort(values.begin(), values.end(), [](const Registered& left, const Registered& right) {
            return left.priority > right.priority;
        });
        for (const auto& value : values) if (value.async_listener) throw std::logic_error("Use dispatch_async for async listeners");
        for (const auto& value : values) value.listener(event);
    }

    [[nodiscard]] Task<void> dispatch_async(const Event& event) const {
        const auto found = listeners_.find(std::string{event.name()});
        if (found == listeners_.end()) co_return;
        auto values = found->second;
        std::stable_sort(values.begin(),values.end(),[](const Registered& a,const Registered& b){return a.priority > b.priority;});
        for (const auto& value : values) { if (value.async_listener) co_await value.async_listener(event); else value.listener(event); }
    }

    [[nodiscard]] std::size_t listener_count(std::string_view event) const {
        const auto found = listeners_.find(std::string{event});
        return found == listeners_.end() ? 0 : found->second.size();
    }

    void clear() noexcept { listeners_.clear(); }

private:
    struct Registered {
        ListenerId id;
        int priority;
        Listener listener;
        AsyncListener async_listener;
    };

    ListenerId next_id_{1};
    std::unordered_map<std::string, std::vector<Registered>> listeners_;
};

} // namespace gungnir::events

