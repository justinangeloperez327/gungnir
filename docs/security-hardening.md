# Security Hardening

> **Status: Development.** This guide describes the current implementation and documented limits. The 1.0 compatibility contract is not frozen yet.

## Phase 14 guarantees

### HTTP request framing

The HTTP/1 backend rejects ambiguous request framing before dispatch:

- duplicate `Content-Length` headers are rejected;
- any `Transfer-Encoding` header is rejected because the core HTTP/1 backend does not implement transfer-coded request bodies;
- HTTP/1.1 requires exactly one non-empty `Host` header;
- whitespace before a header colon is rejected;
- header names and values are validated before entering `Request`.

These rules are intentionally strict to prevent front-end/back-end parser disagreement and request-smuggling ambiguity.

### Header boundaries

`Request::set_header` and `Response::header` reject invalid field names and control characters. Response serialization still performs a second validation at the wire boundary.

Incoming `X-Request-ID` values are only reflected when they are bounded and use a conservative identifier character set. Invalid or oversized values are replaced with a cryptographically random Gungnir request ID.

### Host validation

`host_validation` canonicalizes configured and presented hosts:

- comparison is case-insensitive;
- an optional port is ignored for host allowlisting;
- a terminal DNS dot is normalized;
- malformed host syntax, user-info, paths, invalid ports, and unbracketed IPv6 literals are rejected.

The middleware still requires an explicit allowlist to restrict accepted hosts. With an empty allowlist it validates syntax but accepts any valid host.

### Trusted proxies

Forwarded client data is ignored unless the direct socket peer is configured as trusted.

For `X-Forwarded-For`, Gungnir walks the chain from the direct peer toward the client and stops at the first untrusted hop. This prevents a client-controlled leftmost value from overriding the address when a trusted reverse proxy appends to an existing chain.

`X-Real-IP` is only a fallback when `X-Forwarded-For` is absent. `X-Forwarded-Proto` can update the request secure flag only across the trusted-proxy boundary.

### CORS

CORS preflight is now handled by the middleware rather than depending on an application OPTIONS route.

The middleware:

- validates the presented origin against the configured single origin or allowlist;
- validates requested methods and headers;
- returns a 204 preflight response for an allowed request;
- returns 403 for a denied preflight;
- emits `Vary` for origin/preflight dimensions;
- supports `Access-Control-Allow-Credentials`;
- rejects the insecure/invalid wildcard-origin plus credentials combination;
- emits a bounded configurable preflight max age.

### Rate limiting

The process-local limiter validates its configuration, emits limit/remaining metadata and `Retry-After` on rejection, and bounds the number of client buckets with `max_clients`.

Expired buckets are pruned before capacity rejection. If the bucket table is still full, a new client is rejected instead of growing process memory without bound.

A process-local limiter is not a distributed rate limiter. Multi-instance deployments still require a shared atomic backend when a global quota is needed.

### Cookies and sessions

Cookie serialization enforces browser cookie-prefix invariants:

- `__Secure-` cookies require `Secure`;
- `__Host-` cookies require `Secure`, `Path=/`, and no `Domain`;
- `SameSite=None` continues to require `Secure`.

Session identifiers and CSRF tokens continue to use the operating system cryptographic random source. Authentication state changes rotate session identifiers, and CSRF comparison remains constant-time.

### Security response headers

The default security-header middleware continues to emit CSP, frame, MIME-sniffing and referrer controls. Requests considered secure also receive:

```text
Strict-Transport-Security: max-age=31536000
```

Only mark a proxied request secure through a configured trusted proxy.

## CI security gate

Phase 14 promotes the following suites into primary pull-request CI:

- `gungnir.security`
- `gungnir.security_hardening`
- `gungnir.csrf`
- `gungnir.auth_session`
- `gungnir.session_lifecycle`
- `gungnir.view_security`

The hardening suite includes adversarial cases for HTTP framing, header injection, CORS preflight, proxy spoofing, Host handling, cookie prefixes, request IDs and rate-limit state growth.

## Remaining boundaries

Phase 14 does not claim a blanket security certification. Applications still need to define authorization rules, allowed hosts/origins/proxies, upload policy, secrets management, TLS termination, distributed rate limiting, database privileges, logging redaction and production error policy.

Optional protocol/adaptor implementations remain subject to their own conformance and integration tests.

## Implementation references

- [include/gungnir/http/security.hpp](../include/gungnir/http/security.hpp)
- [src/http/security.cpp](../src/http/security.cpp)
- [src/http/message.cpp](../src/http/message.cpp)
- [src/http/server.cpp](../src/http/server.cpp)
- [src/http/cookie.cpp](../src/http/cookie.cpp)
- [src/security/security.cpp](../src/security/security.cpp)
- [tests/security_hardening.cpp](../tests/security_hardening.cpp)

See [Security](security.md), [Authentication](authentication.md), [Sessions](session.md), and [Production](production.md).
