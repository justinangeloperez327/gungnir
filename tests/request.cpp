#include <cassert>
#include <string>
#include <unordered_map>

#include <gungnir/gungnir.hpp>
#include <gungnir/http/message.hpp>

class ExampleModel {
public:
    gungnir::model::AttributeMap attributes() const {
        return {
            {"id", gungnir::Int64{7}},
            {"name", gungnir::String{"Ana"}}
        };
    }
};

int main() {
    using namespace gungnir;

    const std::string raw_form =
        "POST /users?source=web&name=Query+Name HTTP/1.1\r\n"
        "Host: localhost\r\n"
        "Content-Type: application/x-www-form-urlencoded\r\n"
        "Cookie: session=abc123; theme=dark\r\n"
        "Content-Length: 25\r\n"
        "\r\n"
        "name=Form+Name&role=admin";

    auto form_request =
        http::wire::parse_request(raw_form);

    assert(form_request.path() == "/users");
    assert(
        form_request.target() ==
        "/users?source=web&name=Query+Name"
    );
    assert(form_request.query("source") == "web");
    assert(
        form_request.query("name") ==
        "Query Name"
    );
    assert(
        form_request.input("name") ==
        "Form Name"
    );
    assert(form_request.input("role") == "admin");
    assert(
        form_request.cookie("session") ==
        "abc123"
    );
    assert(
        form_request.cookie("theme") ==
        "dark"
    );
    assert(form_request.has("source"));
    assert(form_request.has("role"));

    const auto only =
        form_request.only({"name", "source"});

    assert(only.size() == 2);
    assert(only.at("name") == "Form Name");
    assert(only.at("source") == "web");

    const auto except =
        form_request.except({"role"});

    assert(!except.contains("role"));

    const std::string json_body =
        "{\"name\":\"Json Name\","
        "\"active\":true,"
        "\"count\":3}";

    Request json_request{
        http::Method::post,
        "/users?name=Query",
        json_body
    };

    json_request.set_header(
        "Content-Type",
        "application/json; charset=utf-8"
    );

    assert(
        json_request.input("name") ==
        "Json Name"
    );
    assert(
        json_request.input("active") ==
        "true"
    );
    assert(
        json_request.input("count") ==
        "3"
    );
    assert(
        json_request.json()
            .get("name")
            ->string() ==
        "Json Name"
    );

    const auto json_response = Response::json(
        std::unordered_map<
            std::string,
            std::string
        >{
            {"status", "ok"}
        },
        201
    );

    assert(json_response.status() == 201);
    assert(
        json_response.header("Content-Type") ==
        "application/json; charset=utf-8"
    );
    assert(
        json_response.body().find(
            "\"status\":\"ok\""
        ) != std::string::npos
    );

    const auto model_response =
        Response::json(ExampleModel{});

    assert(
        model_response.body().find(
            "\"id\":7"
        ) != std::string::npos
    );
    assert(
        model_response.body().find(
            "\"name\":\"Ana\""
        ) != std::string::npos
    );

    const auto redirect =
        Response::redirect("/login");

    assert(redirect.status() == 302);
    assert(
        redirect.header("Location") ==
        "/login"
    );

    assert(
        Response::no_content().status() == 204
    );

    return 0;
}
