#include <gungnir/http/exception_handler.hpp>

#include <exception>
#include <utility>

#include <gungnir/http/errors.hpp>
#include <gungnir/model/errors.hpp>
#include <gungnir/validation/exception.hpp>

namespace gungnir::http {

namespace {

Response message_response(
    Request& request,
    String message,
    Integer status
) {
    if (request.expects_json()) {
        return Response::json(
            Json::object({
                {"message", Json{std::move(message)}}
            }),
            status
        );
    }

    return Response::text(
        std::move(message),
        status
    );
}

} // namespace

Response ExceptionHandler::render(
    Request& request,
    std::exception_ptr error
) const noexcept {
    try {
        if (error) {
            std::rethrow_exception(error);
        }

        return message_response(
            request,
            "Internal Server Error",
            500
        );
    } catch (
        const validation::ValidationException& exception
    ) {
        if (request.expects_json()) {
            Json::Object payload;

            payload.insert_or_assign(
                "message",
                Json{"Validation failed"}
            );

            payload.insert_or_assign(
                "errors",
                make_json(exception.errors())
            );

            return Response::json(
                Json::object(std::move(payload)),
                422
            );
        }

        return Response::text(
            "Validation failed",
            422
        );
    } catch (const ModelNotFoundError&) {
        return message_response(
            request,
            "Not Found",
            404
        );
    } catch (const HttpException& exception) {
        return message_response(
            request,
            exception.what(),
            exception.status()
        );
    } catch (...) {
        return message_response(
            request,
            "Internal Server Error",
            500
        );
    }
}

} // namespace gungnir::http
