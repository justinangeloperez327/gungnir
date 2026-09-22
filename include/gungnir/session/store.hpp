#pragma once

#include <optional>
#include <string>
#include <string_view>

#include <gungnir/session/session.hpp>

namespace gungnir::session {

class Store {
public:
    virtual ~Store() = default;
    [[nodiscard]] virtual std::optional<Session> load(std::string_view id) = 0;
    virtual void save(const Session& session) = 0;
    virtual void erase(std::string_view id) = 0;
};

} // namespace gungnir::session
