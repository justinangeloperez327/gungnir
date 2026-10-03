#include "benchmark.hpp"

#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <string>

#include <gungnir/http/message.hpp>
#include <gungnir/routing/router.hpp>

namespace {

std::uint64_t parse_request_case(
    const std::string& raw,
    std::size_t iteration
) {
    const auto request =
        gungnir::http::wire::
            parse_request(raw);

    return
        static_cast<std::uint64_t>(
            request.path().size() +
            request.body().size() +
            request.headers().size() +
            request.query().size()
        ) +
        static_cast<std::uint64_t>(
            iteration
        );
}

std::uint64_t serialize_response_case(
    const gungnir::http::Response& response,
    std::size_t iteration
) {
    const auto wire =
        gungnir::http::wire::
            serialize_response(
                response,
                false,
                gungnir::http::
                    ConnectionDirective::
                        keep_alive
            );

    return
        static_cast<std::uint64_t>(
            wire.size()
        ) +
        static_cast<std::uint64_t>(
            iteration
        );
}

std::uint64_t dispatch_case(
    const gungnir::routing::Router& router,
    std::string target,
    std::size_t iteration
) {
    gungnir::http::Request request{
        gungnir::http::Method::get,
        std::move(target)
    };

    auto task =
        router.dispatch(request);

    task.run_inline();

    if (!task.done()) {
        throw std::runtime_error(
            "Synchronous routing benchmark unexpectedly suspended"
        );
    }

    auto awaiter =
        task.operator co_await();

    const auto response =
        awaiter.await_resume();

    return
        static_cast<std::uint64_t>(
            response.status()
        ) +
        static_cast<std::uint64_t>(
            response.body().size()
        ) +
        static_cast<std::uint64_t>(
            request.parameters().size()
        ) +
        static_cast<std::uint64_t>(
            iteration
        );
}

gungnir::routing::Router
make_router() {
    gungnir::routing::Router router;

    for (
        std::size_t index = 0;
        index < 128;
        ++index
    ) {
        router.get(
            "/static/" +
                std::to_string(index),
            [] {
                return
                    gungnir::http::
                        Response::text(
                            "static"
                        );
            }
        );
    }

    for (
        std::size_t index = 0;
        index < 64;
        ++index
    ) {
        router.get(
            "/teams/" +
                std::to_string(index) +
                "/users/{id}",
            [](
                gungnir::http::Request&
                    request
            ) {
                return
                    gungnir::http::
                        Response::text(
                            std::string{
                                request.parameter(
                                    "id"
                                )
                            }
                        );
            }
        ).where_number("id");
    }

    return router;
}

} // namespace

int main(
    int argc,
    char** argv
) {
    const auto config =
        gungnir::benchmark::
            parse_config(
                argc,
                argv
            );

    const std::string raw =
        "POST /api/users?active=1&page=2 HTTP/1.1\r\n"
        "Host: example.test\r\n"
        "Accept: application/json\r\n"
        "Content-Type: application/json\r\n"
        "User-Agent: gungnir-benchmark\r\n"
        "X-Request-ID: perf-baseline\r\n"
        "Content-Length: 28\r\n"
        "\r\n"
        "{\"name\":\"Ada\",\"active\":true}";

    auto response =
        gungnir::http::Response::json(
            "{\"ok\":true,\"items\":[1,2,3]}"
        );

    response
        .header(
            "cache-control",
            "no-store"
        )
        .header(
            "x-request-id",
            "perf-baseline"
        );

    const auto router =
        make_router();

    gungnir::benchmark::Suite suite{
        "http-routing",
        config
    };

    suite.run(
        "http1_parse_request",
        [&](std::size_t iteration) {
            return parse_request_case(
                raw,
                iteration
            );
        }
    );

    suite.run(
        "http1_serialize_response",
        [&](std::size_t iteration) {
            return
                serialize_response_case(
                    response,
                    iteration
                );
        }
    );

    suite.run(
        "router_static_tail_match",
        [&](std::size_t iteration) {
            return dispatch_case(
                router,
                "/static/127",
                iteration
            );
        }
    );

    suite.run(
        "router_parameterized_tail_match",
        [&](std::size_t iteration) {
            return dispatch_case(
                router,
                "/teams/63/users/4242",
                iteration
            );
        }
    );

    return suite.finish();
}
