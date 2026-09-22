# Sessions

Gungnir sessions separate request state from persistence.

`session::Session` owns one session's values, flash data and identifier. `session::Store` is the persistence contract. `MemoryStore` is suitable for development and tests; distributed or durable deployments require a production store.

## Lifecycle

A session identifier must be generated with a cryptographically secure random source by the HTTP/session integration layer. The core session object intentionally does not invent predictable identifiers.

Use `regenerate()` after authentication or privilege changes to prevent session fixation. `invalidate()` clears data and rotates or removes the identifier.

## Flash data

`flash()` queues data for the next request. `age_flash()` advances queued flash data into the readable flash set and expires the previous request's flash values.

## Persistence

Stores load, save and erase sessions by opaque identifier. Applications must not place sensitive session payloads directly into client cookies unless a separately reviewed authenticated-encryption format is used.

## Cookies

Production session cookies should normally be `HttpOnly`, `Secure`, and use an appropriate `SameSite` policy. `SameSite=None` requires `Secure`.

## Concurrency

Session state is request-scoped. A production store must define how concurrent requests for the same session avoid lost updates. The in-memory adapter serializes its own map access but is not a distributed concurrency mechanism.
