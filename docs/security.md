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

Use `gungnir::http::trusted_proxies()` before `gungnir::http::rate_limit()` when an application is intentionally deployed behind a known reverse proxy:

```cpp
router.use(
    gungnir::http::trusted_proxies({
        .proxies = {
            "127.0.0.1"
        }
    })
);

router.use(
    gungnir::http::rate_limit()
);
```

Without trusted proxy configuration, `rate_limit()` uses the socket peer IP recorded by the server and ignores spoofable forwarding headers.

## CSRF

CSRF protection depends on session lifecycle, token generation, token rotation, and request/session binding. It belongs with the session implementation rather than a fake standalone token check.

## Cryptography

Gungnir does not implement custom password hashing, encryption, signing, random-number generation, or TLS cryptography. Production implementations should use established cryptographic libraries and operating-system facilities.


## CSRF protection

Cookie-authenticated browser routes can use session-bound CSRF protection:

```cpp
router.use(
    gungnir::session::middleware(
        session_store
    )
);

router.use(
    gungnir::http::csrf()
);

router.use(
    gungnir::auth::session(
        identity_resolver
    )
);
```

Use this order: **session → CSRF → session authentication**. The ordering lets CSRF observe session-ID rotation caused by login, logout, or stale authentication and rotate the CSRF token at the same boundary.

Safe methods (`GET`, `HEAD`, and `OPTIONS`) do not require a submitted token. They ensure a session token exists. Application code can render it with:

```cpp
auto token =
    gungnir::http::csrf_token(
        request
    );
```

Unsafe methods (`POST`, `PUT`, `PATCH`, and `DELETE`) require the current token in either:

- the `X-CSRF-Token` request header; or
- the `_token` URL-encoded form field.

Query-string tokens are intentionally not accepted because URLs are routinely copied, logged, cached, and included in referrers.

Token comparison is constant-time. A missing or mismatched token returns HTTP **419 Page Expired** before the route handler runs.

When the underlying session ID rotates, the CSRF token also rotates. A token captured before login/logout cannot be reused with the new authenticated session.

CSRF protects cookie-authenticated browser requests from cross-site request forgery. It does not mitigate XSS; scripts executing in the application's own origin can read or submit same-origin state.
