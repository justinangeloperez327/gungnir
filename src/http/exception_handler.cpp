#include <gungnir/http/exception_handler.hpp>
#include <gungnir/http/errors.hpp>
#include <gungnir/auth/authorize.hpp>
#include <gungnir/model/errors.hpp>
#include <gungnir/validation/exception.hpp>
#include <gungnir/logging/json_stream_sink.hpp>
#include <gungnir/core/container.hpp>
#include <iostream>
#include <utility>

namespace gungnir::http {
namespace {
String escape_html(std::string_view value) {
    String result;
    for (const auto c : value) {
        switch (c) {
        case '&': result += "&amp;"; break;
        case '<': result += "&lt;"; break;
        case '>': result += "&gt;"; break;
        case '"': result += "&quot;"; break;
        case '\'': result += "&#39;"; break;
        default: result += c;
        }
    }
    return result;
}
String status_message(int status) {
    switch (status) {
    case 400: return "Bad Request";
    case 401: return "Unauthenticated";
    case 403: return "Forbidden";
    case 404: return "Not Found";
    case 405: return "Method Not Allowed";
    case 409: return "Conflict";
    case 419: return "Page Expired";
    case 422: return "Validation failed";
    case 429: return "Too Many Requests";
    case 503: return "Service Unavailable";
    default: return status >= 500 ? "Internal Server Error" : "Request failed";
    }
}
std::shared_ptr<logging::Logger> fallback_logger() {
    static const auto logger = [] {
        auto value = std::make_shared<logging::Logger>();
        value->sink(std::make_shared<logging::JsonStreamSink>(std::clog));
        return value;
    }();
    return logger;
}
Response default_response(Request& request, std::exception_ptr exception, const errors::Error& error, bool debug) {
    const auto status = error.status();
    const auto message = (status >= 500 && !debug) || error.message().empty() || error.type() == "ModelNotFoundError" ? status_message(status) : error.message();
    Json::Object payload{{"message", Json{message}}};
    // Preserve validation's structured field errors without submitting values.
    if (const auto validation = errors::catch_as<validation::ValidationException>(exception))
        payload.insert_or_assign("errors", make_json(validation->errors()));
    if (debug) {
        payload.emplace("exception", error.type());
        if (!error.file().empty()) {
            payload.emplace("file", error.file());
            payload.emplace("line", Json{static_cast<UInt64>(error.line())});
        }
        payload.emplace("trace", make_json(error.trace()));
    }
    if (request.expects_json()) return Response::json(Json::object(std::move(payload)), status);
    if (request.header("Accept").find("text/html") == std::string_view::npos)
        return Response::text(message, status);
    String html = "<!doctype html><html lang=\"en\"><head><meta charset=\"utf-8\"><meta name=\"viewport\" content=\"width=device-width,initial-scale=1\"><title>" + std::to_string(status) + " " + escape_html(status_message(status)) + "</title></head><body><main><h1>" + std::to_string(status) + " " + escape_html(status_message(status)) + "</h1><p>" + escape_html(message) + "</p>";
    if (debug) {
        html += "<h2>" + escape_html(error.type()) + "</h2>";
        if (!error.file().empty()) html += "<p>" + escape_html(error.file()) + ":" + std::to_string(error.line()) + "</p>";
        html += "<h2>Stack frames</h2><pre>";
        for (const auto& frame : error.trace()) html += escape_html(frame) + "\n";
        html += "</pre>";
    }
    html += "</main></body></html>";
    auto response = Response::html(std::move(html), status);
    response.header("Content-Security-Policy", "default-src 'none'; frame-ancestors 'none'");
    response.header("X-Content-Type-Options", "nosniff");
    return response;
}
}
ExceptionHandler& ExceptionHandler::debug(bool enabled) { debug_ = [enabled] { return enabled; }; return *this; }
ExceptionHandler& ExceptionHandler::debug_when(std::function<bool()> resolver) { debug_ = std::move(resolver); return *this; }
ExceptionHandler& ExceptionHandler::debug_allowed_when(std::function<bool()> policy) { debug_allowed_ = std::move(policy); return *this; }
ExceptionHandler& ExceptionHandler::logger(std::shared_ptr<logging::Logger> value) { logger_ = std::move(value); return *this; }
ExceptionHandler& ExceptionHandler::on_report(Reporter callback) {
    reporters_.push_back([callback=std::move(callback)](Request& request, std::exception_ptr exception) { return callback(request, errors::Error::from_exception(exception)); }); return *this;
}
ExceptionHandler& ExceptionHandler::on_render(Renderer callback) {
    renderers_.push_back([callback=std::move(callback)](Request& request, std::exception_ptr exception) { return callback(request, errors::Error::from_exception(exception)); }); return *this;
}
void ExceptionHandler::write_log(Request& request, const errors::Error& error) const noexcept {
    try {
        auto logger = logger_;
        if (!logger && request.has_services()) {
            try { logger = request.services().resolve<logging::Logger>(); } catch (...) {}
        }
        if (!logger || !logger->has_sinks()) logger = fallback_logger();
        logging::Context context{{"exception", error.type()}, {"status", std::to_string(error.status())}, {"method", String{to_string(request.method())}}};
        String stack;
        for (const auto& frame : error.trace()) { stack += frame; stack += '\n'; }
        context.emplace("stack",std::move(stack));
        // Do not include query strings, headers, request bodies or submitted values.
        if (!error.file().empty()) { context.emplace("file", error.file()); context.emplace("line", std::to_string(error.line())); }
        const auto trace = gungnir::detail::current_execution_context()->trace;
        if (trace.valid()) { context.emplace("trace_id", trace.trace_id); context.emplace("span_id", trace.span_id); }
        logger->error(error.message(), std::move(context));
    } catch (...) {
        // Logging must never replace the original failure.
    }
}
void ExceptionHandler::report(Request& request, std::exception_ptr exception) const noexcept {
    try {
        const auto ignored = ignored_;
        for (const auto& matches : ignored) if (matches(exception)) return;
        const auto reporters = reporters_;
        for (const auto& reporter : reporters) {
            try { if (reporter(request, exception)) return; }
            catch (...) { write_log(request, errors::Error::from_exception(std::current_exception())); }
        }
        const auto error = errors::Error::from_exception(exception);
        if (error.status() >= 500) write_log(request, error);
    } catch (...) {}
}
Response ExceptionHandler::render(Request& request, std::exception_ptr exception) const {
    report(request, exception);
    const auto renderers = renderers_;
    for (const auto& renderer : renderers) {
        try { if (auto response = renderer(request, exception)) return std::move(*response); }
        catch (...) { write_log(request, errors::Error::from_exception(std::current_exception())); }
    }
    bool debug = false;
    try { debug = debug_ && debug_() && debug_allowed_ && debug_allowed_(); } catch (...) {}
    auto response = default_response(request, exception, errors::Error::from_exception(exception), debug);
    response.header("Cache-Control", "no-store");
    return response;
}
}
