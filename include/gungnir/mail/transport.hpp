#pragma once
#include <gungnir/mail/message.hpp>

namespace gungnir::mail {

class Transport {
public:
    virtual ~Transport() = default;
    virtual void send(const Message& message) = 0;
};

class Mailer {
public:
    explicit Mailer(Transport& transport) : transport_(&transport) {}
    void send(const Message& message) { transport_->send(message); }
private:
    Transport* transport_;
};

} // namespace gungnir::mail
