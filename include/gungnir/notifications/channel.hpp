#pragma once

#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>

#include <gungnir/notifications/notification.hpp>

namespace gungnir::notifications {

class Channel {
public:
    virtual ~Channel() = default;
    virtual void send(std::string_view recipient, const Notification& notification) = 0;
};

class Manager {
public:
    Manager& channel(std::string name, Channel& channel) {
        channels_.insert_or_assign(std::move(name), &channel);
        return *this;
    }

    void send(std::string_view recipient, const Notification& notification) const {
        for (const auto& name : notification.channels()) {
            const auto found = channels_.find(name);
            if (found == channels_.end() || found->second == nullptr) {
                throw std::logic_error("Gungnir notification channel is not configured: " + name);
            }
            found->second->send(recipient, notification);
        }
    }

private:
    std::unordered_map<std::string, Channel*> channels_;
};

} // namespace gungnir::notifications
