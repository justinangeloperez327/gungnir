# Canonical authentication contract audit

Baseline: `261a262c4bc964f0b8c916287d6583fdb6a2fd4c` on `main`, containing merged
PRs #175, #176 and #177. Authentication is integrated with the canonical session
handles and identity snapshots.

## Proven contract gap

The product's login example calls `auth.attempt(request, credentials, remember)`.
The canonical validator had no `auth` service surface. Native `SessionGuard`,
password hashing, identity restoration, token digest storage and atomic recall
already existed; those implementations are preserved.

The documented remember argument used string request input where the native
guard requires a boolean. The guide now compares the submitted form value
explicitly and describes the typed boolean choice.

## Implementation and ownership

Canonical validation exposes `auth.attempt`, `auth.logout` and `Password::hash`,
`verify`, `needsRehash`, using the existing request auth-state checks. Credential
object literals
receive shape/type diagnostics; dynamic objects are checked at runtime without
reflecting secret input into errors. Authentication APIs cannot be serialized or
stored as declaration fields. Calls use validated bindings and structural IR.

`ServiceOptions.authentication` registers the shared native guard. The guard's
additional request-only overloads stage cookie effects in the existing auth
context, preserving the documented boolean control flow. Guard middleware
recalls a remember token before the action and transfers staged cookies to the
final response after awaited work. Its shared owner keeps providers and token
stores alive. Existing explicit-response overloads remain available.

The middleware owns the shared auth context for the suspended operation and
clears its active guard marker and untransferred cookies on every exit. Calls
reject an absent or different guard scope instead of silently losing cookie
effects. Service and middleware registration must use the same guard instance.

The native session and auth middleware remain responsible for persistence and
identifier rotation. Guard configuration requires them to run before guard
middleware. The password backend requires `GUNGNIR_WITH_PASSWORD=ON`; the
installed config finds OpenSSL Crypto independently of TLS. Feature-disabled
generated consumers receive an explicit runtime configuration error.

## Integrated merge repair

The #177 merge resolution removed existing request/response/JSON validator
bindings, identity member and context-field restrictions, and session receiver
lowering. Its Documentation, CI and Compiler Conformance jobs failed on the
generated HTTP contract and public HTTP fragments at `d85cdd7d14af8015a40fd602666aaa661df2e4a7`.
This batch restores the combined contracts, preserves ORM receiver ownership,
and composes session and authentication serialization restrictions.

The snapshot script also compared `first_program_hash` before initialization.
The two generated outputs had identical SHA256 hashes. Removing that premature
duplicate comparison retains the initialized comparison and the full
GCC/Clang/MSVC output matrix.

## Acceptance evidence

`tests/fixtures/structured/authentication.gnr` supplies login/logout controllers,
password helpers and awaited middleware. `tests/structured_authentication.cpp`
routes requests through them and checks validation errors, unknown accounts,
wrong passwords, successful login, session fixation protection, final-response
cookie effects, salted hashes, digest-only token storage, token rotation/replay
rejection, logout revocation, disabled identity resolution and provider ownership.
Canonical session writes persist after successful credential login. Guarded
identity reads and owned JSON snapshots agree after login and remember recall;
logout clears the identity and session data without exposing password material.
Invalid calls must agree in emission and validation-only diagnostics and spans.
The guide gate checks login/logout and password examples. Compiler CI compares
authentication C++, validated AST and IR across GCC, Clang and MSVC; primary CI
runs the password-enabled generated and installed consumers.

Local GCC 13 Debug verification with SQLite and password hashing enabled, TLS
disabled: **all 92 CTest checks pass** on the combined ORM/HTTP/context/authentication
branch, including installed CLI generators, migrations, live HTTP and dev
restart. Damaged local build artifacts were rebuilt before testing; the snapshot
check passes after the merge repair described above. Native public, generated
ORM, HTTP, context and authentication consumers are verified against the installed
package, which discovers OpenSSL Crypto independently.
**Three public credential/password examples pass
strict validation**. Documentation/development checks and workflow YAML parsing
pass. Scope regressions reject absent/mismatched middleware and calls after
request completion, and verify cleanup when validation throws. Remote compiler
and feature-disabled results are recorded in the PR after verification.

## Remaining product requirements

Merged PR #177 supplies owned identity snapshots and typed policy actor
acceptance. This batch does not establish the full
application acceptance gate, a persistent remember-store adapter, active password
backend execution on every platform, complete route assembly, uploads, other
application services or live database/Redis matrices. The product matrix in PR
#175 remains authoritative for those requirements. No release/version change or
1.0 completeness claim is made here.
