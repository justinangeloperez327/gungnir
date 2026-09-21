#include <cassert>
#include <string>

#include <gungnir/gungnir.hpp>

int main() {
    using namespace gungnir;

    Request request{
        http::Method::post,
        "/users",
        "name=Justin&email=justin%40example.com&age=30"
    };

    request.set_header(
        "Content-Type",
        "application/x-www-form-urlencoded"
    );

    const auto validated = request.validate({
        {"name", "required|string|min:3|max:50"},
        {"email", "required|email"},
        {"age", "required|integer|min:18|max:100"},
        {"nickname", "sometimes|string|max:20"}
    });

    assert(validated.size() == 3);
    assert(validated.at("name") == "Justin");
    assert(
        validated.at("email") ==
        "justin@example.com"
    );
    assert(validated.at("age") == "30");

    Request invalid{
        http::Method::post,
        "/users",
        "name=&email=invalid&age=10"
    };

    invalid.set_header(
        "Content-Type",
        "application/x-www-form-urlencoded"
    );

    bool failed = false;

    try {
        (void) invalid.validate({
            {"name", "required"},
            {"email", "required|email"},
            {"age", "integer|min:18"}
        });
    } catch (
        const validation::ValidationException& exception
    ) {
        failed = true;
        assert(exception.errors().contains("name"));
        assert(exception.errors().contains("email"));
        assert(exception.errors().contains("age"));
    }

    assert(failed);

    Request password{
        http::Method::post,
        "/register",
        "password=secret123&password_confirmation=secret123&terms=yes"
    };

    password.set_header(
        "Content-Type",
        "application/x-www-form-urlencoded"
    );

    const auto confirmed = password.validate({
        {"password", "required|confirmed|min:8"},
        {"terms", "accepted"}
    });

    assert(
        confirmed.at("password") ==
        "secret123"
    );

    return 0;
}
