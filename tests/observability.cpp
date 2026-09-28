#include <cassert>
#include <condition_variable>
#include <memory>
#include <mutex>

#include <gungnir/core/executor.hpp>
#include <gungnir/observability/observability.hpp>

int main() {
    using namespace gungnir;
    using namespace gungnir::observability;

    Tracer noop;
    assert(
        !noop.start_span("noop").valid()
    );

    auto sink =
        std::make_shared<
            MemorySpanSink
        >();

    auto tracer =
        std::make_shared<Tracer>(
            sink
        );

    set_global_tracer(tracer);

    auto root =
        tracer->start_span(
            "request",
            {
                {
                    "http.method",
                    "GET"
                }
            }
        );

    assert(root.valid());

    const auto root_context =
        root.context();

    assert(root_context.valid());

    {
        auto root_scope =
            root.scope();

        assert(
            current_context().trace_id ==
            root_context.trace_id
        );

        auto child =
            tracer->start_span(
                "database.query"
            );

        const auto child_context =
            child.context();

        assert(
            child_context.trace_id ==
            root_context.trace_id
        );

        assert(
            child_context.span_id !=
            root_context.span_id
        );

        child
            .attribute(
                "db.system",
                "sqlite"
            )
            .status(
                SpanStatus::ok
            );

        child.end();
    }

    assert(
        !current_context().valid()
    );

    Executor executor{1};
    executor.start();

    std::mutex mutex;
    std::condition_variable ready;
    bool completed = false;

    {
        auto root_scope =
            root.scope();

        executor.post(
            [&, tracer] {
                const auto inherited =
                    current_context();

                assert(
                    inherited.trace_id ==
                    root_context.trace_id
                );

                assert(
                    inherited.span_id ==
                    root_context.span_id
                );

                auto async_child =
                    tracer->start_span(
                        "executor.work"
                    );

                async_child.end();

                {
                    std::lock_guard lock{
                        mutex
                    };

                    completed = true;
                }

                ready.notify_all();
            }
        );
    }

    {
        std::unique_lock lock{
            mutex
        };

        ready.wait(
            lock,
            [&] {
                return completed;
            }
        );
    }

    executor.stop();
    executor.join();

    root.status(
        SpanStatus::ok
    );

    root.end();

    const auto spans =
        sink->spans();

    assert(spans.size() == 3);

    bool saw_database = false;
    bool saw_executor = false;
    bool saw_root = false;

    for (const auto& span : spans) {
        assert(
            span.trace_id ==
            root_context.trace_id
        );

        assert(
            span.ended_at >=
            span.started_at
        );

        if (
            span.name ==
            "database.query"
        ) {
            saw_database = true;

            assert(
                span.parent_span_id ==
                root_context.span_id
            );

            assert(
                span.status ==
                SpanStatus::ok
            );

            assert(
                span.attributes.at(
                    "db.system"
                ) ==
                "sqlite"
            );
        } else if (
            span.name ==
            "executor.work"
        ) {
            saw_executor = true;

            assert(
                span.parent_span_id ==
                root_context.span_id
            );
        } else if (
            span.name ==
            "request"
        ) {
            saw_root = true;

            assert(
                span.parent_span_id
                    .empty()
            );

            assert(
                span.status ==
                SpanStatus::ok
            );
        }
    }

    assert(saw_database);
    assert(saw_executor);
    assert(saw_root);

    tracer->flush();
    tracer->shutdown();

    set_global_tracer(nullptr);

    return 0;
}
