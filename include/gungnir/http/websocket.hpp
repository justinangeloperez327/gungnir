#pragma once
#include <string>
#include <string_view>

namespace gungnir::http {

struct WebSocketUpgrade {
    std::string key;
    std::string protocol;
};

[[nodiscard]] inline bool websocket_upgrade(std::string_view connection, std::string_view upgrade) {
    return connection.find("Upgrade") != std::string_view::npos &&
           (upgrade == "websocket" || upgrade == "WebSocket");
}

} // namespace gungnir::http
