# Authentication

> **Status: experimental, pre-1.0.** This page describes the current implementation. Native C++ APIs and `.gnr` syntax are identified separately. Proposed contracts are in the [design specification](design/authentication.md).

## Current behavior

Authentication currently uses `Identity`, credential-resolving `Guard`, a guard `Manager`, and a request-owned `Context`. Identity stores an ID, roles and string attributes.

`Request::authenticated`, `guest` and `user` inspect attached identity state. `AuthenticateSession` integrates session authentication. Applications supply the resolver/provider behavior and configure guards explicitly.

Phase 14 promotes authentication/session lifecycle and CSRF regressions into the primary pull-request security gate. Session login/logout and stale identity recovery must continue rotating session identifiers without preserving obsolete authenticated state.

Enable `GUNGNIR_WITH_PASSWORD=ON` (OpenSSL 3) and include `<gungnir/auth/login.hpp>` for `SessionGuard`. Applications supply credential and identity resolvers. `attempt(request, response, login, password, remember)` verifies an scrypt hash and rotates the session on success. `recall` consumes and rotates a remember token; `logout` invalidates the session and revokes its token. `Password::hash`, `verify`, and `needs_rehash` handle the versioned hash format. Remember stores retain token digests; the included `MemoryRememberStore` is process-local. Run these operations inside session middleware, with `AuthenticateSession` to restore identities on subsequent requests.

## Limits and planned work

There is no complete Laravel-style static `Auth` facade represented by these APIs. Static `Auth::attempt`, automatic user-model hydration and distributed remember-token persistence remain application integrations.

## Implementation references

- [include/gungnir/auth/auth.hpp](../include/gungnir/auth/auth.hpp)
- [include/gungnir/auth/context.hpp](../include/gungnir/auth/context.hpp)
- [include/gungnir/auth/manager.hpp](../include/gungnir/auth/manager.hpp)
- [include/gungnir/auth/session.hpp](../include/gungnir/auth/session.hpp)

See the [documentation index](README.md), [getting started](getting-started.md), and [target design](design/authentication.md).
