# Sessions

Gungnir sessions are server-side state identified by an opaque cookie.

## Request lifecycle

Attach session middleware to the router:

```cpp
auto store =
    std::make_shared<
        gungnir::session::MemoryStore
    >();

router.use(
    gungnir::session::middleware(
        store
    )
);
```

The middleware:

- accepts only Gungnir-format session identifiers from the request cookie;
- creates missing sessions with 256 bits of operating-system cryptographic randomness;
- attaches the session to the request for the full coroutine/request lifetime;
- ages flash data once per request;
- persists session mutations after the downstream handler completes;
- replaces the session cookie when a session is created or rotated; and
- erases the previous stored identifier after regeneration or invalidation.

Application code accesses the request-owned session directly:

```cpp
request.session().put(
    "user_id",
    "42"
);

auto user_id =
    request.session().get(
        "user_id"
    );
```

## Regeneration and invalidation

Use `regenerate()` after authentication or privilege changes:

```cpp
request.session().regenerate();
```

This preserves session data while assigning a new cryptographically random identifier. The lifecycle middleware persists the new identifier and removes the previous stored identifier.

Use `invalidate()` for logout or full session reset:

```cpp
request.session().invalidate();
```

Invalidation clears normal and flash data and rotates the identifier.

The lower-level overloads that accept an explicit identifier remain available for compatibility and testing. HTTP middleware will replace an empty or non-Gungnir-format identifier before it is persisted to a session cookie.

## Cookie defaults

The default cookie is:

- name: `gungnir_session`
- path: `/`
- `HttpOnly`: enabled
- `Secure`: enabled
- `SameSite=Lax`
- browser-session lifetime unless a future store/lifetime policy defines otherwise

For local plain-HTTP development, set `session::Options::secure` to `false`. Production deployments should keep secure cookies enabled.

## Storage

`MemoryStore` is thread-safe and suitable for tests, local development, and single-process ephemeral deployments. It is not a durable or distributed production session store.

Persistent database/cache-backed stores, expiry/garbage collection, and distributed session lifecycle policies remain separate production-adapter work.

## Authentication integration

Register `session::middleware(...)` before `auth::session(...)`. Authentication stores only the identity ID in session state; the configured identity resolver reloads the current identity on each request. Login, logout, and stale identity resolution trigger session-ID rotation.
