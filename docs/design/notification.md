# Notifications

> **Design specification.** This document preserves the intended contract. Examples and requirements below may exceed the current implementation. See the [current implementation guide](../notification.md) before using an API.

A Gungnir notification describes a user-facing message that may be delivered through one or more channels.

Notifications define:

- which channels should be used;
- what message should be produced for each channel;
- which application data is needed to build that message.

Notifications do not define transport internals, queue workers, SMTP connections, or unrelated business logic.

## Basic notification

```gnr
notification WelcomeNotification {
    public via(User user) {
        return [
            'mail'
        ];
    }

    public mail(User user) {
        return WelcomeMail(
            user: user
        );
    }
}
```

Send the notification:

```gnr
Notification::send(
    user,
    WelcomeNotification()
);
```

## Notification responsibility

A notification answers:

```text
Who should receive this notification?
Which channel or channels should deliver it?
What channel-specific message should be created?
```

Examples of channels may include:

```text
mail
database
sms
push
broadcast
```

Only channels with concrete framework/runtime support should be exposed as available.

## via

The `via()` action defines delivery channels:

```gnr
public via(User user) {
    return [
        'mail',
        'database'
    ];
}
```

The returned channel names must resolve to configured notification channels.

## Mail channel

For mail delivery:

```gnr
public mail(User user) {
    return WelcomeMail(
        user: user
    );
}
```

The notification decides that mail should be sent.

The `mail` declaration defines the actual email composition.

## Database channel

A database notification may return structured data:

```gnr
public database(User user) {
    return {
        'type': 'order_shipped',
        'order_id': order.id,
        'message': 'Your order has shipped.'
    };
}
```

The database notification storage contract belongs to the notification runtime.

## SMS channel

If an SMS adapter is configured:

```gnr
public sms(User user) {
    return sms(
        'Your verification code is ' + code
    );
}
```

Provider credentials and vendor APIs belong to channel adapters, not notification declarations.

## Push channel

If push delivery is supported:

```gnr
public push(User user) {
    return push({
        'title': 'Order shipped',
        'body': 'Your order is on the way.'
    });
}
```

## Notification recipients

A recipient should satisfy the framework's notifiable contract.

For ordinary applications this will often be a model such as:

```gnr
User
```

Routing information may come from conventional fields or configured notification routing.

For mail:

```text
email
```

For SMS:

```text
phone
```

For push:

```text
device token / endpoint
```

The notification system should not hard-code one user schema into the language.

## Explicit routing

Where a recipient does not expose a conventional route, the framework may support explicit routing:

```gnr
Notification::route(
    'mail',
    'person@example.com'
).send(
    WelcomeNotification()
);
```

The exact routing API should remain structured and channel-aware.

## Multiple recipients

```gnr
Notification::send(
    users,
    SystemMaintenanceNotification()
);
```

A collection of notifiable recipients may be accepted where channel semantics permit it.

## Notification data

A notification may receive immutable construction data:

```gnr
notification OrderShippedNotification {
    Order order;

    public via(User user) {
        return ['mail'];
    }

    public mail(User user) {
        return OrderShippedMail(
            order: order,
            user: user
        );
    }
}
```

Notification fields describe message context.

They should not become mutable service state.

## Dependency injection

Where message construction requires a service, injection may be supported:

```gnr
notification InvoiceReadyNotification {
    inject UrlGenerator urls;

    Invoice invoice;

    public via(User user) {
        return ['mail'];
    }

    public mail(User user) {
        return InvoiceReadyMail(
            invoice: invoice,
            url: urls.route(
                'invoices.show',
                {'invoice': invoice.id}
            )
        );
    }
}
```

Dependencies should support message construction, not hide unrelated workflows inside the notification.

## Immediate delivery

Normal delivery may occur synchronously:

```gnr
Notification::send(
    user,
    WelcomeNotification()
);
```

## Queued delivery

Queued notifications should use an explicit queue contract once the queue subsystem is finalized.

Gungnir should not pretend a notification is queued merely because a channel performs asynchronous I/O.

The distinction is:

```text
send now    -> current application lifecycle
queue       -> separate job lifecycle
```

## Notification failures

Channel delivery failures should surface through structured channel errors or queue failure handling.

A failed SMTP request, SMS provider response, or push provider response must not be silently reported as success.

## Notifications and events

Listeners commonly trigger notifications:

```gnr
listener NotifyUserRegistered {
    public handle(UserRegistered event) {
        Notification::send(
            event.user,
            WelcomeNotification()
        );
    }
}
```

The event describes what happened.

The listener reacts.

The notification describes the user-facing message.

## Notifications and mail

Notification:

```gnr
notification WelcomeNotification {
    public via(User user) {
        return ['mail'];
    }

    public mail(User user) {
        return WelcomeMail(
            user: user
        );
    }
}
```

Mail:

```gnr
mail WelcomeMail {
    User user;

    public subject() {
        return 'Welcome';
    }

    public content() {
        return view('mail/welcome', {
            'user': user
        });
    }
}
```

These responsibilities should remain separate.

## What does not belong in a notification

Notifications should not:

- perform large business workflows;
- mutate unrelated records;
- implement transport protocols;
- store SMTP/API credentials;
- contain controller logic;
- perform authorization decisions;
- act as event listeners themselves.

## Notification grammar

Conceptually:

```text
notificationDeclaration
  := 'notification' Identifier '{'
       notificationMember*
     '}'

notificationMember
  := fieldDeclaration
   | injectDeclaration
   | notificationAction

notificationAction
  := 'public' Identifier '(' parameterList? ')' block
```

Recognized channel methods should correspond to configured/supported notification channels.

## Notification AST

```text
NotificationDeclaration
  name
  fields[]
  injections[]
  via
  channelActions[]
```

## Semantic validation

The compiler should validate:

- notification names are unique;
- fields and injection types resolve;
- exactly one usable `via()` action exists;
- channel names returned by `via()` are valid where configuration is known;
- each selected channel has a compatible channel method;
- mail-channel methods return a mail-compatible value;
- database-channel methods return serializable structured data;
- arbitrary unrelated methods are rejected;
- recipient parameters are compatible with notifiable values.

## Compiler contract

```gnr
notification WelcomeNotification {
    public via(User user) {
        return ['mail'];
    }

    public mail(User user) {
        return WelcomeMail(user: user);
    }
}
```

should pass through:

```text
source
  -> parser
  -> NotificationDeclaration AST
  -> recipient/channel resolution
  -> channel result validation
  -> validated notification AST
  -> notification lowering
  -> runtime channel dispatch
  -> C++23 generation
```

The transpiler must not identify notification channels by raw source scanning.

## Design rule

```text
notification = recipient-aware message routing + channel-specific message creation
```

Notifications decide how a user should be notified.

Channel adapters deliver the message.

Mailables define email composition.

