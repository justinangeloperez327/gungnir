#pragma once

#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>

namespace gungnir::http {

class SessionData {
public:
    SessionData& put(std::string key, std::string value) {
        values_.insert_or_assign(std::move(key), std::move(value)); return *this;
    }
    [[nodiscard]] std::string_view get(std::string_view key) const noexcept {
        const auto found=values_.find(std::string{key});
        return found==values_.end()?std::string_view{}:std::string_view{found->second};
    }
    [[nodiscard]] bool has(std::string_view key) const { return values_.contains(std::string{key}); }
    void forget(std::string_view key){values_.erase(std::string{key});}
    void flush() noexcept {values_.clear();}
private:
    std::unordered_map<std::string,std::string> values_;
};

class MemorySessionStore {
public:
    [[nodiscard]] SessionData load(std::string_view id) const {
        std::lock_guard lock{mutex_}; const auto found=sessions_.find(std::string{id});
        return found==sessions_.end()?SessionData{}:found->second;
    }
    void save(std::string id, SessionData session) {
        std::lock_guard lock{mutex_}; sessions_.insert_or_assign(std::move(id),std::move(session));
    }
    void destroy(std::string_view id){std::lock_guard lock{mutex_};sessions_.erase(std::string{id});}
private:
    mutable std::mutex mutex_;
    std::unordered_map<std::string,SessionData> sessions_;
};

} // namespace gungnir::http
