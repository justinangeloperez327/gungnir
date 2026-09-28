#pragma once

#include <chrono>
#include <exception>
#include <string>

#include <gungnir/mail/message.hpp>
#include <gungnir/observability/metrics.hpp>
#include <gungnir/observability/trace.hpp>

namespace gungnir::mail {

class Transport {
public:
    virtual ~Transport() = default;
    virtual void send(
        const Message& message
    ) = 0;
};

class Mailer {
public:
    explicit Mailer(
        Transport& transport
    )
        : transport_(&transport) {}

    void send(
        const Message& message
    ) {
        const auto metric_started =
            std::chrono::steady_clock::now();

        const auto recipient_count =
            message.recipients().size() +
            message.cc_recipients().size() +
            message.bcc_recipients().size();

        auto span =
            observability::
                global_tracer()
                ->start_span(
                    "mail.send",
                    {
                        {
                            "messaging.system",
                            "email"
                        },
                        {
                            "messaging.operation",
                            "send"
                        },
                        {
                            "messaging.destination.count",
                            std::to_string(
                                recipient_count
                            )
                        }
                    }
                );

        auto scope =
            span.valid()
                ? span.scope()
                : observability::Scope{};

        try {
            transport_->send(
                message
            );

            span.status(
                observability::
                    SpanStatus::ok
            );

            span.end();

            record_metrics(
                metric_started,
                recipient_count,
                "ok"
            );
        } catch (
            const std::exception& error
        ) {
            span.error(
                error.what()
            );

            span.end();

            record_metrics(
                metric_started,
                recipient_count,
                "error"
            );

            throw;
        } catch (...) {
            span.error(
                "Unknown mail transport exception"
            );

            span.end();

            record_metrics(
                metric_started,
                recipient_count,
                "error"
            );

            throw;
        }
    }

private:
    static void record_metrics(
        std::chrono::steady_clock::time_point started,
        std::size_t recipient_count,
        std::string outcome
    ) {
        auto attributes =
            observability::Attributes{
                {
                    "messaging.system",
                    "email"
                },
                {
                    "outcome",
                    std::move(outcome)
                }
            };

        const auto elapsed =
            std::chrono::duration<
                double,
                std::milli
            >(
                std::chrono::
                    steady_clock::now() -
                started
            ).count();

        auto meter =
            observability::
                global_meter();

        meter->histogram(
            "mail.send.duration"
        ).record(
            elapsed,
            attributes
        );

        meter->counter(
            "mail.send.count"
        ).add(
            1.0,
            attributes
        );

        meter->histogram(
            "mail.recipient.count"
        ).record(
            static_cast<double>(
                recipient_count
            ),
            std::move(attributes)
        );
    }

    Transport* transport_;
};

} // namespace gungnir::mail
