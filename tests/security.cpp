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

    return 0;
}
