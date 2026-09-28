#pragma once

#include <atomic>
#include <chrono>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>

#include <gungnir/security/random.hpp>

namespace gungnir::observability {

using Attributes =
    std::unordered_map<
        std::string,
        std::string
    >;

enum class SpanStatus {
    unset,
    ok,
    error
};

struct TraceContext {
    std::string trace_id;
    std::string span_id;

    [[nodiscard]]
    bool valid()
        const noexcept {
        return
            !trace_id.empty() &&
            !span_id.empty();
    }
};

struct SpanRecord {
    std::string trace_id;
    std::string span_id;
    std::string parent_span_id;
    std::string name;
    Attributes attributes;
    SpanStatus status{
        SpanStatus::unset
    };
    std::string status_message;
    std::chrono::system_clock::time_point
        started_at{};
    std::chrono::system_clock::time_point
        ended_at{};
};

class SpanSink {
public:
    virtual ~SpanSink() = default;

    virtual void export_span(
        const SpanRecord& span
    ) = 0;

    virtual void flush() {}
    virtual void shutdown() {}
};

class Tracer;

namespace detail {

inline thread_local TraceContext
    current_trace_context{};

[[nodiscard]]
inline std::mutex&
global_tracer_mutex() {
    static std::mutex mutex;
    return mutex;
}

[[nodiscard]]
inline std::shared_ptr<Tracer>&
global_tracer_storage() {
    static std::shared_ptr<Tracer>
        tracer;

    return tracer;
}

} // namespace detail

class Scope {
public:
    Scope() = default;

    explicit Scope(
        TraceContext context
    )
        : previous_(
            detail::
                current_trace_context
          ),
          active_(true) {
        detail::
            current_trace_context =
            std::move(context);
    }

    Scope(
        const Scope&
    ) = delete;

    Scope& operator=(
        const Scope&
    ) = delete;

    Scope(
        Scope&& other
    ) noexcept
        : previous_(
            std::move(
                other.previous_
            )
          ),
          active_(
            std::exchange(
                other.active_,
                false
            )
          ) {}

    Scope& operator=(
        Scope&& other
    ) noexcept {
        if (this == &other) {
            return *this;
        }

        reset();

        previous_ =
            std::move(
                other.previous_
            );

        active_ =
            std::exchange(
                other.active_,
                false
            );

        return *this;
    }

    ~Scope() {
        reset();
    }

    void reset()
        noexcept {
        if (!active_) {
            return;
        }

        detail::
            current_trace_context =
            std::move(previous_);

        active_ = false;
    }

private:
    TraceContext previous_;
    bool active_{false};
};

[[nodiscard]]
inline TraceContext current_context() {
    return
        detail::
            current_trace_context;
}

[[nodiscard]]
inline Scope activate(
    TraceContext context
) {
    return Scope{
        std::move(context)
    };
}

inline void clear_current_context()
    noexcept {
    detail::
        current_trace_context = {};
}

class Span {
public:
    Span() = default;

    Span(
        std::shared_ptr<SpanSink> sink,
        SpanRecord record
    )
        : state_(
            std::make_shared<State>(
                std::move(sink),
                std::move(record)
            )
          ) {}

    [[nodiscard]]
    bool valid()
        const noexcept {
        return
            static_cast<bool>(
                state_
            );
    }

    [[nodiscard]]
    TraceContext context()
        const {
        if (!state_) {
            return {};
        }

        std::lock_guard lock{
            state_->mutex
        };

        return {
            state_->record.trace_id,
            state_->record.span_id
        };
    }

    [[nodiscard]]
    Scope scope()
        const {
        return activate(
            context()
        );
    }

    Span& attribute(
        std::string key,
        std::string value
    ) {
        if (!state_) {
            return *this;
        }

        std::lock_guard lock{
            state_->mutex
        };

        if (!state_->ended) {
            state_->record
                .attributes
                .insert_or_assign(
                    std::move(key),
                    std::move(value)
                );
        }

        return *this;
    }

    Span& status(
        SpanStatus value,
        std::string message = {}
    ) {
        if (!state_) {
            return *this;
        }

        std::lock_guard lock{
            state_->mutex
        };

        if (!state_->ended) {
            state_->record.status =
                value;

            state_->record
                .status_message =
                std::move(message);
        }

        return *this;
    }

    Span& error(
        std::string message
    ) {
        return status(
            SpanStatus::error,
            std::move(message)
        );
    }

    void end()
        noexcept {
        if (!state_) {
            return;
        }

        std::shared_ptr<SpanSink> sink;
        SpanRecord record;

        {
            std::lock_guard lock{
                state_->mutex
            };

            if (state_->ended) {
                return;
            }

            state_->ended = true;
            state_->record.ended_at =
                std::chrono::
                    system_clock::now();

            sink = state_->sink;
            record = state_->record;
        }

        if (!sink) {
            return;
        }

        try {
            sink->export_span(record);
        } catch (...) {
        }
    }

    ~Span() {
        if (
            state_ &&
            state_.use_count() == 1
        ) {
            end();
        }
    }

private:
    struct State {
        State(
            std::shared_ptr<SpanSink>
                value_sink,
            SpanRecord value_record
        )
            : sink(
                std::move(value_sink)
              ),
              record(
                std::move(value_record)
              ) {}

        std::mutex mutex;
        std::shared_ptr<SpanSink>
            sink;
        SpanRecord record;
        bool ended{false};
    };

    std::shared_ptr<State> state_;
};

class Tracer {
public:
    Tracer() = default;

    explicit Tracer(
        std::shared_ptr<SpanSink> sink
    )
        : sink_(
            std::move(sink)
          ) {}

    Tracer& sink(
        std::shared_ptr<SpanSink> value
    ) {
        std::lock_guard lock{
            mutex_
        };

        sink_ =
            std::move(value);

        return *this;
    }

    [[nodiscard]]
    Span start_span(
        std::string name,
        Attributes attributes = {},
        std::optional<TraceContext>
            parent = std::nullopt
    ) const {
        if (name.empty()) {
            return {};
        }

        std::shared_ptr<SpanSink>
            sink;

        {
            std::lock_guard lock{
                mutex_
            };

            sink = sink_;
        }

        if (!sink) {
            return {};
        }

        if (!parent) {
            const auto current =
                current_context();

            if (current.valid()) {
                parent =
                    current;
            }
        }

        SpanRecord record;

        if (
            parent &&
            parent->valid()
        ) {
            record.trace_id =
                parent->trace_id;

            record.parent_span_id =
                parent->span_id;
        } else {
            record.trace_id =
                security::random_token(
                    16
                );
        }

        record.span_id =
            security::random_token(
                8
            );

        record.name =
            std::move(name);

        record.attributes =
            std::move(attributes);

        record.started_at =
            std::chrono::
                system_clock::now();

        return Span{
            std::move(sink),
            std::move(record)
        };
    }

    void flush()
        const {
        std::shared_ptr<SpanSink>
            sink;

        {
            std::lock_guard lock{
                mutex_
            };

            sink = sink_;
        }

        if (sink) {
            sink->flush();
        }
    }

    void shutdown()
        const {
        std::shared_ptr<SpanSink>
            sink;

        {
            std::lock_guard lock{
                mutex_
            };

            sink = sink_;
        }

        if (sink) {
            sink->shutdown();
        }
    }

private:
    mutable std::mutex mutex_;
    std::shared_ptr<SpanSink>
        sink_;
};

[[nodiscard]]
inline std::shared_ptr<Tracer>
global_tracer() {
    std::lock_guard lock{
        detail::
            global_tracer_mutex()
    };

    auto& tracer =
        detail::
            global_tracer_storage();

    if (!tracer) {
        tracer =
            std::make_shared<Tracer>();
    }

    return tracer;
}

inline void set_global_tracer(
    std::shared_ptr<Tracer> tracer
) {
    std::lock_guard lock{
        detail::
            global_tracer_mutex()
    };

    detail::
        global_tracer_storage() =
        tracer
            ? std::move(tracer)
            : std::make_shared<
                Tracer
              >();
}

} // namespace gungnir::observability
