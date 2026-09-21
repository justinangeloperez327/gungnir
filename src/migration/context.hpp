#pragma once

#include <gungnir/migration/definition.hpp>

namespace gungnir::migration::detail {

[[nodiscard]] Plan* current_plan() noexcept;

class Scope {
public:
    explicit Scope(Plan& plan) noexcept;
    ~Scope();

    Scope(const Scope&) = delete;
    Scope& operator=(const Scope&) = delete;

private:
    Plan* previous_;
};

} // namespace gungnir::migration::detail
