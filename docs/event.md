# Events

Events represent facts that have occurred in the application and allow independent parts of the system to react without tightly coupling the producer to every consumer.

## Defining an event

```gnr
event UserRegistered {
    int user_id;
    string email;
}
```

Events are immutable typed data declarations.

## Dispatching events

Application services dispatch an event through the event dispatcher. Registered listeners for that event type are invoked according to their priority and sync/async contract.

## Event names

Generated events have stable qualified names derived from their module and declaration.

## Async dispatch

Asynchronous listeners can be awaited through asynchronous dispatch. Synchronous dispatch is reserved for synchronous listeners.

For background work that must survive the current process or be retried, use [Queues and Jobs](queues.md) rather than treating in-process events as a durable queue.
