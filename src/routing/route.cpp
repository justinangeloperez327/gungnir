#include <gungnir/routing/route.hpp>

#include <mutex>
#include <stdexcept>

namespace gungnir::routing::detail {

namespace {

std::mutex runtime_mutex;
thread_local Router* active_router = nullptr;
thread_local Container* active_container = nullptr;

} // namespace

void bind_route_runtime(
    Router& router,
    Container& container
) noexcept {
    std::lock_guard lock{runtime_mutex};
    active_router = &router;
    active_container = &container;
}

void unbind_route_runtime(
    Router& router,
    Container& container
) noexcept {
    std::lock_guard lock{runtime_mutex};

    if (
        active_router == &router &&
        active_container == &container
    ) {
        active_router = nullptr;
        active_container = nullptr;
    }
}

Router& route_router() {
    if (auto context = gungnir::detail::application_context(); context->router) return *context->router;
    std::lock_guard lock{runtime_mutex};

    if (!active_router) {
        throw std::logic_error(
            "Gungnir Route facade requires an active Application"
        );
    }

    return *active_router;
}

Container& route_container() {
    if (auto context = gungnir::detail::application_context(); context->container) return *context->container;
    std::lock_guard lock{runtime_mutex};

    if (!active_container) {
        throw std::logic_error(
            "Gungnir Route facade requires an active Application"
        );
    }

    return *active_container;
}

} // namespace gungnir::routing::detail
