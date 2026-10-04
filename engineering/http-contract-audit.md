# Canonical HTTP contract audit

Baseline: `8033cf6837f9cdc55ab5dfb5f805076f015dde67` on `main`.
This branch is independent of the model/ORM implementation in PR #175.

## Proven gaps

Native `Request::json`, metadata, input groups and authentication checks existed,
but the canonical validator rejected their documented language calls. Native
response setters returned references that escaped temporary call receivers in
generated chains. Malformed JSON reached the HTTP exception renderer as an
ordinary parsing exception and became a server error.

## Implemented contract

`SyntaxParser -> ProgramValidator -> ValidatedProject -> structural C++ IR ->
C++23 -> native runtime` now carries JSON access, optional object lookup, JSON
kind inspection, arrays/objects, request input maps, metadata and auth-state
checks, response headers/status/body, no-content and buffered downloads.
Builtins validate argument types and arity. Mutable response setters reject
immutable bindings before emission. Native request methods and JSON parsing remain
the underlying implementations.

Non-async call results use their validated value type, preserving schema-builder
references to enclosing table definitions. This lifetime repair and repeated-call
argument reset match the corresponding fixes in PR #175. Missing JSON members
return an owned optional; a present null member remains a present JSON value.
Malformed or empty JSON read through `request.json()` returns HTTP 400 without
including the body in the error response.

## Evidence

`tests/fixtures/structured/http.gnr` compiles into a native executable in
`tests/structured_http.cpp`. It exercises controllers, all input sources, structured
JSON type preservation, temporary-response getters, nested calls, response copies,
header injection rejection, middleware before/after behavior, anonymous redirect,
session/authenticated requests and the HTTP validation/exception paths. Invalid
API signatures and immutable mutations have matching diagnostics in emission and
validation-only modes.

`tests/docs_http_examples.py` checks public request/response fragments and canonical
middleware declarations in their documented action context. Route-registration
fragments use the application's separate route compilation path and are outside
this checker. CI compiles and executes the generated HTTP program against both
the build tree and installed package. Compiler conformance compares HTTP C++,
validated AST and IR across GCC, Clang and MSVC.

Local GCC 13 Debug verification: **84/84 CTest tests pass**, including installed
CLI generation, migrations, HTTP handling and dev restart/error handling.
The generated HTTP executable and external native consumer also build and run
against an installed package. **19 public HTTP examples/declarations pass strict
validation**. Documentation/development checks and workflow YAML parsing pass.
Remote compiler/platform results are recorded in the associated PR after inspection.

## Remaining product requirements

This batch does not complete the whole HTTP/authentication contract. Canonical
session mutation, cookies on responses, uploaded files, streaming and WebSocket
construction, credential attempts, typed user/model hydration and remember-token
operations still need their own integration evidence. Routing groups, named
aliases and typed model binding require an application-level acceptance test.
The full subsystem matrix remains in PR #175's engineering audit; the 1.0 gate
has not been met. No package or language version is changed here.
