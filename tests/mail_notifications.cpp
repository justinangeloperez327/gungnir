#include <cassert>
#include <string>
#include <vector>
#include <gungnir/mail/mail.hpp>
#include <gungnir/notifications/notifications.hpp>

class Notice final : public gungnir::notifications::Notification {
public:
    std::string_view name() const noexcept override { return "notice"; }
    std::vector<std::string> channels() const override { return {"test"}; }
};
class TestChannel final : public gungnir::notifications::Channel {
public:
    void send(std::string_view recipient, const gungnir::notifications::Notification& notification) override {
        last_recipient = recipient; last_name = notification.name();
    }
    std::string last_recipient; std::string last_name;
};
int main() {
    using namespace gungnir;
    mail::MemoryTransport transport;
    mail::Mailer mailer{transport};
    mail::Message message;
    message.from({"sender@test.invalid", "Sender"}).to({"user@test.invalid", "User"}).subject("Welcome").text("Hello");
    mailer.send(message);
    assert(transport.messages().size() == 1);
    TestChannel channel;
    notifications::Manager manager;
    manager.channel("test", channel);
    Notice notice;
    manager.send("user-42", notice);
    assert(channel.last_recipient == "user-42");
    assert(channel.last_name == "notice");
    return 0;
}
