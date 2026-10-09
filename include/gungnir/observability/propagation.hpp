#pragma once

#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>

#include <gungnir/core/execution_context.hpp>

namespace gungnir::observability {

[[nodiscard]] inline bool valid_trace_id(std::string_view value, std::size_t size) noexcept {
    if (value.size() != size) return false;
    bool nonzero = false;
    for (const char digit : value) {
        if (!((digit >= '0' && digit <= '9') || (digit >= 'a' && digit <= 'f'))) return false;
        nonzero = nonzero || digit != '0';
    }
    return nonzero;
}

// The native tracer is always-on when a sink is configured. Flags are validated
// here; they are not a remote sampling instruction. v00 identity propagation is
// deliberately separate from vendor tracestate and baggage.
[[nodiscard]] inline std::optional<TraceContext> parse_traceparent(std::string_view value) {
    if (value.size() != 55 || !value.starts_with("00-") || value[35] != '-' || value[52] != '-') return {};
    if (!valid_trace_id(value.substr(3, 32), 32) || !valid_trace_id(value.substr(36, 16), 16)) return {};
    for (const char digit : value.substr(53, 2))
        if (!((digit >= '0' && digit <= '9') || (digit >= 'a' && digit <= 'f'))) return {};
    return TraceContext{std::string{value.substr(3, 32)}, std::string{value.substr(36, 16)}};
}

[[nodiscard]] inline std::string format_traceparent(const TraceContext& context, bool sampled = true) {
    if (!valid_trace_id(context.trace_id, 32) || !valid_trace_id(context.span_id, 16))
        throw std::invalid_argument("Trace context contains an invalid identifier");
    return "00-" + context.trace_id + "-" + context.span_id + (sampled ? "-01" : "-00");
}

} // namespace gungnir::observability
