#pragma once

#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>

#include <gungnir/session/store.hpp>

namespace gungnir::session {

class MemoryStore final : public Store {
public:
    [[nodiscard]] std::optional<Session> load(std::string_view id) override {
        std::lock_guard lock{mutex_};
        const auto found = sessions_.find(std::string{id});
        return found == sessions_.end() ? std::nullopt : std::optional<Session>{found->second};
    }

    void save(const Session& session) override {
        std::lock_guard lock{mutex_};
        sessions_.insert_or_assign(std::string{session.id()}, session);
    }

    void erase(std::string_view id) override {
        std::lock_guard lock{mutex_};
        sessions_.erase(std::string{id});
    }

private:
    std::mutex mutex_;
    std::unordered_map<std::string, Session> sessions_;
};

} // namespace gungnir::session
