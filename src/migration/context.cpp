#include "context.hpp"

namespace gungnir::migration::detail {

namespace {
thread_local Plan* active_plan = nullptr;
}

Plan* current_plan() noexcept {
    return active_plan;
}

Scope::Scope(Plan& plan) noexcept : previous_(active_plan) {
    active_plan = &plan;
}

Scope::~Scope() {
    active_plan = previous_;
}

} // namespace gungnir::migration::detail
