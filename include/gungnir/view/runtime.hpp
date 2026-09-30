#pragma once

#include <memory>
#include <gungnir/core/execution_context.hpp>
#include <utility>

namespace gungnir::view {

class Engine;

namespace runtime {

using EngineHandle =
    std::shared_ptr<Engine>;

class Scope {
public:
    Scope() = default;

    explicit Scope(
        EngineHandle engine
    ) noexcept;

    Scope(
        const Scope&
    ) = delete;

    Scope& operator=(
        const Scope&
    ) = delete;

    Scope(
        Scope&& other
    ) noexcept;

    Scope& operator=(
        Scope&& other
    ) noexcept;

    ~Scope();

    void reset()
        noexcept;

private:
    gungnir::detail::ContextHandle owner_;
    EngineHandle previous_;
    bool active_{false};
};

void use(
    EngineHandle engine
) noexcept;

void clear() noexcept;

[[nodiscard]]
bool using_engine(
    const Engine& engine
) noexcept;

[[nodiscard]]
EngineHandle current()
    noexcept;

[[nodiscard]]
EngineHandle engine();

[[nodiscard]]
Scope activate(
    EngineHandle engine
) noexcept;

void clear_current()
    noexcept;

} // namespace runtime

} // namespace gungnir::view

