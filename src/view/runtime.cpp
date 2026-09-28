#include <gungnir/view/runtime.hpp>

#include <mutex>
#include <stdexcept>

#include <gungnir/view/engine.hpp>

namespace gungnir::view::runtime {

namespace {

std::mutex runtime_mutex;

EngineHandle fallback_engine;

thread_local EngineHandle
    current_engine;

} // namespace

Scope::Scope(
    EngineHandle engine
) noexcept
    : previous_(
        std::move(
            current_engine
        )
      ),
      active_(true) {
    current_engine =
        std::move(engine);
}

Scope::Scope(
    Scope&& other
) noexcept
    : previous_(
        std::move(
            other.previous_
        )
      ),
      active_(
        std::exchange(
            other.active_,
            false
        )
      ) {}

Scope& Scope::operator=(
    Scope&& other
) noexcept {
    if (this == &other) {
        return *this;
    }

    reset();

    previous_ =
        std::move(
            other.previous_
        );

    active_ =
        std::exchange(
            other.active_,
            false
        );

    return *this;
}

Scope::~Scope() {
    reset();
}

void Scope::reset()
    noexcept {
    if (!active_) {
        return;
    }

    current_engine =
        std::move(previous_);

    active_ = false;
}

void use(
    EngineHandle engine
) noexcept {
    std::lock_guard lock{
        runtime_mutex
    };

    fallback_engine =
        std::move(engine);
}

void clear()
    noexcept {
    std::lock_guard lock{
        runtime_mutex
    };

    fallback_engine.reset();
}

bool using_engine(
    const Engine& engine
) noexcept {
    std::lock_guard lock{
        runtime_mutex
    };

    return
        fallback_engine.get() ==
        &engine;
}

EngineHandle current()
    noexcept {
    if (current_engine) {
        return current_engine;
    }

    std::lock_guard lock{
        runtime_mutex
    };

    return fallback_engine;
}

EngineHandle engine() {
    auto selected =
        current();

    if (!selected) {
        throw std::logic_error(
            "Gungnir view runtime requires an active Application or scoped view engine"
        );
    }

    return selected;
}

Scope activate(
    EngineHandle engine
) noexcept {
    return Scope{
        std::move(engine)
    };
}

void clear_current()
    noexcept {
    current_engine.reset();
}

} // namespace gungnir::view::runtime
