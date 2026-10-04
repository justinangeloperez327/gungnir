# Canonical request-context contract audit

Baseline: `8033cf6837f9cdc55ab5dfb5f805076f015dde67` on `main`.
This is an independent batch after PRs #175 and #176.

## Proven gaps and implementation

Documented `request.session()` and `request.user()` were rejected by the
canonical validator. Native sessions and auth context existed. The compiler now
exposes a typed `Session` handle, owned `AuthIdentity?` snapshots, session
read/write/flash/rotation APIs, identity fields and role checks, and response
cookie construction/expiration. It uses the authoritative syntax, validation,
structural IR and C++23 emission path.

The session handle owns the existing native session through a shared pointer;
generated calls check an empty native handle before accessing it. It preserves
the live object across functions and awaited work. Snapshots copy identity data
and do not retain a pointer into the request's auth context. Request-context
types cannot be declaration fields. Session handles cannot be serialized;
application data must be selected explicitly.

Cookie options are validated in literal source where possible and at runtime for
dynamic JSON. Native cookie serialization enforces names, values, domains,
secure prefixes, SameSite and attribute safety before attachment. Expiration
preserves path/domain matching and forces a zero lifetime. Native middleware and
guard implementations continue handling persistence and auth lifecycle.

Password-enabled installed packages previously exported `OpenSSL::Crypto`
without finding that dependency when TLS was disabled. The package config now
finds Crypto for that independent feature. Main CI enables the password feature;
compiler conformance also covers the default package without it.

## Evidence

`tests/fixtures/structured/request_context.gnr` and
`tests/structured_request_context.cpp` exercise generated session/controller and
middleware calls through native routing: persistence across requests, flash
aging, forgetting/clearing, old identifier rejection, rotation/invalidation,
anonymous handling, restored identity snapshots, native model hydration, typed
policy actor resolution, forbidden responses, hidden model fields, and cookie
security. Returned session handles and copied identities remain safe after
request destruction. Empty native handles throw rather than dereferencing null.

The password-enabled variant uses the existing credential resolver and
`SessionGuard` to verify success/failure and logout before inspecting generated
identity/authorization paths. The default variant establishes auth context using
native session middleware. These tests do not expose a canonical credential
attempt API; that remains separate work.

Emission and validation-only failures must agree in codes, messages and source
spans. The focused guide checker covers all session fragments and identity/cookie
fragments; full login and route examples are separate application contracts.
CI compares context C++, validated AST and IR across GCC, Clang and MSVC, and
executes the same generated fixture against an installed package.

Local GCC 13 Debug verification with SQLite and password hashing enabled, TLS
disabled: all **84 CTest checks pass**, including installed CLI generation,
migrations, live HTTP and dev restart coverage. Five transient local executables
were rebuilt or had their executable modes restored before their successful
reruns. The generated context fixture and native public consumer also build and
run against an installed package that independently discovers OpenSSL Crypto.
The default password-disabled context variant passes as well. **Eight public
session, identity and cookie examples pass strict validation**; documentation,
development consistency and workflow YAML checks pass.

## Remaining requirements

Credential attempts, remember-token operations and typed service access still
need canonical APIs. Uploads, streaming/WebSocket construction, complete routing
acceptance, other service integrations and live adapter matrices remain in the
full product audit from PR #175. These changes do not meet the 1.0 completeness
gate and do not alter package/language versions or release-channel identity.
