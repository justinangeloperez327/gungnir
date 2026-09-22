#pragma once

#include <string>
#include <string_view>
#include <vector>

namespace gungnir::notifications {

class Notification {
public:
    virtual ~Notification() = default;
    [[nodiscard]] virtual std::string_view name() const noexcept = 0;
    [[nodiscard]] virtual std::vector<std::string> channels() const = 0;
};

} // namespace gungnir::notifications
