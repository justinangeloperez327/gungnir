# Security

Gungnir security is layered. The framework supplies safe primitives and middleware contracts; cryptographic algorithms and deployment trust boundaries must remain explicit.

## HTTP hardening

Existing HTTP middleware provides:

- security response headers
- request body limits
- host validation
- CORS policy
- request identifiers
- rate limiting

These controls are application policy and should be configured deliberately.

## Constant-time comparison

`security::constant_time_equal()` is available for comparing already-derived secret values where timing-sensitive equality is required.

It is not a password hashing function and does not replace a vetted cryptographic library.

## Header and cookie validation

`valid_header_value()` rejects carriage-return/newline injection. `valid_cookie_name()` validates cookie token names before serialization.

Framework-generated response metadata must not allow CRLF injection.

## Proxies and rate limiting

Client-supplied `X-Forwarded-For` is not trustworthy by itself. Rate limiting and client-IP resolution must only honor forwarding headers after a trusted-proxy boundary has been configured.

## CSRF

CSRF protection depends on session lifecycle, token generation, token rotation, and request/session binding. It belongs with the session implementation rather than a fake standalone token check.

## Cryptography

Gungnir does not implement custom password hashing, encryption, signing, random-number generation, or TLS cryptography. Production implementations should use established cryptographic libraries and operating-system facilities.
