# Error Handling

> **Design specification.** This document preserves the intended contract. Examples and requirements below may exceed the current implementation. See the [current implementation guide](../errors.md) before using an API.

Gungnir separates language/compiler diagnostics, application/runtime errors, and HTTP error rendering.

These are different layers and must not be conflated.

# Compiler diagnostics

Syntax, semantic, type, control-flow, and framework-contract errors are compile-time diagnostics.

Examples:

~~~text
unknown symbol
invalid type
invalid return
route action not found
invalid await
invalid relationship
~~~

Compile-time errors prevent Validated AST construction and normal code generation.

# Runtime errors

Runtime failures occur after a program has compiled.

Examples:

- database unavailable;
- mail transport failure;
- storage I/O failure;
- queue lease failure;
- invalid runtime configuration;
- network timeout.

Framework-owned runtime errors should use a stable taxonomy and optional error codes where useful.

# Application errors

Application/domain errors may remain application-defined.

The framework should not force every application failure into one giant Gungnir exception hierarchy.

# HTTP rendering

The HTTP boundary converts known request/application/framework failures into responses.

Typical mappings include:

~~~text
validation failure      -> 422
model/resource missing  -> 404
unauthenticated         -> 401
authorization denied    -> 403
explicit HTTP error     -> declared status
unknown runtime failure -> 500
~~~

Exact application response formatting may be configurable.

# Production safety

Production error responses must not expose:

- stack traces;
- filesystem paths;
- credentials;
- database connection strings;
- generated C++ internals;
- native exception text from unknown failures;
- private application source paths.

Detailed context belongs in logs/observability.

# Development diagnostics

Development mode may provide richer error pages or source diagnostics.

Debug rendering must be controlled by application mode and disabled in production.

# Error ownership

A boundary that receives an error must:

- handle it according to a documented contract;
- translate it to a domain/framework error;
- or propagate it.

It must not swallow the failure merely to make an operation appear successful.

# Async errors

Errors from awaited operations propagate through the async execution model.

Cancellation is distinct from ordinary failure and should remain distinguishable.

The application language should not expose native C++ coroutine exception plumbing.

# Error model direction

The language may use framework exception propagation, explicit Result<T,E>, or both depending on API design.

The final source-language error syntax must be defined deliberately before introducing try/catch/throw grammar.

Do not copy C++ exception syntax automatically.

# HTTP exceptions

Explicit HTTP failures may carry:

~~~text
status
safe public message
headers where allowed
stable error code
~~~

Header values must still pass security validation.

# Validation errors

Validation errors retain structured field/rule information.

They should not collapse into an opaque string before the response/view layer can use them.

# Database errors

Database drivers should preserve useful categories such as:

- connection failure;
- timeout/cancellation;
- constraint violation;
- transaction failure;
- query execution failure.

Raw vendor details may be logged but should not automatically be exposed to clients.

# Generated C++ boundary

Generated C++ may use:

- exceptions;
- expected/result objects;
- error codes;
- RAII cleanup;

depending on runtime design.

Those are implementation details.

# Design rule

~~~text
compile-time mistakes -> diagnostics
runtime failures       -> structured runtime errors
HTTP boundary          -> safe response mapping
production             -> no internal detail leakage
~~~

