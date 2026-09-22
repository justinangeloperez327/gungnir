# Error Handling

Gungnir separates framework error taxonomy from HTTP rendering.

## Framework errors

`errors::Error` is the common framework-owned runtime error base and carries an optional stable code. Configuration and runtime specializations provide an initial taxonomy without forcing unrelated standard-library or application exceptions into Gungnir types.

Subsystem-specific exceptions may continue to expose richer information where their contracts require it.

## HTTP rendering

The HTTP exception handler maps known validation, model and HTTP failures to responses. Unknown exceptions produce a generic 500 response.

Production responses must not expose exception messages, stack traces, filesystem paths, credentials, generated C++ internals or other implementation details for unknown failures.

Validation failures retain their structured 422 response. Model-not-found failures remain 404 responses. Explicit HTTP exceptions retain their declared status.

## Debugging

Detailed diagnostics belong in logs and development tooling rather than unconditional HTTP responses. A future debug renderer may expose additional development-only context, but it must be controlled by application mode and remain disabled in production.

## Exception ownership

Gungnir does not swallow exceptions merely to make an operation appear successful. Boundaries must either handle an exception according to their documented contract, translate it to a domain-specific error, or propagate it.
