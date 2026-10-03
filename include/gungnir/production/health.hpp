#pragma once

#include <functional>
#include <stdexcept>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

namespace gungnir::production {

struct HealthCheck {
    std::string name;
    std::function<bool()> check;
};

struct HealthCheckResult {
    std::string name;
    bool healthy{false};
};

struct HealthReport {
    bool healthy{true};
    std::vector<HealthCheckResult> checks;
};

class Health {
public:
    Health& liveness(
        std::string name,
        std::function<bool()> check
    ) {
        register_check(
            liveness_,
            liveness_names_,
            std::move(name),
            std::move(check),
            "liveness"
        );

        return *this;
    }

    Health& readiness(
        std::string name,
        std::function<bool()> check
    ) {
        register_check(
            readiness_,
            readiness_names_,
            std::move(name),
            std::move(check),
            "readiness"
        );

        return *this;
    }

    [[nodiscard]]
    bool live() const noexcept {
        return evaluate_boolean(
            liveness_
        );
    }

    [[nodiscard]]
    bool ready() const noexcept {
        return evaluate_boolean(
            readiness_
        );
    }

    [[nodiscard]]
    HealthReport liveness_report()
        const {
        return evaluate(liveness_);
    }

    [[nodiscard]]
    HealthReport readiness_report()
        const {
        return evaluate(readiness_);
    }

    [[nodiscard]]
    const std::vector<HealthCheck>&
    liveness_checks()
        const noexcept {
        return liveness_;
    }

    [[nodiscard]]
    const std::vector<HealthCheck>&
    readiness_checks()
        const noexcept {
        return readiness_;
    }

private:
    static void register_check(
        std::vector<HealthCheck>& target,
        std::unordered_set<std::string>& names,
        std::string name,
        std::function<bool()> check,
        const char* kind
    ) {
        if (name.empty()) {
            throw std::invalid_argument(
                std::string{"Health "} +
                kind +
                " check name must not be empty"
            );
        }

        if (!check) {
            throw std::invalid_argument(
                std::string{"Health "} +
                kind +
                " check callback must not be empty"
            );
        }

        if (!names.insert(name).second) {
            throw std::logic_error(
                std::string{"Health "} +
                kind +
                " check is already registered: " +
                name
            );
        }

        target.push_back({
            std::move(name),
            std::move(check)
        });
    }

    [[nodiscard]]
    static bool evaluate_boolean(
        const std::vector<HealthCheck>& checks
    ) noexcept {
        for (const auto& item : checks) {
            try {
                if (
                    !item.check ||
                    !item.check()
                ) {
                    return false;
                }
            } catch (...) {
                return false;
            }
        }

        return true;
    }

    [[nodiscard]]
    static HealthReport evaluate(
        const std::vector<HealthCheck>& checks
    ) {
        HealthReport report;
        report.checks.reserve(
            checks.size()
        );

        for (const auto& item : checks) {
            bool healthy = false;

            try {
                healthy =
                    item.check &&
                    item.check();
            } catch (...) {
                healthy = false;
            }

            report.healthy =
                report.healthy &&
                healthy;

            report.checks.push_back({
                item.name,
                healthy
            });
        }

        return report;
    }

    std::vector<HealthCheck> liveness_;
    std::vector<HealthCheck> readiness_;

    std::unordered_set<std::string>
        liveness_names_;

    std::unordered_set<std::string>
        readiness_names_;
};

} // namespace gungnir::production
