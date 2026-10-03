# Middleware

Middleware runs around route and controller execution. It is used for authentication, authorization, sessions, CSRF protection, CORS, logging, rate limiting, and other request-level concerns.

## Defining middleware

```gnr
middleware EnsureActiveUser {
    async handle(Request request, Next next) {
        if (!request.authenticated()) {
            return redirect("/login");
        }

        return await next(request);
    }
}
```

Middleware receives the request and a `Next` continuation. Calling `next` passes the request to the remainder of the middleware pipeline.

## Before and after behavior

Code before `next` runs before downstream middleware and the route handler. Code after the awaited continuation can inspect or modify the response.

## Registration

Middleware can be registered as:

- application/global middleware;
- named aliases;
- middleware groups;
- route-specific middleware.

## Route middleware

```gnr
Route::get("/dashboard", DashboardController::index)
    .middleware("auth");
```

## Dependency injection

Middleware can receive services from the application container through dependency injection.

## Async middleware

The continuation is asynchronous. Middleware that forwards to downstream handlers normally uses `async handle` and `await next(request)`.

## Ordering

Application middleware registration supports deterministic ordering and priority so security and request-context middleware run in the intended sequence.
