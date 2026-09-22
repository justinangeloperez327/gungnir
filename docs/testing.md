# Testing

Gungnir provides small testing helpers over the real router and HTTP request/response types.

## HTTP tests

`testing::Http` dispatches requests directly through a router. It does not open a socket, so route and middleware behavior can be exercised without a network server.

Available convenience methods include GET, POST and the generic `send` method.

## Response assertions

Helpers validate status codes, response body fragments and headers. Failed assertions throw with a focused message so they can be used from the project's existing executable-style tests or another C++ test runner.

## Isolation

The testing layer does not silently reset global or external state. Database transactions, cache stores, sessions, queues and other resources must be isolated explicitly by the test environment until their runtime contracts provide safe scoped reset mechanisms.

## Scope

These helpers are intentionally thin. They exercise production routing code rather than introducing a second fake routing implementation.
