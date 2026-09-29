#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <gungnir/http/method.hpp>
#include <gungnir/http/response.hpp>
#include <gungnir/http/runtime.hpp>

struct nghttp2_session;

namespace gungnir::http::detail {

struct Http2RequestData {
    std::int32_t stream_id{0};
    Method method{Method::get};
    std::string path;
    std::vector<
        std::pair<
            std::string,
            std::string
        >
    > headers;
    std::string body;
};

class Http2Session {
public:
    Http2Session(
        Http2Options options,
        std::size_t max_request_bytes
    );

    ~Http2Session();

    Http2Session(
        const Http2Session&
    ) = delete;

    Http2Session& operator=(
        const Http2Session&
    ) = delete;

    void receive(
        std::string_view bytes
    );

    [[nodiscard]]
    std::vector<Http2RequestData>
    take_requests();

    [[nodiscard]]
    std::vector<std::int32_t>
    take_closed_streams();

    void submit_response(
        std::int32_t stream_id,
        const Response& response,
        bool omit_body = false
    );

    void submit_stream_response(
        std::int32_t stream_id,
        const Response& response,
        bool omit_body = false
    );

    void push_stream_chunk(
        std::int32_t stream_id,
        std::string chunk
    );

    void finish_stream(
        std::int32_t stream_id
    );

    void reset_stream(
        std::int32_t stream_id,
        std::uint32_t error_code
    );

    void graceful_shutdown();

    void terminate(
        std::uint32_t error_code
    );

    [[nodiscard]]
    std::string next_output();

    [[nodiscard]]
    bool want_read()
        const noexcept;

    [[nodiscard]]
    bool want_write()
        const noexcept;

    [[nodiscard]]
    std::size_t active_streams()
        const noexcept;

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace gungnir::http::detail
