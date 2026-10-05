#pragma once

#include <stdexcept>
#include <memory>
#include <vector>
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
    Manager& channel(std::string name, std::shared_ptr<Channel> channel) {
        if (!channel) throw std::invalid_argument("Notification channel cannot be null");
        channels_.insert_or_assign(std::move(name), channel.get());
        owners_.push_back(std::move(channel)); return *this;
    }
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

    [[nodiscard]] bool has_channel(std::string_view name) const {
        const auto found = channels_.find(std::string{name});
        return found != channels_.end() && found->second != nullptr;
    }
    void send_on(std::string_view name, std::string_view recipient, const Notification& notification) const {
        const auto found = channels_.find(std::string{name});
        if (found == channels_.end() || found->second == nullptr)
            throw std::logic_error("Gungnir notification channel is not configured: " + std::string{name});
        found->second->send(recipient, notification);
    }

private:
    std::unordered_map<std::string, Channel*> channels_;
    std::vector<std::shared_ptr<Channel>> owners_;
};

} // namespace gungnir::notifications
