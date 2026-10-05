#pragma once

#include <memory>
#include <stdexcept>
#include <gungnir/logging/logger.hpp>
#include <gungnir/observability/attributes.hpp>
#include <gungnir/observability/trace.hpp>

namespace gungnir::logging {
class Service {
public:
    explicit Service(std::shared_ptr<Logger> logger) : logger_(std::move(logger)) {
        if (!logger_) throw std::invalid_argument("Logging requires a logger");
    }
    Service(const std::shared_ptr<Service>& service) : Service(service ? service->logger_ : nullptr) {}
    void debug(std::string message, const http::Json& fields = http::Json::object({})) const { write(Level::debug, std::move(message), fields); }
    void info(std::string message, const http::Json& fields = http::Json::object({})) const { write(Level::info, std::move(message), fields); }
    void warning(std::string message, const http::Json& fields = http::Json::object({})) const { write(Level::warning, std::move(message), fields); }
    void error(std::string message, const http::Json& fields = http::Json::object({})) const { write(Level::error, std::move(message), fields); }
private:
    void write(Level level, std::string message, const http::Json& fields) const {
        auto context = observability::attributes(fields);
        const auto trace = observability::current_context();
        if (trace.valid()) {
            context.insert_or_assign("trace_id", trace.trace_id);
            context.insert_or_assign("span_id", trace.span_id);
        }
        logger_->log(level, std::move(message), std::move(context));
    }
    std::shared_ptr<Logger> logger_;
};
}
