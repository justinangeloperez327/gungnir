# Notifications

Notifications deliver application messages to a recipient through one or more channels.

The typed delivery APIs below require the current source SDK. See the
[v1.0.0 release scope](release-v1.md) for the published package contract.

## Defining a notification

```gnr
notification WelcomeNotification {
    via(User recipient) {
        return ["mail", "database"];
    }

    toMail(User recipient) {
        return WelcomeMail(recipient.name);
    }

    toDatabase(User recipient) {
        return {
            "message": "Welcome to Gungnir"
        };
    }
}
```

`via` selects the channels for a specific recipient.
The recipient is a non-optional model, and all payload methods use the same
model type. Inject `Notifications` to send or queue a declaration:

```gnr
controller NotificationController {
    inject Notifications notifications;
    show(User user) {
        notifications.send(user, WelcomeNotification());
        return noContent();
    }
}
```

## Mail notifications

`toMail` returns a structured mail declaration. The mail channel resolves the recipient address and sends the resulting message through the configured mail transport.
By default it reads the model's initialized string `email` attribute. Model
serialization visibility does not suppress internal address resolution. The
default sender comes from `ServiceOptions.sender`. Missing or invalid addresses
fail before delivery.

## Database notifications

`toDatabase` returns structured JSON stored by the database notification channel.
The recipient address is the model's primary key, including custom string keys.
Enable `ServiceOptions.database_notifications` and create a SQL `notifications`
table with `id`, `type`, `recipient`, and `data` columns. The channel uses bound
values on the application's write connection. MongoDB requires a separately
configured custom channel; the built-in database channel is SQL-only.
Models inside the payload retain `hidden`/`visible` serialization rules.

## Custom channels

Applications and extensions can register additional notification channels.
Register native `Channel` implementations on the configured `Manager`. Custom
channels receive the declaration name and composed payloads through the native
`Notification` interface. For custom channels, `toDatabase` supplies a shared
JSON payload and the default recipient route is the model's primary key.

Override an address for a particular model and channel in `bootstrap::boot`:

```cpp
auto notifications = app.container().resolve<gungnir::notifications::Service>();
notifications->route<User>("mail", [](const User& user) {
    return user.attribute_value("contact_email")
        .transform([](const auto& value) { return gungnir::model::value_cast<gungnir::String>(value); })
        .value_or("");
});
```

The facade resolves all routes, checks configured channels and composes selected
payloads before invoking any channel. Duplicate channel names are rejected.
Channel failures propagate and may follow an earlier successful channel;
delivery across channels is not a transaction.

## Queueing

Notification delivery can be moved to a queue when a channel should not delay the current HTTP request.

```gnr
controller QueuedNotificationController {
    inject Notifications notifications;
    show(User user) {
        const id = notifications.queue(user, WelcomeNotification(), 3);
        return json({job: id});
    }
}
```

`queue(recipient, notification, attempts = 1, afterCommit = true)` returns a job
ID and follows the same commit, rollback, retry and worker rules as mail. It
stores the selected channels, resolved addresses and composed payloads rather
than a model or service pointer. Changes to the recipient after publication do
not change that snapshot. Configure the required channels in every worker.

Each retry repeats the notification's channel sequence. Delivery is at least
once, so a channel that succeeded before a later failure can receive a duplicate.
Use application-level deduplication when a channel requires it.

See [Mail](mail.md) and [Queues and Jobs](queues.md).
