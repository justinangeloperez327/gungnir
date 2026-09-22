#pragma once

#include <mutex>
#include <vector>

#include <gungnir/mail/transport.hpp>

namespace gungnir::mail {

class MemoryTransport final : public Transport {
public:
    void send(const Message& message) override {
        std::lock_guard lock{mutex_};
        messages_.push_back(message);
    }

    [[nodiscard]] std::vector<Message> messages() const {
        std::lock_guard lock{mutex_};
        return messages_;
    }

private:
    mutable std::mutex mutex_;
    std::vector<Message> messages_;
};

} // namespace gungnir::mail
