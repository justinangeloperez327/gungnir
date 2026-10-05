#pragma once

#include <cmath>
#include <memory>
#include <stdexcept>
#include <gungnir/observability/attributes.hpp>
#include <gungnir/observability/metrics.hpp>

namespace gungnir::observability {
class Service {
public:
    Service(std::shared_ptr<Tracer> tracer, std::shared_ptr<Meter> meter)
        : tracer_(std::move(tracer)), meter_(std::move(meter)) {
        if (!tracer_ || !meter_) throw std::invalid_argument("Telemetry requires a tracer and meter");
    }
    Service(const std::shared_ptr<Service>& service)
        : Service(service ? service->tracer_ : nullptr, service ? service->meter_ : nullptr) {}
    [[nodiscard]] Span span(std::string name, const http::Json& fields = http::Json::object({})) const {
        require_name(name);
        return tracer_->start_span(std::move(name), attributes(fields));
    }
    void counter(std::string name, double value = 1, const http::Json& fields = http::Json::object({})) const {
        require_name(name); require_value(value, true);
        meter_->counter(std::move(name)).add(value, attributes(fields));
    }
    void gauge(std::string name, double value, const http::Json& fields = http::Json::object({})) const {
        require_name(name); require_value(value, false);
        meter_->gauge(std::move(name)).set(value, attributes(fields));
    }
    void histogram(std::string name, double value, const http::Json& fields = http::Json::object({})) const {
        require_name(name); require_value(value, true);
        meter_->histogram(std::move(name)).record(value, attributes(fields));
    }
private:
    static void require_name(const std::string& name) {
        if (name.empty()) throw std::invalid_argument("Telemetry names cannot be empty");
    }
    static void require_value(double value, bool nonnegative) {
        if (!std::isfinite(value) || (nonnegative && value < 0)) throw std::invalid_argument("Telemetry value is out of range");
    }
    std::shared_ptr<Tracer> tracer_;
    std::shared_ptr<Meter> meter_;
};
inline Span span_attribute(const Span& span, std::string key, std::string value) {
    auto result = span; result.attribute(key, attribute_value(key, std::move(value))); return result;
}
inline Span span_error(const Span& span, std::string message) { auto result = span; result.error(std::move(message)); return result; }
inline void span_end(const Span& span) { auto result = span; result.end(); }
inline std::string span_trace_id(const Span& span) { return span.context().trace_id; }
inline std::string span_id(const Span& span) { return span.context().span_id; }
}
