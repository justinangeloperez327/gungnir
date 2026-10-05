#pragma once

#include <memory>
#include <stdexcept>
#include <gungnir/config/repository.hpp>
#include <gungnir/http/json.hpp>

namespace gungnir::config {

// Application code reads the existing repository without borrowing its owner.
// Configure the repository before serving requests or starting workers.
class Service {
public:
    explicit Service(std::shared_ptr<const Repository> repository)
        : repository_(std::move(repository)) {
        if (!repository_) throw std::invalid_argument("Configuration requires a repository");
    }
    Service(const std::shared_ptr<Service>& service) : Service(service ? service->repository_ : nullptr) {}
    [[nodiscard]] bool has(std::string_view key) const { return repository_->has(key); }
    [[nodiscard]] std::optional<http::Json> get(std::string_view key) const {
        const auto value = repository_->find(key);
        if (!value) return std::nullopt;
        return std::visit([](const auto& item) -> http::Json {
            if constexpr (std::same_as<std::remove_cvref_t<decltype(item)>, std::monostate>) return nullptr;
            else return http::make_json(item);
        }, *value);
    }
    [[nodiscard]] String string(std::string_view key, String fallback = {}) const { return repository_->string(key, std::move(fallback)); }
    [[nodiscard]] Int64 integer(std::string_view key, Int64 fallback = 0) const { return repository_->integer(key, fallback); }
    [[nodiscard]] Boolean boolean(std::string_view key, Boolean fallback = false) const { return repository_->boolean(key, fallback); }
    [[nodiscard]] Double number(std::string_view key, Double fallback = 0) const { return repository_->number(key, fallback); }
private:
    std::shared_ptr<const Repository> repository_;
};
}
