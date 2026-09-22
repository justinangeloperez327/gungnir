#include <gungnir/http/message.hpp>

#include <charconv>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace gungnir::http::wire {

namespace {

std::string_view trim(std::string_view value) {
    while (!value.empty() && (value.front() == ' ' || value.front() == '\t')) {
        value.remove_prefix(1);
    }

    while (!value.empty() && (value.back() == ' ' || value.back() == '\t')) {
        value.remove_suffix(1);
    }

    return value;
}

std::optional<Method> parse_method(std::string_view value) {
    if (value == "GET") return Method::get;
    if (value == "POST") return Method::post;
    if (value == "PUT") return Method::put;
    if (value == "PATCH") return Method::patch;
    if (value == "DELETE") return Method::delete_;
    if (value == "OPTIONS") return Method::options;
    if (value == "HEAD") return Method::head;
    return std::nullopt;
}

std::string_view reason_phrase(int status) noexcept {
    switch (status) {
        case 200: return "OK";
        case 201: return "Created";
        case 202: return "Accepted";
        case 204: return "No Content";
        case 301: return "Moved Permanently";
        case 302: return "Found";
        case 303: return "See Other";
        case 307: return "Temporary Redirect";
        case 308: return "Permanent Redirect";
        case 400: return "Bad Request";
        case 401: return "Unauthorized";
        case 403: return "Forbidden";
        case 404: return "Not Found";
        case 405: return "Method Not Allowed";
        case 409: return "Conflict";
        case 413: return "Payload Too Large";
        case 415: return "Unsupported Media Type";
        case 422: return "Unprocessable Content";
        case 429: return "Too Many Requests";
        case 500: return "Internal Server Error";
        case 501: return "Not Implemented";
        case 502: return "Bad Gateway";
        case 503: return "Service Unavailable";
        default: return "Unknown";
    }
}

void validate_header(std::string_view name, std::string_view value) {
    if (
        name.empty() ||
        name.find('\r') != std::string_view::npos ||
        name.find('\n') != std::string_view::npos ||
        value.find('\r') != std::string_view::npos ||
        value.find('\n') != std::string_view::npos
    ) {
        throw std::invalid_argument("Invalid HTTP header");
    }
}

} // namespace

Request parse_request(std::string_view message) {
    const auto header_end = message.find("\r\n\r\n");
    if (header_end == std::string_view::npos) {
        throw std::invalid_argument("Incomplete HTTP request headers");
    }

    const auto request_line_end = message.find("\r\n");
    if (
        request_line_end == std::string_view::npos ||
        request_line_end > header_end
    ) {
        throw std::invalid_argument("Invalid HTTP request line");
    }

    const auto request_line = message.substr(0, request_line_end);
    const auto first_space = request_line.find(' ');
    const auto second_space = first_space == std::string_view::npos
        ? std::string_view::npos
        : request_line.find(' ', first_space + 1);

    if (
        first_space == std::string_view::npos ||
        second_space == std::string_view::npos
    ) {
        throw std::invalid_argument("Invalid HTTP request line");
    }

    const auto method_value = request_line.substr(0, first_space);
    const auto method = parse_method(method_value);
    if (!method) {
        throw std::invalid_argument("Unsupported HTTP method");
    }

    auto target = request_line.substr(
        first_space + 1,
        second_space - first_space - 1
    );
    const auto version = request_line.substr(second_space + 1);

    if (version != "HTTP/1.1" && version != "HTTP/1.0") {
        throw std::invalid_argument("Unsupported HTTP version");
    }

    if (target.empty() || target.front() != '/') {
        throw std::invalid_argument("Unsupported HTTP request target");
    }

    std::vector<std::pair<std::string, std::string>> headers;
    std::optional<std::size_t> content_length;

    std::size_t cursor = request_line_end + 2;
    while (cursor < header_end) {
        const auto line_end = message.find("\r\n", cursor);
        const auto bounded_end =
            line_end == std::string_view::npos || line_end > header_end
                ? header_end
                : line_end;

        const auto line = message.substr(cursor, bounded_end - cursor);
        const auto colon = line.find(':');
        if (colon == std::string_view::npos) {
            throw std::invalid_argument("Invalid HTTP header");
        }

        const auto name = trim(line.substr(0, colon));
        const auto value = trim(line.substr(colon + 1));
        if (name.empty()) {
            throw std::invalid_argument("Invalid HTTP header name");
        }

        std::string lower_name{name};
        for (auto& character : lower_name) {
            if (character >= 'A' && character <= 'Z') {
                character = static_cast<char>(character - 'A' + 'a');
            }
        }

        if (lower_name == "content-length") {
            std::size_t parsed = 0;
            const auto* begin = value.data();
            const auto* end = value.data() + value.size();
            const auto result = std::from_chars(begin, end, parsed);
            if (result.ec != std::errc{} || result.ptr != end) {
                throw std::invalid_argument("Invalid Content-Length");
            }
            content_length = parsed;
        }

        if (
            lower_name == "transfer-encoding" &&
            value != "identity"
        ) {
            throw std::invalid_argument(
                "Transfer-Encoding is not supported by this HTTP backend"
            );
        }

        headers.emplace_back(std::move(lower_name), std::string{value});

        if (bounded_end == header_end) {
            break;
        }
        cursor = bounded_end + 2;
    }

    const auto body_offset = header_end + 4;
    auto body = message.substr(body_offset);

    if (content_length) {
        if (body.size() < *content_length) {
            throw std::invalid_argument("Incomplete HTTP request body");
        }
        body = body.substr(0, *content_length);
    } else {
        body = {};
    }

    Request request{
        *method,
        std::string{target.empty() ? std::string_view{"/"} : target},
        std::string{body}
    };

    for (auto& [name, value] : headers) {
        request.set_header(std::move(name), std::move(value));
    }

    return request;
}

std::string serialize_response(
    const Response& response,
    bool omit_body,
    ConnectionDirective connection
) {
    std::string output;
    output.reserve(response.body().size() + 256);

    output += "HTTP/1.1 ";
    output += std::to_string(response.status());
    output += ' ';
    output += reason_phrase(response.status());
    output += "\r\n";

    for (const auto& [name, value] : response.headers()) {
        if (name == "content-length" || name == "connection") {
            continue;
        }

        validate_header(name, value);
        output += name;
        output += ": ";
        output += value;
        output += "\r\n";
    }

    output += "content-length: ";
    output += std::to_string(response.body().size());
    output += "\r\n";
    output += "connection: close\r\n";
    output += "\r\n";

    if (!omit_body) {
        output.append(response.body());
    }

    return output;
}

bool request_keep_alive(const Request& request) noexcept {
    const auto connection = request.header("connection");
    if (connection == "close" || connection == "Close") return false;
    return true;
}

} // namespace gungnir::http::wire
