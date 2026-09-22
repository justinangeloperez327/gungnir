#pragma once

#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>

namespace gungnir::session {

class Session {
public:
    explicit Session(std::string id = {}) : id_(std::move(id)) {}

    [[nodiscard]] std::string_view id() const noexcept { return id_; }
    void id(std::string value) { id_ = std::move(value); }

    void put(std::string key, std::string value) {
        values_.insert_or_assign(std::move(key), std::move(value));
    }

    [[nodiscard]] std::string_view get(std::string_view key) const noexcept {
        const auto found = values_.find(std::string{key});
        return found == values_.end() ? std::string_view{} : std::string_view{found->second};
    }

    [[nodiscard]] bool has(std::string_view key) const {
        return values_.contains(std::string{key});
    }

    void forget(std::string_view key) { values_.erase(std::string{key}); }
    void clear() noexcept { values_.clear(); }

    void flash(std::string key, std::string value) {
        flash_next_.insert_or_assign(std::move(key), std::move(value));
    }

    [[nodiscard]] std::string_view flashed(std::string_view key) const noexcept {
        const auto found = flash_current_.find(std::string{key});
        return found == flash_current_.end() ? std::string_view{} : std::string_view{found->second};
    }

    void age_flash() {
        flash_current_ = std::move(flash_next_);
        flash_next_.clear();
    }

    void regenerate(std::string new_id) {
        id_ = std::move(new_id);
        regenerated_ = true;
    }

    void invalidate(std::string new_id = {}) {
        clear();
        flash_current_.clear();
        flash_next_.clear();
        id_ = std::move(new_id);
        regenerated_ = true;
    }

    [[nodiscard]] bool regenerated() const noexcept { return regenerated_; }
    void mark_persisted() noexcept { regenerated_ = false; }

    [[nodiscard]] const std::unordered_map<std::string, std::string>& values() const noexcept {
        return values_;
    }

private:
    std::string id_;
    std::unordered_map<std::string, std::string> values_;
    std::unordered_map<std::string, std::string> flash_current_;
    std::unordered_map<std::string, std::string> flash_next_;
    bool regenerated_{false};
};

} // namespace gungnir::session
