# Listeners

> **Future design specification.** This document describes intended long-term architecture beyond the published `1.0` contract. Examples and requirements may exceed the current implementation. See the [current implementation guide](../listener.md) before using an API.


## Release alignment

This document describes possible future architecture beyond 1.0. Current implementation guides and executable tests define the published contract.

A Gungnir listener reacts to an application event.

Listeners contain event-handling behavior and may use injected dependencies.

A listener may contain only:

- injected dependencies;
- one public `handle(EventType event)` action.

## Basic listener

```gnr
listener SendWelcomeNotification {
    public handle(UserRegistered event) {
        Notification::send(
            event.user,
            WelcomeNotification(
                user: event.user
            )
        );
    }
}
```

## Listener responsibility

A listener answers:

```text
When this event happens, what reaction should run?
```

Typical listener work includes:

- sending notifications;
- sending mail;
- writing audit records;
- updating projections/read models;
- invoking secondary application services;
- triggering integration work.

Core synchronous listeners should remain reasonably small.

Long-running work belongs to the queue/job system.

## Handle action

Canonical signature:

```gnr
public handle(UserRegistered event) {
    // ...
}
```

The event type identifies which event the listener consumes.

For the initial contract, each listener handles one event type through one `handle` action.

## Dependency injection

```gnr
listener UpdateAnalytics {
    inject AnalyticsService analytics;

    public handle(OrderPaid event) {
        analytics.recordPayment(
            event.order,
            event.amount
        );
    }
}
```

No explicit constructor is required.

## Synchronous listeners

Normal listeners execute synchronously:

```gnr
listener WriteAuditLog {
    inject AuditService audit;

    public handle(UserRegistered event) {
        audit.record(
            'user.registered',
            event.user.id
        );
    }
}
```

If the listener fails, the exception propagates according to the event-dispatch/error contract.

## Async listeners

An async listener may be supported for asynchronous in-process work:

```gnr
listener SyncProfile {
    inject ProfileService profiles;

    public async handle(UserRegistered event) {
        await profiles.sync(event.user);
    }
}
```

However, `async` does not mean queued/background execution.

It only means the current dispatch path may suspend while the listener completes.

Queued execution belongs to the queue/job system.

## Queued listeners

When queue support is finalized, a listener may explicitly opt into queued execution through structured metadata or a dedicated queue declaration.

Gungnir should not infer background execution merely because a listener is async.

The contract must keep these distinct:

```text
sync listener   -> caller waits
async listener  -> caller awaits completion
queued listener -> separate job lifecycle
```

## Listener registration

Where naming/project discovery is available, the compiler/application bootstrap may register listeners from their typed `handle` parameter.

Conceptually:

```text
SendWelcomeNotification.handle(UserRegistered)
  -> listens to UserRegistered
```

No string event name should be necessary for normal typed Gungnir code.

Explicit registration may still exist for advanced application configuration.

## Multiple listeners

Multiple listeners may consume the same event:

```text
UserRegistered
  -> SendWelcomeNotification
  -> CreateAuditEntry
  -> UpdateAnalytics
```

Order should be deterministic where configured, but application correctness should avoid accidental dependence on unspecified ordering.

## Listener priority

If priority is supported, it should be structured metadata rather than hidden naming behavior.

The exact syntax belongs to event/bootstrap configuration.

## Event payload

Listeners receive the event value directly:

```gnr
public handle(OrderPaid event) {
    const order = event.order;
    const amount = event.amount;
}
```

Listeners should not need to query a global event registry to retrieve the payload.

## Listener and notification

A listener may trigger a notification:

```gnr
listener NotifyUserRegistered {
    public handle(UserRegistered event) {
        Notification::send(
            event.user,
            WelcomeNotification(
                user: event.user
            )
        );
    }
}
```

## Listener and mail

A listener may send a mailable directly when notification routing is unnecessary:

```gnr
listener SendInvoiceMail {
    public handle(InvoiceIssued event) {
        Mail::to(event.invoice.customer.email)
            .send(
                InvoiceMail(
                    invoice: event.invoice
                )
            );
    }
}
```

## No arbitrary fields or methods

Listeners should not become stateful general-purpose classes.

Only:

```text
inject declarations
public handle(...)
```

belong in the initial listener contract.

Reusable logic should move to an injected service.

## Listener grammar

Conceptually:

```text
listenerDeclaration
  := 'listener' Identifier '{'
       listenerMember*
     '}'

listenerMember
  := injectDeclaration
   | listenerHandle

listenerHandle
  := 'public' 'async'? 'handle'
     '(' EventType Identifier ')'
     block
```

## Listener AST

```text
ListenerDeclaration
  name
  injections[]
  handle
```

Handle:

```text
ListenerHandle
  eventType
  eventParameter
  async
  body
```

## Semantic validation

The compiler should validate:

- listener names are unique;
- exactly one `handle` action exists;
- the action is public;
- its event parameter resolves to a declared event;
- injected types resolve;
- `await` occurs only in async listeners;
- arbitrary fields/methods/constructors are rejected;
- listener registration can be derived or resolved deterministically.

## Compiler contract

```gnr
listener SendWelcomeNotification {
    public handle(UserRegistered event) {
        Notification::send(
            event.user,
            WelcomeNotification(user: event.user)
        );
    }
}
```

should pass through:

```text
source
  -> parser
  -> ListenerDeclaration AST
  -> event/type resolution
  -> dependency resolution
  -> semantic validation
  -> validated listener AST
  -> event-registration lowering
  -> C++23 generation
```

The transpiler must not discover listener/event relationships by string scanning.

## Design rule

```text
listener = injected dependencies + one typed event handler
```

Events describe facts.

Listeners react to those facts.

