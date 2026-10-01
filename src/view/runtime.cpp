#include <gungnir/view/runtime.hpp>

#include <mutex>
#include <stdexcept>

#include <gungnir/view/engine.hpp>

namespace gungnir::view::runtime {

namespace {

std::mutex runtime_mutex;

thread_local EngineHandle fallback_engine;



} // namespace

Scope::Scope(
    EngineHandle engine
) noexcept
    : owner_(gungnir::detail::current_execution_context()),
      previous_(
        std::move(
            gungnir::detail::active_context->view
        )
      ),
      active_(true) {
    gungnir::detail::active_context->view =
        std::move(engine);
}

Scope::Scope(
    Scope&& other
) noexcept
    : owner_(std::move(other.owner_)),
      previous_(
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

    owner_ = std::move(other.owner_);
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

    owner_->view =
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
    if (gungnir::detail::active_context->view) {
        return gungnir::detail::active_context->view;
    }

    if (auto context = gungnir::detail::application_context(); context->view) return context->view;
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
    gungnir::detail::active_context->view.reset();
}

} // namespace gungnir::view::runtime

