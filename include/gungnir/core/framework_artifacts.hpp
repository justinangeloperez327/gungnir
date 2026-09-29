#pragma once

#include <gungnir/events/event.hpp>
#include <gungnir/mail/message.hpp>
#include <gungnir/notifications/notification.hpp>

namespace gungnir {

class Policy {
public:
    virtual ~Policy() = default;
};

using Event = events::Event;

class Listener {
public:
    virtual ~Listener() = default;
};

using Notification = notifications::Notification;

class Mail : public mail::Message {
public:
    virtual ~Mail() = default;
};

} // namespace gungnir
