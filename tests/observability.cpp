#include <cassert>
#include <condition_variable>
#include <memory>
#include <mutex>

#include <gungnir/core/executor.hpp>
#include <gungnir/database/database.hpp>
#include <gungnir/mail/mail.hpp>
#include <gungnir/observability/observability.hpp>
#include <gungnir/queue/queue.hpp>
#include <gungnir/scheduler/scheduling.hpp>

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

    sink->clear();

    auto application_root =
        tracer->start_span(
            "application.request"
        );

    const auto application_context =
        application_root.context();

    queue::MemoryDriver
        queue_driver;

    {
        auto application_scope =
            application_root.scope();

        auto driver =
            std::make_shared<
                database::CallbackDriver
            >(
                database::Backend::
                    postgresql,
                [](
                    const String&,
                    const std::vector<
                        model::AttributeValue
                    >&
                ) {
                    return
                        database::Result{};
                }
            );

        database::Connection connection{
            "primary",
            std::move(driver)
        };

        static_cast<void>(
            connection.execute(
                "select * from users where id = ?",
                {}
            )
        );

        queue_driver.push({
            "job-1",
            "mail.send",
            "42",
            0,
            1
        });

        scheduler::SystemClock clock;
        scheduler::Scheduler
            scheduler{clock};

        scheduler.every(
            "cleanup",
            std::chrono::seconds{10},
            [] {}
        );

        assert(
            scheduler.run_due() == 1
        );

        mail::MemoryTransport
            transport;

        mail::Mailer mailer{
            transport
        };

        mail::Message message;

        message
            .from({
                "sender@test.invalid",
                "Sender"
            })
            .to({
                "user@test.invalid",
                "User"
            })
            .subject("Trace")
            .text("Body");

        mailer.send(message);
    }

    application_root.end();

    queue::Worker queue_worker{
        queue_driver
    };

    queue_worker.handle(
        "mail.send",
        [](
            std::string_view
        ) {}
    );

    assert(
        queue_worker.run_one()
    );

    const auto framework_spans =
        sink->spans();

    bool saw_db = false;
    bool saw_queue = false;
    bool saw_scheduler = false;
    bool saw_mail = false;

    for (
        const auto& span :
        framework_spans
    ) {
        if (
            span.name ==
            "database.query"
        ) {
            saw_db = true;

            assert(
                span.trace_id ==
                application_context.trace_id
            );

            assert(
                span.parent_span_id ==
                application_context.span_id
            );

            assert(
                span.attributes.at(
                    "db.operation"
                ) ==
                "SELECT"
            );
        } else if (
            span.name ==
            "queue.job"
        ) {
            saw_queue = true;

            assert(
                span.trace_id ==
                application_context.trace_id
            );

            assert(
                span.parent_span_id ==
                application_context.span_id
            );
        } else if (
            span.name ==
            "scheduler.task"
        ) {
            saw_scheduler = true;

            assert(
                span.trace_id ==
                application_context.trace_id
            );
        } else if (
            span.name ==
            "mail.send"
        ) {
            saw_mail = true;

            assert(
                span.trace_id ==
                application_context.trace_id
            );
        }
    }

    assert(saw_db);
    assert(saw_queue);
    assert(saw_scheduler);
    assert(saw_mail);

    tracer->flush();
    tracer->shutdown();

    set_global_tracer(nullptr);

    return 0;
}
