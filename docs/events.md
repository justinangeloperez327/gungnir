# Events

Gungnir events provide synchronous in-process application notifications.

## Event contract

Events implement `events::Event` and expose a stable name. The dispatcher intentionally receives the event by reference; dispatch does not imply ownership or persistence.

## Listeners

`Dispatcher::listen()` registers a listener and returns an identifier that can later be removed with `forget()`. Optional priorities order listeners from higher to lower priority while preserving registration order for equal priorities.

## Dispatch

`dispatch()` runs listeners synchronously on the caller's execution path. Exceptions are not swallowed.

This is deliberate: application events and background jobs are different concepts.

## Async and queued listeners

The event dispatcher does not fake asynchronous execution. Listeners that must run later should integrate with the queue subsystem once a concrete job lifecycle exists.

## Concurrency

A dispatcher should normally be configured during application bootstrap and treated as immutable while requests are concurrently dispatching. Runtime mutation and dispatch from multiple threads require external lifecycle coordination.
