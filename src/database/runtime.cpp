#include <gungnir/database/runtime.hpp>

#include <stdexcept>

namespace gungnir::database::runtime {

namespace {
Manager* active_manager = nullptr;
}

void use(Manager& manager) noexcept {
    active_manager = &manager;
}

void clear() noexcept {
    active_manager = nullptr;
}

bool configured() noexcept {
    return active_manager != nullptr;
}

bool using_manager(const Manager& manager) noexcept {
    return active_manager == &manager;
}

Manager& manager() {
    if (active_manager == nullptr) {
        throw std::logic_error(
            "Gungnir database runtime is not configured"
        );
    }

    return *active_manager;
}

std::shared_ptr<Connection> connection(std::string_view name) {
    return manager().connection(name);
}

} // namespace gungnir::database::runtime
