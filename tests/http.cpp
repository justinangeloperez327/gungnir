#include <cassert>
#include <string>

#include <gungnir/http/message.hpp>

int main() {
    using namespace gungnir::http;

    const std::string raw =
        "POST /users?active=1 HTTP/1.1\r\n"
        "Host: localhost\r\n"
        "Content-Type: application/json\r\n"
        "Content-Length: 14\r\n"
        "\r\n"
        "{\"name\":\"Ana\"}";

    auto request = wire::parse_request(raw);

    assert(request.method() == Method::post);
    assert(request.path() == "/users");
    assert(request.body() == "{\"name\":\"Ana\"}");
    assert(request.header("content-type") == "application/json");
    assert(request.header("Content-Type") == "application/json");
    assert(request.header("HOST") == "localhost");

    auto response = Response::text("ok", 201);
    response.header("X-Test", "yes");

    const auto payload = wire::serialize_response(response);

    assert(payload.find("HTTP/1.1 201 Created\r\n") == 0);
    assert(
        payload.find("content-type: text/plain; charset=utf-8\r\n") !=
        std::string::npos
    );
    assert(
        payload.find("x-test: yes\r\n") !=
        std::string::npos
    );
    assert(
        payload.find("content-length: 2\r\n") !=
        std::string::npos
    );
    assert(
        payload.find("connection: close\r\n") !=
        std::string::npos
    );
    assert(payload.ends_with("\r\n\r\nok"));

    const auto head_payload = wire::serialize_response(response, true);
    assert(head_payload.ends_with("\r\n\r\n"));
    assert(!head_payload.ends_with("\r\n\r\nok"));

    return 0;
}
