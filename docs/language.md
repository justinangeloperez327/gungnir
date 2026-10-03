# The Gungnir Language

Gungnir applications are written with an expressive framework-oriented language stored in `.gnr` files. The language is designed around web application concepts rather than C++ boilerplate while retaining static types, deterministic compilation, async functions, modules, and native interoperability.

## Framework declarations

Gungnir provides first-class declarations for the major application building blocks:

```gnr
model User {
}

controller UserController {
}

middleware Authenticate {
}

migration CreateUsers {
}

policy UserPolicy {
}

event UserRegistered {
}

listener SendWelcomeEmail {
}

notification WelcomeNotification {
}

mail WelcomeMail {
}

job ProcessImport {
}
```

Each declaration has a framework contract. For example, controllers expose actions, middleware exposes `handle`, migrations expose `up` and `down`, and listeners handle a typed event.

## Static types

Gungnir supports framework and application types including strings, booleans, integers, unsigned integers, floating-point values, decimals, JSON values, optional values, lists, maps, collections, requests, responses, models, and user-defined declarations.

See [Language Types](language-types.md).

## Functions and methods

Functions and framework methods use typed parameters and results:

```gnr
function int add(int left, int right) {
    return left + right;
}
```

Framework declarations may provide an implicit logical result where the declaration contract makes the result unambiguous.

## Async and await

Asynchronous work uses `async` and `await`:

```gnr
controller ReportController {
    async index() {
        const report = await reports.generate();
        return json(report);
    }
}
```

Gungnir lowers asynchronous application code onto its coroutine-based C++ runtime.

## Modules

Applications can divide declarations into modules and import them explicitly. Modules provide namespacing and deterministic dependency resolution.

See [Modules](modules.md).

## Expressions and statements

The language provides ordinary application constructs including bindings, calls, member access, arithmetic and comparison operators, conditionals, loops, lists, objects, lambdas, optional handling, returns, throws, and async calls.

See [Expressions](expressions.md) and [Statements](statements.md).

## Framework integration

The language is integrated with Gungnir's routing, ORM, validation, dependency injection, authorization, queues, notifications, mail, views, and other framework services. Framework misuse is diagnosed before native C++ compilation whenever it can be determined statically.

For the compiler architecture itself, see the contributor reference in the [documentation index](README.md).
