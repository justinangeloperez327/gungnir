#pragma once
#include <string_view>

namespace gungnir::events {

class Event {
public:
    virtual ~Event() = default;
    [[nodiscard]] virtual std::string_view name() const noexcept = 0;
};

} // namespace gungnir::events
