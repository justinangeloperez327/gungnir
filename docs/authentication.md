# Authentication

> **Status: experimental, pre-1.0.** This page describes the current implementation. Native C++ APIs and `.gnr` syntax are identified separately. Proposed contracts are in the [design specification](design/authentication.md).

## Current behavior

Authentication currently uses `Identity`, credential-resolving `Guard`, a guard `Manager`, and a request-owned `Context`. Identity stores an ID, roles and string attributes.

`Request::authenticated`, `guest` and `user` inspect attached identity state. `AuthenticateSession` integrates session authentication. Applications supply the resolver/provider behavior and configure guards explicitly.

## Limits and planned work

There is no complete Laravel-style static `Auth` facade represented by these APIs. Treat `Auth::attempt`, remember-login workflows, built-in password hashing, user-model hydration, token issuance/revocation and guard shorthand in the design specification as planned integration contracts.

## Implementation references

- [include/gungnir/auth/auth.hpp](../include/gungnir/auth/auth.hpp)
- [include/gungnir/auth/context.hpp](../include/gungnir/auth/context.hpp)
- [include/gungnir/auth/manager.hpp](../include/gungnir/auth/manager.hpp)
- [include/gungnir/auth/session.hpp](../include/gungnir/auth/session.hpp)

See the [documentation index](README.md), [getting started](getting-started.md), and [target design](design/authentication.md).
