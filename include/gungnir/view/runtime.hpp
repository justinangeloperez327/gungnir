#pragma once

namespace gungnir::view {

class Engine;

namespace runtime {

void use(Engine& engine) noexcept;
void clear() noexcept;
[[nodiscard]] bool using_engine(const Engine& engine) noexcept;
[[nodiscard]] Engine& engine();

} // namespace runtime

} // namespace gungnir::view
