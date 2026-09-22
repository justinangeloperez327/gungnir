#pragma once

#include <chrono>
#include <cstddef>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>

#include <gungnir/http/middleware.hpp>

namespace gungnir::http {

struct CorsOptions {
    std::string allow_origin{"*"};
    std::string allow_methods{"GET, POST, PUT, PATCH, DELETE, OPTIONS"};
    std::string allow_headers{"Content-Type, Authorization, X-CSRF-Token"};
};

[[nodiscard]] MiddlewareHandler cors(CorsOptions options = {});
[[nodiscard]] MiddlewareHandler security_headers();
[[nodiscard]] MiddlewareHandler body_limit(std::size_t bytes);
[[nodiscard]] MiddlewareHandler host_validation(std::unordered_set<std::string> hosts);
[[nodiscard]] MiddlewareHandler request_id();
[[nodiscard]] MiddlewareHandler request_timing();

struct RateLimitOptions {
    std::size_t requests{60};
    std::chrono::seconds window{60};
};

[[nodiscard]] MiddlewareHandler rate_limit(RateLimitOptions options = {});

class Session {
public:
    void put(std::string key, std::string value) { values_.insert_or_assign(std::move(key), std::move(value)); }
    [[nodiscard]] std::string_view get(std::string_view key) const noexcept {
        const auto found = values_.find(std::string{key});
        return found == values_.end() ? std::string_view{} : std::string_view{found->second};
    }
    [[nodiscard]] bool has(std::string_view key) const { return values_.contains(std::string{key}); }
    void forget(std::string_view key) { values_.erase(std::string{key}); }
    void clear() noexcept { values_.clear(); }
private:
    std::unordered_map<std::string, std::string> values_;
};

struct UploadedFile {
    std::string name;
    std::string filename;
    std::string content_type;
    std::string content;
    [[nodiscard]] std::size_t size() const noexcept { return content.size(); }
};

} // namespace gungnir::http
