#pragma once

#include <string>
#include <optional>
#include <gungnir/mail/message.hpp>
#include <gungnir/http/json.hpp>
#include <string_view>
#include <vector>

namespace gungnir::notifications {

class Notification {
public:
    virtual ~Notification() = default;
    [[nodiscard]] virtual std::optional<mail::Message> mail_message() const { return std::nullopt; }
    [[nodiscard]] virtual std::optional<http::Json> database_payload() const { return std::nullopt; }
    [[nodiscard]] virtual std::string_view name() const noexcept = 0;
    [[nodiscard]] virtual std::vector<std::string> channels() const = 0;
};

} // namespace gungnir::notifications
