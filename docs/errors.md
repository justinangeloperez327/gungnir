# Errors and Diagnostics

Gungnir reports language and framework errors with a diagnostic code,
message, source location and source span.

## Check an application file

```sh
gungnirc app/controllers/HomeController.gnr --check
```

The compiler reports errors in `.gnr` source and exits with a nonzero
status. Fix those errors before running `gungnir build`. Native
compilation can also report missing C++ toolchains, headers or
optional SDK adapters.

## HTTP and application errors

Runtime failures can arise from validation, routes, database connections,
ORM operations, views, storage and external services. Configure the HTTP
`ExceptionHandler` so clients receive appropriate error responses rather
than uncaught native exceptions.

For production applications, keep stack traces and sensitive internal
details out of public responses and send diagnostic details to
[Logging and Observability](logging-observability.md).

See [Validation](validation.md), [Responses](response.md) and
[Production](production.md).
