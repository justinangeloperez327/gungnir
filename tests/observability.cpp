#include <cassert>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <sstream>
#include <thread>
#include <vector>

#include <gungnir/cache/cache.hpp>
#include <gungnir/core/executor.hpp>
#include <gungnir/database/database.hpp>
#include <gungnir/mail/mail.hpp>
#include <gungnir/observability/observability.hpp>
#include <gungnir/logging/json_stream_sink.hpp>
#include <gungnir/logging/service.hpp>
#include <gungnir/http/message.hpp>
#include <gungnir/queue/queue.hpp>
#include <gungnir/scheduler/scheduling.hpp>

int main() {
    using namespace gungnir;
    using namespace gungnir::observability;

    const TraceContext incoming{"0123456789abcdef0123456789abcdef", "0123456789abcdef"};
    assert(parse_traceparent(format_traceparent(incoming))->trace_id == incoming.trace_id);
    assert(parse_traceparent(format_traceparent(incoming, false))->span_id == incoming.span_id);
    for (const auto& invalid : {"", "00-00000000000000000000000000000000-0123456789abcdef-01",
             "00-0123456789abcdef0123456789abcdef-0000000000000000-01",
             "00-0123456789abcdef0123456789abcdeF-0123456789abcdef-01",
             "00-0123456789abcdef0123456789abcdef-0123456789abcdef-xx",
             "01-0123456789abcdef0123456789abcdef-0123456789abcdef-01"})
        assert(!parse_traceparent(invalid));
    bool bad_context = false;
    try { static_cast<void>(format_traceparent({"invalid", "invalid"})); }
    catch (const std::invalid_argument&) { bad_context = true; }
    assert(bad_context);
    const auto duplicate = http::wire::parse_request("GET / HTTP/1.1\r\nHost: localhost\r\ntraceparent: " +
        format_traceparent(incoming) + "\r\nTraceparent: " + format_traceparent(incoming) + "\r\n\r\n");
    assert(!parse_traceparent(duplicate.header("traceparent")));

    std::ostringstream output;
    auto json_sink = std::make_shared<logging::JsonStreamSink>(output, 1024);
    auto logger = std::make_shared<logging::Logger>(); logger->sink(json_sink);
    logging::Service log{logger};
    {
        auto context = activate(incoming);
        log.info("Quoted \"line\"\nUnicode: Freyja 🦅", http::Json::object({{"Authorization", "private-bearer"}, {"request_id", "test-request"}}));
    }
    std::vector<std::thread> writers;
    for (int index = 0; index < 4; ++index)
        writers.emplace_back([&] { for (int count = 0; count < 20; ++count) log.info("parallel"); });
    for (auto& writer : writers) writer.join();
    logger->info(std::string(2000, 'x'));
    assert(json_sink->dropped() == 1);
    std::istringstream lines{output.str()};
    std::string line;
    int count = 0;
    while (std::getline(lines, line)) {
        const auto record = http::Json::parse(line);
        assert(record.get("level")->string() == "info");
        if (count++ == 0) {
            assert(record.get("message")->string() == "Quoted \"line\"\nUnicode: Freyja 🦅");
            const auto& context = *record.get("context");
            assert(context.get("trace_id")->string() == incoming.trace_id);
            assert(context.get("span_id")->string() == incoming.span_id);
            assert(context.get("Authorization")->string() == "[redacted]");
        }
    }
    assert(count == 81 && output.str().find("private-bearer") == std::string::npos);
    std::ostringstream broken; broken.setstate(std::ios::badbit);
    auto broken_sink = std::make_shared<logging::JsonStreamSink>(broken);
    logging::Logger isolated; isolated.sink(broken_sink); isolated.info("sink failure");
    assert(broken_sink->dropped() == 1);
    struct FailedBuffer : std::streambuf {
        std::streamsize xsputn(const char*, std::streamsize) override { return 0; }
    } failed_buffer;
    std::ostream throwing_stream{&failed_buffer}; throwing_stream.exceptions(std::ios::badbit | std::ios::failbit);
    auto throwing_sink = std::make_shared<logging::JsonStreamSink>(throwing_stream);
    logging::Logger throwing_logger; throwing_logger.sink(throwing_sink); throwing_logger.info("throwing stream");
    assert(throwing_sink->dropped() == 1);

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

    auto metric_sink =
        std::make_shared<
            MemoryMetricSink
        >();

    auto meter =
        std::make_shared<Meter>(
            metric_sink
        );

    set_global_meter(meter);

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

        meter->counter(
            "test.counter"
        ).add(
            2.0,
            {
                {
                    "kind",
                    "test"
                }
            }
        );

        meter->gauge(
            "test.gauge"
        ).set(
            3.0
        );

        meter->histogram(
            "test.histogram"
        ).record(
            4.0
        );
    }

    const auto primitive_metrics =
        metric_sink->points();

    assert(
        primitive_metrics.size() == 3
    );

    assert(
        primitive_metrics[0].kind ==
        MetricKind::counter
    );

    assert(
        primitive_metrics[1].kind ==
        MetricKind::gauge
    );

    assert(
        primitive_metrics[2].kind ==
        MetricKind::histogram
    );

    for (
        const auto& point :
        primitive_metrics
    ) {
        assert(
            point.trace_id ==
            root_context.trace_id
        );

        assert(
            point.span_id ==
            root_context.span_id
        );
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
    metric_sink->clear();

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

        cache::MemoryStore
            cache_store;

        cache::Repository
            cache{
                cache_store
            };

        assert(
            !cache.get("missing")
        );

        cache.put(
            "present",
            "value"
        );

        assert(
            cache.get("present") ==
            std::optional<std::string>{
                "value"
            }
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

    const auto framework_metrics =
        metric_sink->points();

    bool saw_db_duration = false;
    bool saw_queue_duration = false;
    bool saw_scheduler_duration = false;
    bool saw_mail_duration = false;
    bool saw_cache_hit = false;
    bool saw_cache_miss = false;

    for (
        const auto& point :
        framework_metrics
    ) {
        if (
            point.name ==
            "db.client.operation.duration"
        ) {
            saw_db_duration = true;
            assert(
                point.kind ==
                MetricKind::histogram
            );
        } else if (
            point.name ==
            "messaging.process.duration"
        ) {
            saw_queue_duration = true;
        } else if (
            point.name ==
            "scheduler.task.duration"
        ) {
            saw_scheduler_duration = true;
        } else if (
            point.name ==
            "mail.send.duration"
        ) {
            saw_mail_duration = true;
        } else if (
            point.name ==
            "cache.request.count"
        ) {
            const auto result =
                point.attributes.at(
                    "cache.result"
                );

            if (result == "hit") {
                saw_cache_hit = true;
            } else if (
                result == "miss"
            ) {
                saw_cache_miss = true;
            }
        }
    }

    assert(saw_db_duration);
    assert(saw_queue_duration);
    assert(saw_scheduler_duration);
    assert(saw_mail_duration);
    assert(saw_cache_hit);
    assert(saw_cache_miss);

    meter->flush();
    meter->shutdown();

    tracer->flush();
    tracer->shutdown();

    set_global_tracer(nullptr);
    set_global_meter(nullptr);

    return 0;
}
