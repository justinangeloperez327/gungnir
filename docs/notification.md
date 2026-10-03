# Notifications

Notifications deliver application messages to a recipient through one or more channels.

## Defining a notification

```gnr
notification WelcomeNotification {
    via(User recipient) {
        return ["mail", "database"];
    }

    toMail(User recipient) {
        return WelcomeMail(recipient.name, recipient.email);
    }

    toDatabase(User recipient) {
        return {
            "message": "Welcome to Gungnir"
        };
    }
}
```

`via` selects the channels for a specific recipient.

## Mail notifications

`toMail` returns a structured mail declaration. The mail channel resolves the recipient address and sends the resulting message through the configured mail transport.

## Database notifications

`toDatabase` returns structured JSON stored by the database notification channel.

## Custom channels

Applications and extensions can register additional notification channels.

## Queueing

Notification delivery can be moved to a queue when a channel should not delay the current HTTP request.

See [Mail](mail.md) and [Queues and Jobs](queues.md).
