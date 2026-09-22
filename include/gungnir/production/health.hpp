#pragma once
#include <functional>
#include <string>
#include <utility>
#include <vector>

namespace gungnir::production {

struct HealthCheck {
    std::string name;
    std::function<bool()> check;
};

class Health {
public:
    Health& readiness(std::string name, std::function<bool()> check) {
        readiness_.push_back({std::move(name), std::move(check)});
        return *this;
    }

    [[nodiscard]] bool live() const noexcept { return true; }

    [[nodiscard]] bool ready() const {
        for (const auto& item : readiness_) {
            if (!item.check()) return false;
        }
        return true;
    }

    [[nodiscard]] const std::vector<HealthCheck>& readiness_checks() const noexcept {
        return readiness_;
    }

private:
    std::vector<HealthCheck> readiness_;
};

} // namespace gungnir::production
