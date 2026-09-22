# Gungnir core maturity

This branch establishes the next framework-maturity layer while preserving Gungnir's expressive surface.

## Foundations

- HTTP security middleware: CORS, security headers, body limits and host validation.
- Request observability: request IDs and server timing.
- Rate-limiting foundation suitable for replacement by distributed stores.
- Session value abstraction and uploaded-file abstraction.
- Development command: `gungnir dev`.
- Existing request cookie parsing, asynchronous task routing, HTTP runtime, view rendering and diagnostics remain the integration points for subsequent work.

## Next integration layer

The public foundations are intentionally small. Authentication, authorization, CSRF, trusted proxies, multipart parsing, model route binding, static/download responses, language-server protocol support, formatter policy, module/import resolution and incremental compilation should build on these primitives without exposing C++ plumbing to Gungnir application code.
