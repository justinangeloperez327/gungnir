#include <gungnir/http/response.hpp>

#include <algorithm>
#include <cctype>
#include <stdexcept>
#include <utility>

#include <gungnir/view/engine.hpp>
#include <gungnir/view/runtime.hpp>

namespace gungnir::http {

namespace {

std::string normalize_header_name(std::string_view name) {
    std::string normalized{name};

    std::transform(
        normalized.begin(),
        normalized.end(),
        normalized.begin(),
        [](unsigned char character) {
            return static_cast<char>(std::tolower(character));
        }
    );

    return normalized;
}

std::string safe_download_filename(
    std::string_view filename
) {
    std::string safe;
    safe.reserve(filename.size());

    for (const auto character : filename) {
        const auto value =
            static_cast<unsigned char>(
                character
            );

        if (
            value < 0x20 ||
            value == 0x7f ||
            character == '"' ||
            character == '\\' ||
            character == '/' ||
            character == ';'
        ) {
            safe.push_back('_');
        } else {
            safe.push_back(character);
        }
    }

    while (
        !safe.empty() &&
        (
            safe.front() == '.' ||
            std::isspace(
                static_cast<unsigned char>(
                    safe.front()
                )
            ) != 0
        )
    ) {
        safe.erase(safe.begin());
    }

    while (
        !safe.empty() &&
        std::isspace(
            static_cast<unsigned char>(
                safe.back()
            )
        ) != 0
    ) {
        safe.pop_back();
    }

    return safe.empty()
        ? std::string{"download"}
        : safe;
}

} // namespace

Response::Response(int status, std::string body)
    : status_(status),
      body_(std::move(body)) {}

int Response::status() const noexcept {
    return status_;
}

std::string_view Response::body() const noexcept {
    return body_;
}

bool Response::streaming()
    const noexcept {
    return
        static_cast<bool>(
            stream_
        );
}

std::shared_ptr<BodyStream>
Response::body_stream()
    const noexcept {
    return stream_;
}

bool Response::websocket_upgrade()
    const noexcept {
    return
        static_cast<bool>(
            websocket_
        );
}

std::shared_ptr<WebSocketSession>
Response::websocket_session()
    const noexcept {
    return websocket_;
}

std::string_view
Response::websocket_protocol()
    const noexcept {
    return websocket_protocol_;
}

Response& Response::status(int value) noexcept {
    status_ = value;
    return *this;
}

Response& Response::body(std::string value) {
    body_ = std::move(value);
    stream_.reset();
    websocket_.reset();
    websocket_protocol_.clear();
    return *this;
}

Response& Response::header(
    std::string name,
    std::string value
) {
    headers_.insert_or_assign(
        normalize_header_name(name),
        std::move(value)
    );

    return *this;
}

std::string_view Response::header(
    std::string_view name
) const noexcept {
    const auto found = headers_.find(
        normalize_header_name(name)
    );

    if (found == headers_.end()) {
        return {};
    }

    return found->second;
}

const Response::Headers& Response::headers() const noexcept { return headers_; }

Response& Response::cookie(Cookie value) { cookies_.push_back(std::move(value)); return *this; }
Response& Response::without_cookie(std::string name, std::string path) {
    Cookie value{.name=std::move(name), .value="", .path=std::move(path), .max_age=std::chrono::seconds{0}};
    cookies_.push_back(std::move(value)); return *this;
}
const Response::Cookies& Response::cookies() const noexcept { return cookies_; }

Response Response::text(
    std::string body,
    int status
) {
    Response response{status, std::move(body)};
    response.header(
        "content-type",
        "text/plain; charset=utf-8"
    );
    return response;
}

Response Response::json(
    Json value,
    int status
) {
    Response response{status, value.dump()};
    response.header(
        "content-type",
        "application/json; charset=utf-8"
    );
    return response;
}

Response Response::view(
    std::string name,
    gungnir::view::Data data,
    int status
) {
    Response response{
        status,
        gungnir::view::runtime::engine()->render(
            name,
            data
        )
    };

    response.header(
        "content-type",
        "text/html; charset=utf-8"
    );

    return response;
}

Response Response::stream(
    BodyStream stream,
    std::string content_type,
    int status
) {
    if (!stream.valid()) {
        throw std::invalid_argument(
            "Streaming response requires a producer"
        );
    }

    Response response{status};

    response.stream_ =
        std::make_shared<BodyStream>(
            std::move(stream)
        );

    response.header(
        "content-type",
        std::move(content_type)
    );

    return response;
}

Response Response::websocket(
    WebSocketSession session,
    std::string protocol
) {
    if (!session.valid()) {
        throw std::invalid_argument(
            "WebSocket response requires a message handler"
        );
    }

    Response response{101};

    response.websocket_ =
        std::make_shared<
            WebSocketSession
        >(
            std::move(session)
        );

    response.websocket_protocol_ =
        std::move(protocol);

    return response;
}

Response Response::no_content() {
    return Response{204};
}

Response Response::redirect(
    std::string location,
    int status
) {
    Response response{status};
    response.header(
        "location",
        std::move(location)
    );
    return response;
}

Response Response::not_found() {
    return Response::text("Not Found", 404);
}

Response Response::html(std::string body, int status) {
    Response response{status, std::move(body)};
    response.header("content-type", "text/html; charset=utf-8");
    return response;
}

Response Response::download(std::string body, std::string filename, std::string content_type, int status) {
    Response response{status, std::move(body)};
    response.header("content-type", std::move(content_type));
    response.header(
        "content-disposition",
        "attachment; filename=\"" +
            safe_download_filename(filename) +
            "\""
    );
    return response;
}

} // namespace gungnir::http
