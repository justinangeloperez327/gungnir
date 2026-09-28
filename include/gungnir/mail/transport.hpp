#pragma once

#include <exception>
#include <string>

#include <gungnir/mail/message.hpp>
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
        } catch (
            const std::exception& error
        ) {
            span.error(
                error.what()
            );

            span.end();
            throw;
        } catch (...) {
            span.error(
                "Unknown mail transport exception"
            );

            span.end();
            throw;
        }
    }

private:
    Transport* transport_;
};

} // namespace gungnir::mail
