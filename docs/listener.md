# Listeners

A listener has a public `handle` method that accepts exactly one non-optional event. Import the event's module when it is declared in another file.

```gnr
listener RecordRegistration {
    priority = 20;
    inject Cache cache;
    handle(UserRegistered event) {
        cache.put('registered.email', event.email);
    }
}
```

Services are injected during application boot. `priority` is an integer literal in the native `int` range; it defaults to zero. Higher values run first, and negative values run later. Applications discover and register generated listeners automatically.

Listeners may dispatch other events. Use async dispatch for async listeners, and await all async calls.

```gnr
event RegistrationRecorded { int user_id; }
listener PublishRegistration {
    priority = -10;
    inject Events events;
    async handle(UserRegistered event) {
        await events.dispatchAsync(RegistrationRecorded(event.user_id));
    }
}
```

A handler returns `void`. Exceptions propagate to the event producer. Shutdown releases listener registrations, including listeners that inject `Events`. For persistence and retries, dispatch a [job](queues.md).
