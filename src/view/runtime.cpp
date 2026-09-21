#include <gungnir/view/runtime.hpp>

#include <mutex>
#include <stdexcept>

#include <gungnir/view/engine.hpp>

namespace gungnir::view::runtime {

namespace {

std::mutex runtime_mutex;
Engine* active_engine = nullptr;

} // namespace

void use(Engine& engine) noexcept {
    std::lock_guard lock{runtime_mutex};
    active_engine = &engine;
}

void clear() noexcept {
    std::lock_guard lock{runtime_mutex};
    active_engine = nullptr;
}

bool using_engine(const Engine& engine) noexcept {
    std::lock_guard lock{runtime_mutex};
    return active_engine == &engine;
}

Engine& engine() {
    std::lock_guard lock{runtime_mutex};

    if (!active_engine) {
        throw std::logic_error(
            "Gungnir view runtime requires an active Application"
        );
    }

    return *active_engine;
}

} // namespace gungnir::view::runtime
