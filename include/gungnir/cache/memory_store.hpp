#pragma once

#include <chrono>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>

#include <gungnir/cache/store.hpp>

namespace gungnir::cache {

class MemoryStore final : public Store {
public:
    [[nodiscard]] std::optional<std::string> get(std::string_view key) override {
        std::lock_guard lock{mutex_};
        const auto found = values_.find(std::string{key});
        if (found == values_.end()) {
            return std::nullopt;
        }
        if (found->second.expires_at && *found->second.expires_at <= std::chrono::steady_clock::now()) {
            values_.erase(found);
            return std::nullopt;
        }
        return found->second.value;
    }

    void put(std::string key, std::string value, std::optional<Duration> ttl = std::nullopt) override {
        std::optional<std::chrono::steady_clock::time_point> expires_at;
        if (ttl) {
            expires_at = std::chrono::steady_clock::now() + *ttl;
        }
        std::lock_guard lock{mutex_};
        values_.insert_or_assign(std::move(key), Entry{std::move(value), expires_at});
    }

    bool forget(std::string_view key) override {
        std::lock_guard lock{mutex_};
        return values_.erase(std::string{key}) != 0;
    }

    void flush() override {
        std::lock_guard lock{mutex_};
        values_.clear();
    }

private:
    struct Entry {
        std::string value;
        std::optional<std::chrono::steady_clock::time_point> expires_at;
    };

    std::mutex mutex_;
    std::unordered_map<std::string, Entry> values_;
};

} // namespace gungnir::cache
