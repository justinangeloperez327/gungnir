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

`auth::Context` stores authentication state for one request/execution context:

```cpp
auth::Context context;
context.login(identity);

if (context.check()) {
    const auto* user = context.user();
}
```

Authentication state must be request-scoped. It must not be stored in global or thread-local state because asynchronous work can overlap and coroutines may migrate between threads.

## Passwords

Gungnir does not ship a home-grown password hashing algorithm. Password storage must use a vetted password-hashing backend such as Argon2id or bcrypt through a concrete security adapter. Generic hashes are not password hashes.

## Sessions and tokens

Session persistence is developed in the Sessions group. Token issuance/revocation requires explicit storage, expiration, rotation and hashing semantics and is not faked by the authentication core.

## Authorization

Roles and abilities belong to authorization policy. The existing `Gate` API remains available, while policy maturity is handled separately in Group 17.
