# Canonical validation and upload contract audit

Baseline: merged main `dd3406085e7e252499e2b889d425c6aa5ba52a34`
(PR #180), tree `9f2bf1eb4b2c5e9228237ac3d95df7057c2b5921`, verified equal
to the reviewed routing head. All seven preceding workflows passed.

## Proven contract gaps

The public validation guide promised UUIDs, URLs, dates, conditional requirements,
files/images and custom rules, while the native validator accepted a smaller rule
set. Structured checks converted scalars to strings, could disclose undeclared
children when a parent container was selected, and did not expose a canonical
nonthrowing result. Literal malformed definitions reached application execution.
Request body parsing did not handle multipart uploads. Two public headers defined
incompatible `UploadedFile` types.

## Shared validation contracts

One definition parser serves ProgramValidator and the native validator. It checks
paths, known names, arity, numeric bounds, comparisons and SQL identifiers even
for absent fields. Literal failures have `GNR2330` source spans in both emission
and validation-only modes. Dynamic definitions retain native checks. Calls remain
resolved expressions in the existing structural C++ IR; the compatibility source
lowerer is not part of this path.

One validation engine now handles JSON and the retained string-map overloads.
Selection keeps scalar types and nested shape. Parent containers with descendant
rules expose only selected children; failed ancestors exclude descendants from
the partial result. Presence, nullable/sometimes, wildcard comparisons,
conditional requirements, type/size rules and bail share their native behavior.
Integer bounds compare decimal representations without double rounding; floating
JSON bounds use floating semantics. UUID spelling, Gregorian dates, explicit
timestamp offsets and HTTP/HTTPS URLs have deterministic documented contracts.

`request.check` and static/instance `Validator::check` return an owning canonical
`ValidationResult` with selected values and field errors. Reusable custom rules
use an explicitly passed, owning registry with fixed messages and synchronous
two-Json predicates. Copies, named functions, captured factories and injected
instances retain ownership. Independently constructed registries remain isolated;
registration and lookup synchronize access without holding a lock in predicates.
Opaque registry, result and upload handles cannot be serialized or stored in
model fields.

Native database rules retain the existing active connection and bound SQL path.
Recording drivers verify values/ignore IDs stay in bindings and use the backend's
identifier quoting and PostgreSQL placeholder numbering. SQLite acceptance runs
real uniqueness/existence queries. This does not establish live equivalence of
every SQL adapter.

## Multipart uploads and storage

The shared upload struct retains the original `security.hpp` fields, adds owned
accessors, and is used by `files.hpp` and Request. Header isolation includes both
entry points. `file`, `files` and `hasFile` expose owned values; empty file
selections, repeated files, basename extraction and binary contents are explicit.
Limits bound body, file, field, header and part counts. Parsing is transactional;
malformed input repeatedly raises 400 rather than being swallowed by `form()`.
Limit failures raise 413.

File validation uses trusted upload context, never submitted JSON metadata.
Image and MIME checks inspect bytes and container bounds independently of client
extensions/media types. PNG checks chunk CRCs; JPEG, GIF and still WebP check
framing. These are format-recognition checks, not pixel decoders. Size rules use
bytes. Validated bytes pass to the existing configured Storage facade and native
disk adapter; a failed validation runs no storage write.

Parser framing follows [RFC 7578](https://www.rfc-editor.org/rfc/rfc7578) and
[RFC 2046](https://www.rfc-editor.org/rfc/rfc2046). Container recognition refers to
the [PNG specification](https://www.w3.org/TR/png-3/),
[JPEG specification](https://www.w3.org/Graphics/JPEG/itu-t81.pdf),
[GIF specification](https://www.w3.org/Graphics/GIF/spec-gif89a.txt), and
[WebP container specification](https://developers.google.com/speed/webp/docs/riff_container).
Timestamp and URL contracts use documented subsets of
[RFC 3339](https://www.rfc-editor.org/rfc/rfc3339) and
[RFC 3986](https://www.rfc-editor.org/rfc/rfc3986); UUID spelling follows
[RFC 9562](https://www.rfc-editor.org/rfc/rfc9562).

## Acceptance evidence

The generated fixture covers rule diagnostics/IR verification, signed/unsigned
boundaries, fractional values, locale independence, typed scalar comparisons,
selected descendants, wildcard conditions, custom registry ownership/isolation,
real SQL checks, binary/repeated uploads, malformed boundaries and every limit.
Four fixed encoded image samples and every truncated prefix check recognition.
Request JSON cannot spoof an upload. A real suspended middleware retains its
upload and captured registry after the request parse cache is invalidated.

Ten public validation snippets, 22 request/response/middleware snippets and seven
cache/storage snippets pass strict canonical validation. The installed CLI
acceptance application now exercises nested validation, reusable rules, 400/422
responses and binary uploads into its configured local disk without editing
generated C++. Installed SDK and GCC/Clang/MSVC gates compile and execute the
same fixture. Conformance compares its emitted C++, validated AST and C++ IR.
Local GCC 13 Debug acceptance passed all 98 registered checks and nine installed
SDK consumers. The final full run passed the 97 non-CLI checks; the unchanged
no-op rebuild assertion required an isolated CLI rerun, which passed with Ninja
and the local SQLite dependency prefix. An earlier Makefiles reload timed out
after passing the new HTTP validation/upload cases. Invalid local build artifacts
were rebuilt without source workarounds. Final remote results are recorded in
the PR after verification.

## Remaining product requirements

This evidence completes the documented local validation and request-upload path.
Dedicated request declaration syntax remains a design document and is not added
as another compiler route. Full application acceptance for events/jobs/scheduling,
mail/notifications, resource authorization, wider adapter matrices, security and
performance remains in the baseline product audit. No release or completeness
version is changed.
