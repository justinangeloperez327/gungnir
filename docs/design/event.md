# Events

> **Gungnir 1.x design specification.** This document describes intended long-term architecture beyond the frozen `1.0.0-rc.1` contract. Examples and requirements may exceed the current implementation. See the [current implementation guide](../event.md) before using an API.


## 1.0 RC alignment

The shipped baseline is `1.0.0-rc.1`. The 1.0 contracts are frozen; this design document may describe additive 1.x evolution or future-major work, but current implementation guides and executable tests remain authoritative.

A Gungnir event is an immutable application fact.

Events communicate that something meaningful has already happened:

```gnr
event UserRegistered {
    User user;
}
```

Events contain data only.

They do not contain handlers, business methods, persistence logic, or delivery logic.

## Basic event

```gnr
event OrderPaid {
    Order order;
    decimal amount;
}
```

The fields form the event payload.

After construction, event payload values should be treated as immutable.

## Event responsibility

An event answers:

```text
What happened?
What data describes what happened?
```

Examples:

```text
UserRegistered
OrderPaid
InvoiceIssued
PasswordChanged
ProjectCreated
```

Event names should normally describe completed facts in the past tense.

## Event fields

Fields use normal Gungnir types:

```gnr
event UserRegistered {
    User user;
    datetime registeredAt;
}
```

Event fields may include:

- scalar values;
- identifiers;
- models/application records;
- value objects;
- lists/collections where valid;
- structured application data.

Fields must be serializable only when the chosen transport requires serialization. In-process events need not pretend to be queue messages.

## Dispatching events

Dispatch an event explicitly:

```gnr
event(new UserRegistered(
    user: user
));
```

or, if the language constructor syntax is standardized differently, the compiler should lower the canonical event-construction syntax to the same event value.

The important contract is:

```text
construct event value
dispatch event value
```

A framework facade may also expose:

```gnr
Event::dispatch(UserRegistered(
    user: user
));
```

One canonical spelling should be selected during general function/expression grammar finalization; the event declaration itself remains unchanged.

## Synchronous dispatch

Core application events are synchronous by default.

Conceptually:

```text
dispatch event
  -> listener A
  -> listener B
  -> listener C
  -> return to caller
```

Exceptions are not silently swallowed.

If work must happen later, it belongs to the queue/job contract rather than pretending ordinary event dispatch is asynchronous.

## Event ordering

When multiple listeners handle the same event, execution order must be deterministic when priorities/order are configured.

Equal-priority listeners should preserve deterministic registration order.

Applications should avoid depending on listener order unless ordering is explicitly part of configuration.

## Events are facts

Prefer:

```text
OrderPaid
UserRegistered
InvoiceIssued
```

over command-like event names such as:

```text
PayOrder
RegisterUser
IssueInvoice
```

Commands request behavior.

Events describe completed behavior.

## Events and transactions

Dispatch timing matters.

An event that claims a database operation succeeded should not be emitted before that operation is actually durable if listeners rely on durability.

Transaction-aware dispatch may later support after-commit semantics, but it must be explicit.

The framework must not silently claim after-commit behavior when the underlying transaction/runtime cannot guarantee it.

## Events and models

Models do not define event handlers.

A service/controller may perform persistence and then dispatch an event:

```gnr
const user = User::create(data);

event(UserRegistered(
    user: user
));
```

## Events and listeners

Events contain data.

Listeners contain reactions.

```gnr
event UserRegistered {
    User user;
}
```

```gnr
listener SendWelcomeNotification {
    public handle(UserRegistered event) {
        // react
    }
}
```

## No arbitrary methods

This should be rejected:

```gnr
event UserRegistered {
    User user;

    sendEmail() {
        // ...
    }
}
```

Events are data declarations, not general-purpose classes.

## No mutable runtime state

Events should not contain mutable counters, caches, database handles, or hidden service references.

## Event grammar

Conceptually:

```text
eventDeclaration
  := 'event' Identifier '{'
       eventField*
     '}'

eventField
  := Type Identifier ';'
```

## Event AST

```text
EventDeclaration
  name
  fields[]
```

Field:

```text
EventField
  type
  name
```

## Semantic validation

The compiler should validate:

- event names are unique;
- field names are unique;
- field types resolve;
- unsupported mutable/general methods are rejected;
- constructors/destructors are not declared manually;
- listener references to events resolve;
- queued/serialized transports only accept serializable payloads when that transport is used.

## Compiler contract

```gnr
event OrderPaid {
    Order order;
    decimal amount;
}
```

should pass through:

```text
source
  -> parser
  -> EventDeclaration AST
  -> field/type resolution
  -> semantic validation
  -> validated event metadata
  -> event lowering
  -> C++23 generation
```

The generated native representation may be a struct/value type, but normal application code should not need native inheritance or event-interface plumbing.

## Design rule

```text
event = immutable data describing something that happened
```

Events contain facts.

Listeners react to those facts.

