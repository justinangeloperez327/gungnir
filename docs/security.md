# Security

> **Status: experimental, pre-1.0.** This page describes the current implementation. Native C++ APIs and `.gnr` syntax are identified separately. Proposed contracts are in the [design specification](design/security.md).

## Current behavior

Native security building blocks include HTTP security middleware, trusted proxy configuration, session/CSRF integration, random identifiers and authorization decisions. Views escape ordinary interpolation and database APIs support parameter bindings.

Configure the trusted proxy boundary, host validation, CORS, rate limits, session cookies and body/header limits for the actual application. Enforce authorization at the operation boundary.

## Limits and planned work

These mechanisms do not establish a blanket security audit. Raw view output bypasses escaping; model view conversion does not promise hidden-field filtering; session authentication needs application credential verification. Validate all trust boundaries with the selected runtime configuration.

## Implementation references

- [include/gungnir/http/security.hpp](../include/gungnir/http/security.hpp)
- [include/gungnir/security/random.hpp](../include/gungnir/security/random.hpp)
- [include/gungnir/session/middleware.hpp](../include/gungnir/session/middleware.hpp)
- [include/gungnir/auth/authorization.hpp](../include/gungnir/auth/authorization.hpp)

See the [documentation index](README.md), [getting started](getting-started.md), and [target design](design/security.md).
