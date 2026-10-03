# Security

> **Development design specification.** This document describes intended long-term architecture beyond the frozen `development` contract. Examples and requirements may exceed the current implementation. See the [current implementation guide](../security.md) before using an API.


## Development alignment

Gungnir is under active development. No 1.0 compatibility contract is frozen; this document may describe intended future architecture, but current implementation guides and executable tests remain authoritative.

Gungnir security is layered across language semantics, HTTP runtime, authentication, authorization, sessions, storage, database access, and deployment.

The framework should provide safe defaults and explicit trust boundaries without claiming that deployment policy can be automated universally.

# Security principles

Core principles are:

~~~text
escape output by default
parameterize database input
fail authorization closed
treat proxy headers as untrusted by default
use secure session identifiers
make raw/native escape hatches explicit
never expose secrets in generated code
bound untrusted input
preserve cancellation and timeouts
~~~

# HTTP hardening

HTTP middleware/runtime should support request/header/body limits, host validation, CORS, security response headers, trusted proxy handling, rate limiting, request IDs/timing, and CSRF for browser session workflows.

These are policy controls and must be configured deliberately.

# Header safety

Response headers must reject CR/LF injection.

Cookie names and values, attachment filenames, redirects, and other response metadata require safe serialization.

Do not concatenate untrusted values directly into raw headers.

# Trusted proxies

Forwarded client information is untrusted unless the direct peer is a configured trusted proxy.

Headers such as X-Forwarded-For, Forwarded, and X-Forwarded-Proto must not automatically override socket-level peer or scheme information.

# Rate limiting

Rate limiting must use a trustworthy client or application key.

A process-local limiter only protects one process.

Distributed rate limiting requires a shared backend with the required atomicity semantics.

# CSRF

CSRF protection applies primarily to authenticated browser/session workflows.

The contract requires a securely generated token, session binding, validation on state-changing requests, appropriate token rotation, and safe comparison.

CSRF is not a substitute for authentication or authorization.

# Authentication

Authentication verifies identity.

Password storage must use a vetted password-hashing implementation through framework password helpers/runtime.

Do not implement password hashing with generic hashes or home-grown cryptography.

# Authorization

Authorization is separate from authentication.

Undefined policy or ability resolution must fail closed.

A hidden button in a view is not authorization enforcement.

# Sessions

Session identifiers require cryptographic randomness.

Login or privilege changes regenerate IDs.

Logout invalidates state.

Production cookies should use HttpOnly, Secure, and an appropriate SameSite policy.

# Constant-time comparison

Constant-time equality may be used for already-derived secrets or tokens where timing resistance matters.

It is not a password hashing function.

# Database

Queries must use bound parameters.

Do not interpolate untrusted runtime values into SQL.

Database credentials belong in secrets/configuration.

# Views

Double-brace interpolation is escaped by default.

Raw output is explicit and must only receive trusted or sanitized HTML.

HTML, attribute, URL, JavaScript, and CSS contexts may require different encoders.

# Storage

Storage paths must remain contained within configured roots or buckets.

Reject traversal, unsafe absolute paths, NULs, and unsafe link/reparse traversal according to adapter guarantees.

# Uploads

Uploaded files are untrusted.

Validate size, content/type as required by the application, filename handling, storage destination, and authorization.

Never trust a browser-supplied filename as a safe filesystem path.

# Mail

Mail headers, addresses, and attachments must be encoded and validated at the transport boundary.

Credentials remain runtime configuration secrets.

# Native extensions

Loading a native plugin executes code with application privileges.

Plugin loading must be explicit.

Do not auto-execute arbitrary libraries found on disk.

# Cryptography

Gungnir should use reviewed platform/library cryptographic implementations.

The framework should not invent cryptographic primitives.

Key generation, encryption, TLS, password hashing, and signing require explicit providers and lifecycle.

# TLS

Production network traffic should use TLS at the appropriate termination layer.

If the built-in runtime terminates TLS, certificate validation and protocol behavior must follow the runtime contract.

Reverse-proxy TLS is valid when the trust boundary is configured correctly.

# Secrets

Secrets must not be embedded in source examples as real values, generated C++, logs, diagnostics, or queue payloads unless an explicit secure design requires it.

Use environment or secret-management configuration.

# Production errors

Unknown failures should render generic responses.

Stack traces, generated source paths, credentials, and native exception details belong in protected logs or development tooling.

# Design rule

~~~text
safe by default
trust explicitly
validate at boundaries
keep security guarantees through lowering and runtime adapters
~~~

