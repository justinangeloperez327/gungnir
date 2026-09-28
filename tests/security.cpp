#include <cassert>

#include <gungnir/security/security.hpp>

int main() {
    using namespace gungnir;

    assert(security::constant_time_equal("token", "token"));
    assert(!security::constant_time_equal("token", "other"));
    assert(!security::constant_time_equal("token", "token-longer"));

    assert(security::valid_header_value("application/json"));
    assert(!security::valid_header_value("safe\r\ninjected: true"));

    assert(security::valid_cookie_name("session_id"));
    assert(!security::valid_cookie_name(""));
    assert(!security::valid_cookie_name("session id"));
    assert(!security::valid_cookie_name("session=id"));

    const auto first =
        security::random_token(16);

    const auto second =
        security::random_token(16);

    assert(first.size() == 32);
    assert(second.size() == 32);
    assert(first != second);

    for (const auto character : first) {
        assert(
            (
                character >= '0' &&
                character <= '9'
            ) ||
            (
                character >= 'a' &&
                character <= 'f'
            )
        );
    }

    return 0;
}
