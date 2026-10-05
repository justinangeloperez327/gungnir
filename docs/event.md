# Events

Events are immutable typed facts delivered to listeners in the current process.

```gnr
event UserRegistered {
    int user_id;
    string email;
}
```

Inject `Events` into the producer. Generated applications register listeners during boot, before serving requests.

```gnr
controller RegistrationController {
    inject Events events;
    store(int id, string email) {
        events.dispatch(UserRegistered(id, email));
        return noContent();
    }
}
```

`dispatch` invokes synchronous listeners in descending priority order. Equal priorities retain registration order. If any listener is async, synchronous dispatch fails before invoking any listener; use awaited dispatch instead.

```gnr
controller AsyncRegistrationController {
    inject Events events;
    async store(int id, string email) {
        await events.dispatchAsync(UserRegistered(id, email));
        return noContent();
    }
}
```

Async dispatch awaits listeners sequentially and retains the event through suspension. A listener failure stops dispatch and propagates to the caller. Event names derive from their module and declaration. Events provide no persistence or retry policy; use [queued jobs](queues.md) for background work.
