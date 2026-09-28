#pragma once

#include <optional>
#include <string>
#include <string_view>

#include <gungnir/observability/trace.hpp>

namespace gungnir::queue {

class Job {
public:
    virtual ~Job() = default;

    [[nodiscard]]
    virtual std::string_view name()
        const noexcept = 0;

    [[nodiscard]]
    virtual std::string payload()
        const = 0;
};

struct Envelope {
    std::string id;
    std::string name;
    std::string payload;
    unsigned attempts{0};
    unsigned max_attempts{1};

    // Driver-owned lease token. Application code should not
    // persist or manufacture this value.
    std::string reservation;

    // Trace propagation metadata. Drivers persist these fields
    // but application payloads remain transport-agnostic.
    std::string trace_id;
    std::string parent_span_id;
};

inline void inherit_trace(
    Envelope& job
) {
    if (!job.trace_id.empty()) {
        return;
    }

    const auto context =
        observability::
            current_context();

    if (!context.valid()) {
        return;
    }

    job.trace_id =
        context.trace_id;

    job.parent_span_id =
        context.span_id;
}

[[nodiscard]]
inline std::optional<
    observability::TraceContext
>
trace_parent(
    const Envelope& job
) {
    if (
        job.trace_id.empty() ||
        job.parent_span_id.empty()
    ) {
        return std::nullopt;
    }

    return
        observability::TraceContext{
            job.trace_id,
            job.parent_span_id
        };
}

} // namespace gungnir::queue
