#pragma once

#include <utility>

#include <gungnir/core/types.hpp>
#include <gungnir/http/request.hpp>
#include <gungnir/http/response.hpp>
#include <gungnir/view/data.hpp>

namespace gungnir {

using Request = http::Request;
using Response = http::Response;

class Controller {
public:
    virtual ~Controller() = default;

protected:
    [[nodiscard]] static Response response(
        String body = {},
        Integer status = 200
    );

    [[nodiscard]] static Response text(
        String body,
        Integer status = 200
    );

    [[nodiscard]] static Response json(
        String body,
        Integer status = 200
    );

    template <typename T>
    [[nodiscard]] static Response json(
        T&& value,
        Integer status = 200
    ) {
        return Response::json(
            std::forward<T>(value),
            status
        );
    }

    [[nodiscard]] static Response view(
        String name,
        gungnir::view::Data data = {},
        Integer status = 200
    );

    [[nodiscard]] static Response html(
        String body,
        Integer status = 200
    );

    [[nodiscard]] static Response download(
        String body,
        String filename,
        String content_type = "application/octet-stream",
        Integer status = 200
    );

    [[nodiscard]] static Response no_content();

    [[nodiscard]] static Response redirect(
        String location,
        Integer status = 302
    );
};

} // namespace gungnir
