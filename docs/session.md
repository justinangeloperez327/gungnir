# Sessions

Gungnir sessions provide request-scoped server-side state identified by an opaque client cookie.

Sessions integrate with authentication, CSRF protection, flash data, redirects, and browser workflows.

# Architecture

A session consists of:

~~~text
opaque session ID
server-side values
flash data
persistence store
cookie metadata
request-owned session context
~~~

The client cookie identifies the session. Application session state is not stored directly in an unsigned/plain client cookie.

# Request lifecycle

Session middleware should:

1. read and validate the session cookie;
2. load server-side state if it exists;
3. create a secure ID when needed;
4. attach the session to the request context;
5. age flash data once;
6. execute downstream middleware/controller code;
7. persist mutations;
8. emit, rotate, or expire the cookie as required.

The session remains request-owned for the full coroutine lifetime.

# Session IDs

Session identifiers must be generated from an operating-system cryptographically secure random source with sufficient entropy.

Predictable IDs are not acceptable.

# Regeneration

After login or privilege changes, regenerate the session identifier.

Regeneration preserves intended session data while replacing the ID and invalidating the old identifier.

This mitigates session fixation.

# Invalidation

Logout or full reset should invalidate the session.

Invalidation clears appropriate data, rotates or removes the old identifier, and prevents stale state reuse.

# Flash data

Flash values live for a limited request lifecycle:

~~~text
request N
  set flash

request N+1
  read flash

later
  expired
~~~

This supports validation errors, status messages, and old form input.

# Stores

Session persistence uses a Store abstraction.

Possible stores include MemoryStore, RedisStore, and future database/custom stores.

MemoryStore is suitable only for development, tests, and single-instance use.

# Distributed sessions

A distributed store must define TTL/expiry, concurrent update behavior, stale ID deletion, atomicity expectations, and failover behavior.

Do not claim distributed-safe semantics merely because data is stored in Redis.

# Cookies

Production session cookies should normally use:

~~~text
HttpOnly
Secure
appropriate SameSite
restricted Path
appropriate Domain
~~~

SameSite=None requires Secure.

# Authentication integration

Session authentication stores only the identity material required by the configured guard/provider.

Login should regenerate the session ID.

Logout should invalidate or rotate authentication/session state according to the authentication contract.

# CSRF integration

Browser session authentication and CSRF belong to the same request/session security boundary.

CSRF tokens should bind to session lifecycle and rotate when required.

The view helper only renders the token; validation occurs in middleware/security runtime.

# Async safety

The active session must follow the logical request across await.

Do not rely on raw thread-local storage without coroutine-aware context propagation.

# Concurrency

Concurrent requests for the same session can race.

A production store must define how lost updates are avoided or documented.

A process-local mutex only protects one process.

# Security

Do not store plaintext credentials in session state.

Session IDs should not be logged unnecessarily.

# Design rule

~~~text
cookie identifies
store persists
request owns active session
login regenerates
logout invalidates
async keeps context attached to the request
~~~
