# Authentication

Gungnir authentication separates credential resolution from request authentication state.

## Identity

`auth::Identity` is the authenticated principal. It contains a stable identifier plus optional roles and attributes. Authentication establishes identity; authorization decides what that identity may do.

## Guards

A guard resolves a credential into an identity.

```cpp
auth::Manager auth;
auth.guard("api", auth::Guard{[](std::string_view token) {
    // Validate the credential using the application's provider.
    return std::optional<auth::Identity>{};
}});
auth.default_guard("api");
```

The core guard deliberately does not assume whether the credential is a session identifier, opaque API token, signed token, or another scheme.

## Request context

`auth::Context` stores authentication state for one request/execution context. HTTP applications can attach it through session authentication middleware and then access it directly from the request:

```cpp
if (request.authenticated()) {
    const auto* user =
        request.user();
}
```

The full context is available through `request.auth()`.

Authentication state is request-owned. It is not stored in global or thread-local state, so asynchronous work may suspend and resume on another worker without losing or crossing identity state.

## Session-backed authentication

Register the session lifecycle before session authentication:

```cpp
router.use(
    gungnir::session::middleware(
        session_store
    )
);

router.use(
    gungnir::auth::session(
        [](std::string_view id)
            -> std::optional<
                gungnir::auth::Identity
            > {
            return users.find_identity(id);
        }
    )
);
```

Only the stable identity ID is stored in the session. The resolver runs for each authenticated request, so current roles and attributes are loaded from the application's identity source rather than cached permanently in session state.

### Login

```cpp
auto identity =
    auth_manager.authenticate(
        credential
    );

if (identity) {
    request.auth().login(
        *identity
    );
}
```

Login marks the request authentication context as changed. After the handler completes, the middleware stores the identity ID and requests session-ID regeneration. The outer session middleware persists the rotated session and removes the previous stored identifier.

### Logout

```cpp
request.auth().logout();
```

Logout removes the authentication identity and rotates the session ID while preserving unrelated application session values. For a complete session reset, also call `request.session().invalidate()`.

If a stored identity can no longer be resolved, or resolves to a different identity ID, Gungnir treats it as stale: the request becomes a guest, the stored authentication key is removed, and the session ID is rotated.

## Passwords

Gungnir does not ship a home-grown password hashing algorithm. Password storage must use a vetted password-hashing backend such as Argon2id or bcrypt through a concrete security adapter. Generic hashes are not password hashes.

## Sessions and tokens

Request-owned session authentication is implemented through `auth::session(...)`. Token issuance/revocation remains separate: it requires explicit storage, expiration, rotation and hashing semantics and is not faked by the authentication core.

The historical `http::Session` name aliases the canonical `session::Session` type for compatibility. New code should use `gungnir::session::Session`.

## Authorization

Roles and abilities belong to authorization policy. The existing `Gate` API remains available, while policy maturity is handled separately in Group 17.
