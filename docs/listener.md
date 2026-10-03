# Listeners

Listeners react to application events.

## Defining a listener

```gnr
listener SendWelcomeEmail {
    handle(UserRegistered event) {
        // React to the event.
    }
}
```

A listener's `handle` method receives exactly the event type it handles.

## Async listeners

```gnr
listener SyncCustomerProfile {
    async handle(UserRegistered event) {
        await profiles.sync(event.user_id);
    }
}
```

Async listeners are executed through asynchronous event dispatch.

## Dependency injection

Listeners can use application services through the container, allowing event handling logic to remain testable and separated from the event producer.

## Priority

Listener registration can assign priority when deterministic ordering between listeners is required.

Events are in-process application messaging. Use [Queues and Jobs](queues.md) when work requires persistence, retries, or execution outside the request lifecycle.
