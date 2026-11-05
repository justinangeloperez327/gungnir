# Error handling

Gungnir automatically catches failures from routed controllers and middleware.
Each application owns an exception handler; configuration does not leak between
applications or coroutine requests.

| Failure | Default status |
| --- | --- |
| Invalid request | 400 |
| Authentication required | 401 |
| Authorization denied | 403 |
| Missing route or model | 404 |
| Validation failure | 422, with field errors for JSON requests |
| Unexpected exception | 500, with a generic public message |

Responses follow the request's accepted content type: JSON for API requests,
HTML for `Accept: text/html`, and plain text otherwise. A registered route
fallback still controls unmatched routes. Missing bound models fail before the
action runs, and soft-deleted models remain excluded.

## Structured Gungnir exceptions

```gnr
controller UserController {
    update(Request request) {
        try {
            const data = request.validate({ "name": "required|string" });
            return json(data);
        } catch (ValidationError error) {
            return json({ "message": error.message(), "errors": error.errors() }, 422);
        } catch (Error error) {
            report(error, request);
            return text("Unable to update the user", 500);
        }
    }
}
```

`Error` catches every native exception, including unknown native thrown values.
Specific catches include `HttpError`, `ModelNotFoundError`, `ValidationError`,
`AuthenticationError`, `AuthorizationError`, and `DatabaseError`. Put specific
catches before `Error`; duplicate and unreachable catches are compile errors.
Catch variables are immutable and scoped to their own handler. Errors expose
`message()`, `status()`, `type()`, and `errors()`; the last returns validation
field errors or an empty object.

Use `throw Error("Internal failure");` or `throw "Internal failure";` to raise an
application error. `throw;` rethrows the original exception inside a catch.
Rethrow cannot cross a callable boundary. `abort(409, "Conflict");` raises an
HTTP exception; statuses must be between 400 and 599. A message is optional.

Async try bodies and catch bodies can use `await`. The compiler retains the
original exception across suspension, so `throw;` still preserves its type and
payload. Returns, loop control, and null guards are checked across all paths.
There is no `finally` clause; native resource cleanup follows RAII.

## Development diagnostics

Set `APP_DEBUG=true` in a development environment to show an error's message,
type, available source file and line, and native stack frames. Both JSON and HTML
responses support diagnostics. Errors raised by structured `.gnr` source retain
the original Gungnir file and line even when C++ line directives are disabled.

Stack capture is bounded and depends on the platform. Gungnir Error instances
capture frames when constructed. Exceptions originating in other native
libraries may only provide frames from the handler, rather than the original
throw site. Windows frames are addresses; symbol names elsewhere depend on the
native binary's symbol information. No source snippet or request data is read
into the debug page.

Applications running in `production` or `prod` suppress built-in debug output,
even if `APP_DEBUG=true` or `debug(true)` was set on the handler. Default 5xx
responses never expose exception messages, paths, or traces. Error responses
are not cached, and generated HTML escapes exception content.

## Reporting and rendering hooks

Configure hooks in `bootstrap/app.hpp`, before serving requests:

```cpp
app.exceptions().on_render(
    [](gungnir::Request& request, const gungnir::errors::Error& error)
        -> std::optional<gungnir::Response> {
        if (error.status() == 404 && !request.expects_json()) {
            return gungnir::Response::html("<h1>Page not found</h1>", 404);
        }
        return std::nullopt; // Use the default response.
    }
);
```

`render<E>(callback)` registers a native exception-type-specific renderer. Its
callback receives `(const E&, Request&)` and returns an optional response.
Renderers are tried in registration order; `std::nullopt` continues to the next
renderer or the default response. Custom renderers control their own response
content and cache policy.

`report<E>(callback)` registers a native type-specific reporter with the same
arguments. Return `true` when reporting is completely handled, or `false` to
continue to default logging. `on_report(callback)` receives `(Request&, const
errors::Error&)` for any exception. `dont_report<E>()` suppresses reporting for a
type without changing rendering. Reporting and rendering hooks fail safely:
a hook failure is logged and the original error uses the default behavior.
Configure hooks during bootstrap, rather than mutating them concurrently.

Unexpected errors are automatically sent to the configured `logging::Logger`.
If no logger with a sink is available, structured JSON is written to standard
error. Logs include the error message, type, status, available source location,
bounded native stack frames, and trace correlation IDs. Expected 4xx failures are not logged by default,
although explicitly registered reporters can handle them. The framework does
not attach request bodies, authorization headers, query strings, or submitted
validation values. Logging remains best effort if the configured sinks fail.

Use `report(error, request)` to report a handled error without changing the HTTP
response. Reporting and subsequently rethrowing an error can produce two log
records. Rendering and reporting can also be called directly through
`app.exceptions()` from native application code.

## Compiler diagnostics

Syntax, type and framework errors are reported before native compilation, with
a diagnostic code, source location and span. Exception contract errors use
`GNR2380`. Compile a project with `gungnirc ROOT --project --check`, or use
`gungnir build` / `gungnir dev` for application validation and compilation.

See [Validation](validation.md), [Controllers](controller.md),
[Logging and observability](logging-observability.md), and [Production](production.md).
