#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

#include <gungnir/core/task.hpp>
#include <gungnir/http/request.hpp>

namespace gungnir::http {

enum class WebSocketOpcode :
    std::uint8_t {
    continuation = 0x0,
    text = 0x1,
    binary = 0x2,
    close = 0x8,
    ping = 0x9,
    pong = 0xa
};

struct WebSocketMessage {
    WebSocketOpcode opcode{
        WebSocketOpcode::text
    };
    std::string payload;

    [[nodiscard]]
    static WebSocketMessage text(
        std::string value
    ) {
        return {
            WebSocketOpcode::text,
            std::move(value)
        };
    }

    [[nodiscard]]
    static WebSocketMessage binary(
        std::string value
    ) {
        return {
            WebSocketOpcode::binary,
            std::move(value)
        };
    }
};

class WebSocketSession {
public:
    using Reply =
        std::optional<
            WebSocketMessage
        >;

    using Handler =
        std::function<
            Task<Reply>(
                WebSocketMessage
            )
        >;

    using SyncHandler =
        std::function<
            Reply(
                WebSocketMessage
            )
        >;

    WebSocketSession() = default;

    explicit WebSocketSession(
        Handler handler
    )
        : handler_(
            std::move(handler)
          ) {}

    explicit WebSocketSession(
        SyncHandler handler
    )
        : handler_(
            [
                handler =
                    std::move(handler)
            ](
                WebSocketMessage message
            ) mutable
                -> Task<Reply> {
                if (!handler) {
                    co_return std::nullopt;
                }

                co_return handler(
                    std::move(message)
                );
            }
          ) {}

    [[nodiscard]]
    bool valid()
        const noexcept {
        return
            static_cast<bool>(
                handler_
            );
    }

    [[nodiscard]]
    Task<Reply> receive(
        WebSocketMessage message
    ) {
        if (!handler_) {
            co_return std::nullopt;
        }

        co_return co_await handler_(
            std::move(message)
        );
    }

private:
    Handler handler_;
};

struct WebSocketHandshake {
    std::string accept;
    std::string protocol;
};

namespace websocket::wire {

struct Frame {
    bool final{true};
    WebSocketOpcode opcode{
        WebSocketOpcode::text
    };
    std::string payload;
};

struct ParseResult {
    std::optional<Frame> frame;
    std::size_t consumed{0};
};

[[nodiscard]]
bool upgrade_requested(
    const Request& request
) noexcept;

[[nodiscard]]
std::optional<WebSocketHandshake>
handshake(
    const Request& request,
    std::string_view selected_protocol = {}
);

[[nodiscard]]
ParseResult parse_client_frame(
    std::string_view bytes,
    std::size_t max_payload_bytes
);

[[nodiscard]]
std::string serialize_server_frame(
    WebSocketOpcode opcode,
    std::string_view payload = {},
    bool final = true
);

[[nodiscard]]
bool valid_utf8(
    std::string_view value
) noexcept;

[[nodiscard]]
bool valid_close_code(
    std::uint16_t code
) noexcept;

} // namespace websocket::wire

} // namespace gungnir::http
