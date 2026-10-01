#pragma once
#include <gungnir/notifications/channel.hpp>
#include <gungnir/mail/transport.hpp>
#include <gungnir/database/runtime.hpp>
#include <gungnir/security/random.hpp>

namespace gungnir::notifications {
class MailChannel final : public Channel {
public:
    MailChannel(std::shared_ptr<mail::Transport> transport, mail::Address sender)
        : transport_(std::move(transport)), sender_(std::move(sender)) {
        if (!transport_) throw std::invalid_argument("Mail notification channel requires a transport");
    }
    void send(std::string_view recipient, const Notification& notification) override {
        auto message = notification.mail_message();
        if (!message) throw std::logic_error("Notification does not define a mail message");
        if (message->sender().email.empty()) message->from(sender_);
        message->to(mail::Address{std::string{recipient}, {}});
        mail::Mailer{*transport_}.send(*message);
    }
private:
    std::shared_ptr<mail::Transport> transport_;
    mail::Address sender_;
};
// Applications create a notifications table with id, type, recipient, data columns.
class DatabaseChannel final : public Channel {
public:
    void send(std::string_view recipient, const Notification& notification) override {
        auto payload = notification.database_payload();
        if (!payload) throw std::logic_error("Notification does not define a database payload");
        auto connection = database::runtime::write_connection();
        if (connection->backend() == database::Backend::mongodb) throw std::logic_error("Database notification channel requires a SQL connection");
        const auto sql = connection->backend() == database::Backend::postgresql
            ? "INSERT INTO notifications (id, type, recipient, data) VALUES ($1, $2, $3, $4)"
            : "INSERT INTO notifications (id, type, recipient, data) VALUES (?, ?, ?, ?)";
        connection->execute(sql,{security::random_token(),std::string{notification.name()},std::string{recipient},payload->dump()});
    }
};
}
