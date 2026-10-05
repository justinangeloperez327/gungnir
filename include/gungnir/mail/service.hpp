#pragma once

#include <gungnir/core/types.hpp>
#include <gungnir/mail/delivery.hpp>
#include <gungnir/mail/transport.hpp>
#include <gungnir/queue/service.hpp>
#include <memory>

namespace gungnir::mail {
class Pending {
public:
    Pending from(String email, String name = {}) const { auto result = *this; result.envelope_.from(checked({std::move(email), std::move(name)})); return result; }
    Pending to(String email, String name = {}) const { auto result = *this; result.envelope_.to(checked({std::move(email), std::move(name)})); return result; }
    Pending cc(String email, String name = {}) const { auto result = *this; result.envelope_.cc(checked({std::move(email), std::move(name)})); return result; }
    Pending bcc(String email, String name = {}) const { auto result = *this; result.envelope_.bcc(checked({std::move(email), std::move(name)})); return result; }
    Pending replyTo(String email, String name = {}) const { auto result = *this; result.envelope_.reply_to(checked({std::move(email), std::move(name)})); return result; }
    Pending attach(String filename, String contents, String content_type = "application/octet-stream") const {
        auto result = *this; delivery::header(filename); delivery::header(content_type);
        if (filename.empty() || content_type.empty()) throw std::invalid_argument("Invalid mail attachment");
        result.envelope_.attach({std::move(filename), std::move(contents), std::move(content_type)}); return result;
    }
    template<class Composer> void send(Composer composer) const { Mailer{*transport_}.send(compose(std::move(composer))); }
    template<class Composer> String queue(Composer composer, Int64 attempts = 1, bool after_commit = true) const {
        if (!queue_) throw std::invalid_argument("Queued mail requires a queue adapter");
        return queue_->dispatch(delivery::Job{compose(std::move(composer))}, attempts, after_commit);
    }
private:
    friend class Service;
    Pending(std::shared_ptr<Transport> transport, std::shared_ptr<queue::Service> queue, Address sender)
        : transport_(std::move(transport)), queue_(std::move(queue)) { envelope_.from(std::move(sender)); }
    static Address checked(Address value) { delivery::address(value); return value; }
    template<class Composer> Message compose(Composer composer) const {
        auto value = composer.message();
        if (value.sender().email.empty()) value.from(envelope_.sender());
        for (const auto& item : envelope_.recipients()) value.to(item);
        for (const auto& item : envelope_.cc_recipients()) value.cc(item);
        for (const auto& item : envelope_.bcc_recipients()) value.bcc(item);
        if (envelope_.reply_address()) value.reply_to(*envelope_.reply_address());
        for (const auto& item : envelope_.attachments()) value.attach(item);
        delivery::validate(value); return value;
    }
    std::shared_ptr<Transport> transport_;
    std::shared_ptr<queue::Service> queue_;
    Message envelope_;
};
class Service {
public:
    Service(const std::shared_ptr<Service>& owner) : Service(checked(owner)) {}
    Service(std::shared_ptr<Transport> transport, Address sender = {}, std::shared_ptr<queue::Service> queue = {})
        : transport_(std::move(transport)), queue_(std::move(queue)), sender_(std::move(sender)) {
        if (!transport_) throw std::invalid_argument("Mail is not configured");
    }
    Pending to(String email, String name = {}) const { return Pending{transport_, queue_, sender_}.to(std::move(email), std::move(name)); }
    void deliver(const Message& value) const { delivery::validate(value); Mailer{*transport_}.send(value); }
private:
    static const Service& checked(const std::shared_ptr<Service>& owner) {
        if (!owner) throw std::invalid_argument("Mail is not configured");
        return *owner;
    }
    std::shared_ptr<Transport> transport_;
    std::shared_ptr<queue::Service> queue_;
    Address sender_;
};
}
