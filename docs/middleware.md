# Middleware

Gungnir middleware is an asynchronous pipeline around a route handler. Middleware can execute logic before calling `next(request)`, short-circuit by returning a response, and execute response-side logic after the continuation completes.

Middleware classes are resolved through the application container, so dependencies remain constructor/container managed instead of being manually instantiated by routes.

The application owns a `MiddlewareRegistry`. Middleware types can be registered under aliases, aliases can be composed into groups, and groups can use a configured priority order. Routes and route groups can reference these aliases rather than carrying framework plumbing.

Example application configuration:

```cpp
app.middleware_alias<AuthMiddleware>("auth");
app.middleware_group("web", {"session", "csrf", "auth"});
app.middleware_priority({"session", "csrf", "auth"});
```

A route may then attach `"auth"`, while a route group may attach the `"web"` middleware group.

`TerminableMiddleware` defines the post-response contract for middleware that needs work after a response has completed. Execution belongs at the server/request lifecycle boundary; the router should not pretend that sending a response has completed while bytes may still be in transport buffers.

Parameterized middleware should be represented as structured middleware configuration rather than Laravel-style colon-delimited strings. This avoids reparsing mini-languages at runtime and keeps generated Gungnir code type-safe.

Request-scoped middleware dependencies depend on the container's scoped lifetime. Full request-scope concurrency must be integrated with the asynchronous request context rather than a process-global scope.
