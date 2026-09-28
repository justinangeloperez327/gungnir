#include "http2_session.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

#include <nghttp2/nghttp2.h>

#include <gungnir/http/cookie.hpp>

namespace gungnir::http::detail {

namespace {

[[nodiscard]]
Method parse_method(
    std::string_view value
) {
    if (value == "GET") {
        return Method::get;
    }

    if (value == "POST") {
        return Method::post;
    }

    if (value == "PUT") {
        return Method::put;
    }

    if (value == "PATCH") {
        return Method::patch;
    }

    if (value == "DELETE") {
        return Method::delete_;
    }

    if (value == "OPTIONS") {
        return Method::options;
    }

    if (value == "HEAD") {
        return Method::head;
    }

    throw std::invalid_argument(
        "Unsupported HTTP/2 method"
    );
}

[[nodiscard]]
bool connection_specific(
    std::string_view name
) noexcept {
    return
        name == "connection" ||
        name == "keep-alive" ||
        name == "proxy-connection" ||
        name == "transfer-encoding" ||
        name == "upgrade";
}

struct SessionDeleter {
    void operator()(
        nghttp2_session* session
    ) const noexcept {
        if (session != nullptr) {
            nghttp2_session_del(
                session
            );
        }
    }
};

struct CallbacksDeleter {
    void operator()(
        nghttp2_session_callbacks*
            callbacks
    ) const noexcept {
        if (callbacks != nullptr) {
            nghttp2_session_callbacks_del(
                callbacks
            );
        }
    }
};

} // namespace

class Http2Session::Impl {
public:
    struct Stream {
        std::int32_t id{0};
        std::string method;
        std::string path;
        std::vector<
            std::pair<
                std::string,
                std::string
            >
        > headers;
        std::string body;
        std::size_t header_bytes{0};
        bool request_headers{false};
        bool completed{false};
    };

    struct ResponseBody {
        std::string body;
        std::size_t offset{0};
    };

    Impl(
        Http2Options value_options,
        std::size_t value_max_request_bytes
    )
        : options(
            std::move(value_options)
          ),
          max_request_bytes(
            value_max_request_bytes
          ) {
        nghttp2_session_callbacks*
            raw_callbacks = nullptr;

        if (
            nghttp2_session_callbacks_new(
                &raw_callbacks
            ) != 0
        ) {
            throw std::runtime_error(
                "Unable to allocate HTTP/2 callbacks"
            );
        }

        std::unique_ptr<
            nghttp2_session_callbacks,
            CallbacksDeleter
        > callbacks{
            raw_callbacks
        };

        nghttp2_session_callbacks_set_on_begin_headers_callback(
            callbacks.get(),
            &Impl::on_begin_headers
        );

        nghttp2_session_callbacks_set_on_header_callback(
            callbacks.get(),
            &Impl::on_header
        );

        nghttp2_session_callbacks_set_on_data_chunk_recv_callback(
            callbacks.get(),
            &Impl::on_data_chunk
        );

        nghttp2_session_callbacks_set_on_frame_recv_callback(
            callbacks.get(),
            &Impl::on_frame_recv
        );

        nghttp2_session_callbacks_set_on_stream_close_callback(
            callbacks.get(),
            &Impl::on_stream_close
        );

        nghttp2_session*
            raw_session = nullptr;

        const auto status =
            nghttp2_session_server_new(
                &raw_session,
                callbacks.get(),
                this
            );

        if (status != 0) {
            throw std::runtime_error(
                std::string{
                    "Unable to create HTTP/2 server session: "
                } +
                nghttp2_strerror(
                    status
                )
            );
        }

        session.reset(
            raw_session
        );

        nghttp2_settings_entry
            settings[] = {
                {
                    NGHTTP2_SETTINGS_MAX_CONCURRENT_STREAMS,
                    static_cast<
                        std::uint32_t
                    >(
                        std::min<
                            std::size_t
                        >(
                            options
                                .max_concurrent_streams,
                            static_cast<
                                std::size_t
                            >(
                                std::numeric_limits<
                                    std::uint32_t
                                >::max()
                            )
                        )
                    )
                },
                {
                    NGHTTP2_SETTINGS_MAX_HEADER_LIST_SIZE,
                    static_cast<
                        std::uint32_t
                    >(
                        std::min<
                            std::size_t
                        >(
                            options
                                .max_header_list_bytes,
                            static_cast<
                                std::size_t
                            >(
                                std::numeric_limits<
                                    std::uint32_t
                                >::max()
                            )
                        )
                    )
                }
            };

        const auto settings_status =
            nghttp2_submit_settings(
                session.get(),
                NGHTTP2_FLAG_NONE,
                settings,
                sizeof(settings) /
                    sizeof(settings[0])
            );

        if (settings_status != 0) {
            throw std::runtime_error(
                std::string{
                    "Unable to submit HTTP/2 SETTINGS: "
                } +
                nghttp2_strerror(
                    settings_status
                )
            );
        }
    }

    static int on_begin_headers(
        nghttp2_session*,
        const nghttp2_frame* frame,
        void* user_data
    ) {
        auto& self =
            *static_cast<Impl*>(
                user_data
            );

        if (
            frame->hd.type !=
                NGHTTP2_HEADERS ||
            frame->headers.cat !=
                NGHTTP2_HCAT_REQUEST
        ) {
            return 0;
        }

        Stream stream;
        stream.id =
            frame->hd.stream_id;
        stream.request_headers =
            true;

        self.streams.insert_or_assign(
            stream.id,
            std::move(stream)
        );

        return 0;
    }

    static int on_header(
        nghttp2_session*,
        const nghttp2_frame* frame,
        const std::uint8_t* name,
        std::size_t name_length,
        const std::uint8_t* value,
        std::size_t value_length,
        std::uint8_t,
        void* user_data
    ) {
        auto& self =
            *static_cast<Impl*>(
                user_data
            );

        const auto found =
            self.streams.find(
                frame->hd.stream_id
            );

        if (
            found ==
            self.streams.end() ||
            !found->second
                .request_headers
        ) {
            return 0;
        }

        auto& stream =
            found->second;

        if (
            name_length >
                self.options
                    .max_header_list_bytes -
                std::min(
                    stream.header_bytes,
                    self.options
                        .max_header_list_bytes
                ) ||
            value_length >
                self.options
                    .max_header_list_bytes -
                std::min(
                    stream.header_bytes +
                        name_length,
                    self.options
                        .max_header_list_bytes
                )
        ) {
            return
                NGHTTP2_ERR_TEMPORAL_CALLBACK_FAILURE;
        }

        stream.header_bytes +=
            name_length +
            value_length;

        const std::string_view
            header_name{
                reinterpret_cast<
                    const char*
                >(name),
                name_length
            };

        const std::string_view
            header_value{
                reinterpret_cast<
                    const char*
                >(value),
                value_length
            };

        if (header_name == ":method") {
            stream.method =
                std::string{
                    header_value
                };

            return 0;
        }

        if (header_name == ":path") {
            stream.path =
                std::string{
                    header_value
                };

            return 0;
        }

        if (
            header_name ==
            ":authority"
        ) {
            stream.headers.emplace_back(
                "host",
                std::string{
                    header_value
                }
            );

            return 0;
        }

        if (
            !header_name.empty() &&
            header_name.front() == ':'
        ) {
            return 0;
        }

        if (
            connection_specific(
                header_name
            ) ||
            (
                header_name == "te" &&
                header_value != "trailers"
            )
        ) {
            return
                NGHTTP2_ERR_TEMPORAL_CALLBACK_FAILURE;
        }

        if (
            stream.header_bytes >
            self.max_request_bytes
        ) {
            return
                NGHTTP2_ERR_TEMPORAL_CALLBACK_FAILURE;
        }

        stream.headers.emplace_back(
            std::string{
                header_name
            },
            std::string{
                header_value
            }
        );

        return 0;
    }

    static int on_data_chunk(
        nghttp2_session*,
        std::uint8_t,
        std::int32_t stream_id,
        const std::uint8_t* data,
        std::size_t length,
        void* user_data
    ) {
        auto& self =
            *static_cast<Impl*>(
                user_data
            );

        const auto found =
            self.streams.find(
                stream_id
            );

        if (
            found ==
            self.streams.end()
        ) {
            return 0;
        }

        auto& body =
            found->second.body;

        const auto header_bytes =
            std::min(
                found->second.header_bytes,
                self.max_request_bytes
            );

        const auto body_budget =
            self.max_request_bytes -
            header_bytes;

        if (
            body.size() > body_budget ||
            length >
                body_budget -
                body.size()
        ) {
            return
                NGHTTP2_ERR_TEMPORAL_CALLBACK_FAILURE;
        }

        body.append(
            reinterpret_cast<
                const char*
            >(data),
            length
        );

        return 0;
    }

    static int on_frame_recv(
        nghttp2_session*,
        const nghttp2_frame* frame,
        void* user_data
    ) {
        auto& self =
            *static_cast<Impl*>(
                user_data
            );

        if (
            (
                frame->hd.type !=
                    NGHTTP2_HEADERS &&
                frame->hd.type !=
                    NGHTTP2_DATA
            ) ||
            (
                frame->hd.flags &
                NGHTTP2_FLAG_END_STREAM
            ) == 0
        ) {
            return 0;
        }

        const auto found =
            self.streams.find(
                frame->hd.stream_id
            );

        if (
            found ==
                self.streams.end() ||
            found->second.completed
        ) {
            return 0;
        }

        auto& stream =
            found->second;

        if (
            stream.method.empty() ||
            stream.path.empty() ||
            stream.path.front() != '/'
        ) {
            static_cast<void>(
                nghttp2_submit_rst_stream(
                    self.session.get(),
                    NGHTTP2_FLAG_NONE,
                    frame->hd.stream_id,
                    NGHTTP2_PROTOCOL_ERROR
                )
            );

            stream.completed = true;

            return 0;
        }

        try {
            Http2RequestData request;
            request.stream_id =
                stream.id;
            request.method =
                parse_method(
                    stream.method
                );
            request.path =
                std::move(
                    stream.path
                );
            request.headers =
                std::move(
                    stream.headers
                );
            request.body =
                std::move(
                    stream.body
                );

            stream.completed = true;

            self.ready_requests.push_back(
                std::move(request)
            );
        } catch (...) {
            static_cast<void>(
                nghttp2_submit_rst_stream(
                    self.session.get(),
                    NGHTTP2_FLAG_NONE,
                    frame->hd.stream_id,
                    NGHTTP2_PROTOCOL_ERROR
                )
            );

            stream.completed = true;
        }

        return 0;
    }

    static int on_stream_close(
        nghttp2_session*,
        std::int32_t stream_id,
        std::uint32_t,
        void* user_data
    ) {
        auto& self =
            *static_cast<Impl*>(
                user_data
            );

        self.streams.erase(
            stream_id
        );

        self.response_bodies.erase(
            stream_id
        );

        self.closed_streams.push_back(
            stream_id
        );

        return 0;
    }

    static ssize_t read_response_body(
        nghttp2_session*,
        std::int32_t,
        std::uint8_t* buffer,
        std::size_t length,
        std::uint32_t* data_flags,
        nghttp2_data_source* source,
        void*
    ) {
        auto* body =
            static_cast<
                ResponseBody*
            >(
                source->ptr
            );

        if (body == nullptr) {
            *data_flags |=
                NGHTTP2_DATA_FLAG_EOF;

            return 0;
        }

        const auto remaining =
            body->body.size() -
            body->offset;

        const auto count =
            std::min(
                remaining,
                length
            );

        if (count != 0) {
            std::memcpy(
                buffer,
                body->body.data() +
                    body->offset,
                count
            );

            body->offset += count;
        }

        if (
            body->offset >=
            body->body.size()
        ) {
            *data_flags |=
                NGHTTP2_DATA_FLAG_EOF;
        }

        return
            static_cast<ssize_t>(
                count
            );
    }

    [[nodiscard]]
    static nghttp2_nv nv(
        std::string_view name,
        std::string_view value
    ) noexcept {
        return {
            const_cast<
                std::uint8_t*
            >(
                reinterpret_cast<
                    const std::uint8_t*
                >(
                    name.data()
                )
            ),
            const_cast<
                std::uint8_t*
            >(
                reinterpret_cast<
                    const std::uint8_t*
                >(
                    value.data()
                )
            ),
            name.size(),
            value.size(),
            NGHTTP2_NV_FLAG_NONE
        };
    }

    Http2Options options;
    std::size_t max_request_bytes{0};

    std::unique_ptr<
        nghttp2_session,
        SessionDeleter
    > session;

    std::unordered_map<
        std::int32_t,
        Stream
    > streams;

    std::unordered_map<
        std::int32_t,
        std::unique_ptr<
            ResponseBody
        >
    > response_bodies;

    std::vector<Http2RequestData>
        ready_requests;

    std::vector<std::int32_t>
        closed_streams;
};

Http2Session::Http2Session(
    Http2Options options,
    std::size_t max_request_bytes
)
    : impl_(
        std::make_unique<Impl>(
            std::move(options),
            max_request_bytes
        )
      ) {}

Http2Session::~Http2Session() =
    default;

void Http2Session::receive(
    std::string_view bytes
) {
    std::size_t offset = 0;

    while (offset < bytes.size()) {
        const auto consumed =
            nghttp2_session_mem_recv(
                impl_->session.get(),
                reinterpret_cast<
                    const std::uint8_t*
                >(
                    bytes.data() +
                    offset
                ),
                bytes.size() -
                    offset
            );

        if (consumed < 0) {
            throw std::runtime_error(
                std::string{
                    "HTTP/2 receive failed: "
                } +
                nghttp2_strerror(
                    static_cast<int>(
                        consumed
                    )
                )
            );
        }

        if (consumed == 0) {
            throw std::runtime_error(
                "HTTP/2 parser made no progress"
            );
        }

        offset +=
            static_cast<std::size_t>(
                consumed
            );
    }
}

std::vector<Http2RequestData>
Http2Session::take_requests() {
    auto requests =
        std::move(
            impl_->ready_requests
        );

    impl_->ready_requests.clear();

    return requests;
}

std::vector<std::int32_t>
Http2Session::take_closed_streams() {
    auto streams =
        std::move(
            impl_->closed_streams
        );

    impl_->closed_streams.clear();

    return streams;
}

void Http2Session::submit_response(
    std::int32_t stream_id,
    const Response& response,
    bool omit_body
) {
    Response effective =
        response;

    if (
        effective.streaming() ||
        effective.websocket_upgrade()
    ) {
        effective =
            Response::text(
                "HTTP/2 response mode is not supported for this route",
                501
            );
    }

    std::vector<std::string>
        names;

    std::vector<std::string>
        values;

    names.reserve(
        effective.headers().size() +
        effective.cookies().size() +
        2
    );

    values.reserve(
        names.capacity()
    );

    names.push_back(
        ":status"
    );

    values.push_back(
        std::to_string(
            effective.status()
        )
    );

    for (
        const auto& [name, value] :
        effective.headers()
    ) {
        if (
            connection_specific(name) ||
            name == "content-length"
        ) {
            continue;
        }

        names.push_back(name);
        values.push_back(value);
    }

    for (
        const auto& cookie :
        effective.cookies()
    ) {
        names.push_back(
            "set-cookie"
        );

        values.push_back(
            serialize_cookie(cookie)
        );
    }

    names.push_back(
        "content-length"
    );

    values.push_back(
        std::to_string(
            effective.body().size()
        )
    );

    std::vector<nghttp2_nv>
        headers;

    headers.reserve(
        names.size()
    );

    for (
        std::size_t index = 0;
        index < names.size();
        ++index
    ) {
        headers.push_back(
            Impl::nv(
                names[index],
                values[index]
            )
        );
    }

    nghttp2_data_provider
        provider{};

    nghttp2_data_provider*
        provider_ptr = nullptr;

    if (
        !omit_body &&
        !effective.body().empty()
    ) {
        auto body =
            std::make_unique<
                Impl::ResponseBody
            >();

        body->body =
            std::string{
                effective.body()
            };

        provider.source.ptr =
            body.get();

        provider.read_callback =
            &Impl::read_response_body;

        impl_->response_bodies
            .insert_or_assign(
                stream_id,
                std::move(body)
            );

        provider_ptr =
            &provider;
    }

    const auto status =
        nghttp2_submit_response(
            impl_->session.get(),
            stream_id,
            headers.data(),
            headers.size(),
            provider_ptr
        );

    if (status != 0) {
        impl_->response_bodies.erase(
            stream_id
        );

        throw std::runtime_error(
            std::string{
                "Unable to submit HTTP/2 response: "
            } +
            nghttp2_strerror(status)
        );
    }
}

void Http2Session::reset_stream(
    std::int32_t stream_id,
    std::uint32_t error_code
) {
    const auto status =
        nghttp2_submit_rst_stream(
            impl_->session.get(),
            NGHTTP2_FLAG_NONE,
            stream_id,
            error_code
        );

    if (
        status != 0 &&
        status !=
            NGHTTP2_ERR_STREAM_CLOSED
    ) {
        throw std::runtime_error(
            std::string{
                "Unable to reset HTTP/2 stream: "
            } +
            nghttp2_strerror(status)
        );
    }
}

void Http2Session::graceful_shutdown() {
    const auto last_stream_id =
        nghttp2_session_get_last_proc_stream_id(
            impl_->session.get()
        );

    const auto status =
        nghttp2_submit_goaway(
            impl_->session.get(),
            NGHTTP2_FLAG_NONE,
            last_stream_id,
            NGHTTP2_NO_ERROR,
            nullptr,
            0
        );

    if (status != 0) {
        throw std::runtime_error(
            std::string{
                "Unable to submit HTTP/2 GOAWAY: "
            } +
            nghttp2_strerror(status)
        );
    }
}

void Http2Session::terminate(
    std::uint32_t error_code
) {
    const auto status =
        nghttp2_session_terminate_session(
            impl_->session.get(),
            error_code
        );

    if (status != 0) {
        throw std::runtime_error(
            std::string{
                "Unable to terminate HTTP/2 session: "
            } +
            nghttp2_strerror(status)
        );
    }
}

std::string Http2Session::next_output() {
    const std::uint8_t* data =
        nullptr;

    const auto length =
        nghttp2_session_mem_send(
            impl_->session.get(),
            &data
        );

    if (length < 0) {
        throw std::runtime_error(
            std::string{
                "HTTP/2 serialization failed: "
            } +
            nghttp2_strerror(
                static_cast<int>(
                    length
                )
            )
        );
    }

    if (
        length == 0 ||
        data == nullptr
    ) {
        return {};
    }

    return std::string{
        reinterpret_cast<
            const char*
        >(data),
        static_cast<std::size_t>(
            length
        )
    };
}

bool Http2Session::want_read()
    const noexcept {
    return
        nghttp2_session_want_read(
            impl_->session.get()
        ) != 0;
}

bool Http2Session::want_write()
    const noexcept {
    return
        nghttp2_session_want_write(
            impl_->session.get()
        ) != 0;
}

std::size_t Http2Session::active_streams()
    const noexcept {
    return impl_->streams.size();
}

} // namespace gungnir::http::detail
