# Middleware

> **Design specification.** This document preserves the intended contract. Examples and requirements below may exceed the current implementation. See the [current implementation guide](../middleware.md) before using an API.

A Gungnir middleware is an HTTP pipeline declaration.

Middleware runs around a route or controller action. It may inspect the request, short-circuit the request with a response, call the next middleware/action, and optionally inspect or modify the response on the way back out.

A middleware may contain only:

- injected dependencies;
- one public `handle(Request request, Next next)` action.

Middleware is not a general-purpose class.

## Basic middleware

```gnr
middleware AuthMiddleware {
    public handle(Request request, Next next) {
        if (!Auth::check()) {
            return redirect('/login');
        }

        return next(request);
    }
}
```

The `handle()` action has an implicit response contract, just like a controller action.

Developers do not write:

```gnr
Response handle(Request request, Next next) {
    // ...
}
```

The middleware context already defines the result as response-compatible.

# Middleware responsibility

A middleware should answer:

```text
Should this request continue?
Should the request be rejected or redirected?
Does request-scoped context need to be established?
Should something happen before the route action?
Should something happen to the response after the route action?
```

Typical middleware responsibilities include:

- authentication checks;
- authorization gates that apply broadly;
- CSRF protection;
- CORS;
- rate limiting;
- session setup;
- request IDs and tracing;
- security headers;
- localization;
- tenant resolution;
- request/response logging.

Application business workflows belong in controllers, services, events, or other application layers.

# The handle action

Every middleware declares exactly one public `handle()` action:

```gnr
middleware ExampleMiddleware {
    public handle(Request request, Next next) {
        return next(request);
    }
}
```

Canonical signature:

```text
public handle(Request request, Next next)
```

For the initial middleware contract:

- the action name must be `handle`;
- the first parameter is `Request request`;
- the second parameter is `Next next`;
- the action has an implicit response contract.

# Calling the next middleware

Continue the pipeline with:

```gnr
return next(request);
```

Conceptually:

```text
current middleware
    -> next middleware
    -> next middleware
    -> controller action
    -> response
    -> middleware unwind
```

A middleware that does not call `next(request)` short-circuits the pipeline.

# Short-circuiting

Middleware may return a response before the controller action executes.

```gnr
middleware AuthMiddleware {
    public handle(Request request, Next next) {
        if (!Auth::check()) {
            return json({
                'message': 'Unauthenticated'
            }, 401);
        }

        return next(request);
    }
}
```

Redirect example:

```gnr
middleware GuestMiddleware {
    public handle(Request request, Next next) {
        if (Auth::check()) {
            return redirect('/dashboard');
        }

        return next(request);
    }
}
```

# Work before the next handler

Middleware may perform request-side work before continuing:

```gnr
middleware RequestIdMiddleware {
    public handle(Request request, Next next) {
        const requestId = request.id();

        Logger::context({
            'request_id': requestId
        });

        return next(request);
    }
}
```

# Work after the next handler

Middleware may capture the returned response:

```gnr
middleware SecurityHeadersMiddleware {
    public handle(Request request, Next next) {
        const response = next(request);

        response.header(
            'X-Content-Type-Options',
            'nosniff'
        );

        return response;
    }
}
```

This gives middleware a normal before/after pipeline model.

# Asynchronous middleware

Use `public async handle()` when middleware performs asynchronous work.

```gnr
middleware AuditMiddleware {
    inject AuditService audit;

    public async handle(Request request, Next next) {
        await audit.recordRequest(request);

        const response = await next(request);

        await audit.recordResponse(response);

        return response;
    }
}
```

Canonical ordering is:

```text
public async handle(...)
```

not:

```text
async public handle(...)
```

An async middleware still has the same implicit response contract.

Conceptually:

```text
Gungnir source                    native contract

public handle(...)                Response
public async handle(...)          Task<Response>
```

The coroutine type is an implementation detail.

# Await rules

`await` is only valid inside async middleware:

```gnr
public async handle(Request request, Next next) {
    const allowed = await permissions.check(request);

    if (!allowed) {
        return response(null, 403);
    }

    return await next(request);
}
```

This is invalid:

```gnr
public handle(Request request, Next next) {
    const allowed = await permissions.check(request);

    return next(request);
}
```

The semantic analyzer should report that `await` requires an async middleware action.

# Dependency injection

Middleware dependencies use `inject`:

```gnr
middleware TenantMiddleware {
    inject TenantResolver tenants;

    public handle(Request request, Next next) {
        const tenant = tenants.resolve(request.host());

        if (tenant == null) {
            return response(null, 404);
        }

        return next(request);
    }
}
```

Syntax:

```text
inject Type name;
```

Gungnir's container resolves middleware dependencies.

Application code does not need explicit constructors for dependency injection.

# No explicit constructors

Do not write:

```gnr
middleware AuthMiddleware {
    AuthMiddleware(AuthService auth) {
        this.auth = auth;
    }
}
```

Use:

```gnr
middleware AuthMiddleware {
    inject AuthService auth;

    public handle(Request request, Next next) {
        // ...
    }
}
```

# No arbitrary fields

Middleware should not carry arbitrary mutable state:

```gnr
middleware BadMiddleware {
    int requestCount = 0;
    string currentUser;
}
```

The middleware declaration should contain only:

```text
inject declarations
public handle(...)
```

Request-specific state belongs to the request context or another scoped dependency.

# No arbitrary methods

For the initial middleware contract, middleware does not contain helper methods such as:

```gnr
private checkUser() {
    // ...
}
```

If behavior becomes substantial or reusable, move it to an injected service:

```gnr
middleware AuthMiddleware {
    inject AuthService auth;

    public handle(Request request, Next next) {
        if (!auth.allowed(request)) {
            return response(null, 403);
        }

        return next(request);
    }
}
```

# Attaching middleware to routes

Middleware may be attached directly to a route:

```gnr
Route::get('/profile', ProfileController::show)
    .middleware(AuthMiddleware);
```

Multiple middleware may be attached in order:

```gnr
Route::post('/orders', OrderController::store)
    .middleware([
        AuthMiddleware,
        VerifiedMiddleware,
        RateLimitMiddleware
    ]);
```

Middleware executes in declared order.

# Route-group middleware

Middleware may apply to a route group:

```gnr
Route::middleware(AuthMiddleware)
    .group(() => {
        Route::get('/profile', ProfileController::show);
        Route::post('/logout', AuthController::logout);
    });
```

Multiple middleware:

```gnr
Route::middleware([
        AuthMiddleware,
        VerifiedMiddleware
    ])
    .group(() => {
        Route::get('/dashboard', DashboardController::index);
    });
```

Routing attachment and group rules are defined in `routing.md`.

# Middleware order

Order is significant.

For:

```gnr
Route::get('/admin', AdminController::index)
    .middleware([
        SessionMiddleware,
        AuthMiddleware,
        AdminMiddleware
    ]);
```

request-side execution is conceptually:

```text
SessionMiddleware
    -> AuthMiddleware
        -> AdminMiddleware
            -> controller
```

Response-side execution unwinds in reverse:

```text
controller
    -> AdminMiddleware
        -> AuthMiddleware
            -> SessionMiddleware
```

The framework must preserve deterministic middleware ordering.

# Global middleware

Application-wide middleware may be configured in the application/bootstrap layer.

Examples might include:

```text
RequestIdMiddleware
CorsMiddleware
TrustedProxyMiddleware
BodyLimitMiddleware
```

Global middleware registration should not require repeating the middleware on every route.

The exact bootstrap/configuration syntax belongs in application documentation.

# Middleware aliases

Gungnir may support aliases for reusable middleware registration:

```text
auth       -> AuthMiddleware
verified   -> VerifiedMiddleware
throttle   -> RateLimitMiddleware
```

Application-facing route syntax should prefer typed middleware references where practical:

```gnr
.middleware(AuthMiddleware)
```

rather than requiring runtime string lookup.

Aliases remain useful for configuration, generated metadata, or interoperability.

# Middleware groups

Named middleware groups may combine several middleware declarations:

```text
web
  SessionMiddleware
  CsrfMiddleware
  AuthMiddleware
```

or:

```text
api
  ApiMiddleware
  RateLimitMiddleware
```

The final application syntax for declaring global middleware groups belongs in application/bootstrap documentation.

Routes should receive the resulting ordered middleware chain structurally rather than parse ad-hoc strings at runtime.

# Parameterized middleware

Gungnir should avoid Laravel-style colon-delimited runtime strings such as:

```text
throttle:60,1
role:admin
```

when the same intent can be represented structurally.

Prefer a typed/configured middleware form such as:

```gnr
Route::get('/admin', AdminController::index)
    .middleware(
        RoleMiddleware('admin')
    );
```

or another compiler-supported structured form defined when parameterized middleware is finalized.

The key rule is:

```text
middleware parameters should be structured syntax,
not strings that must be reparsed at runtime
```

# Authentication middleware

Typical authentication middleware:

```gnr
middleware AuthMiddleware {
    public handle(Request request, Next next) {
        if (!Auth::check()) {
            return json({
                'message': 'Unauthenticated'
            }, 401);
        }

        return next(request);
    }
}
```

The actual authentication system belongs in `authentication.md`.

# Authorization middleware

A broad authorization check may be implemented as middleware when it applies naturally to a route or route group.

```gnr
middleware AdminMiddleware {
    public handle(Request request, Next next) {
        const user = Auth::user();

        if (!user.isAdmin) {
            return response(null, 403);
        }

        return next(request);
    }
}
```

Resource-specific authorization should normally use the policy layer rather than grow into large middleware classes.

# CORS middleware

CORS is a middleware concern because it may inspect the incoming origin/method and modify the outgoing response.

Conceptually:

```gnr
middleware CorsMiddleware {
    public handle(Request request, Next next) {
        const response = next(request);

        response.header(
            'Access-Control-Allow-Origin',
            configuredOrigin
        );

        return response;
    }
}
```

Production CORS behavior should come from framework configuration rather than hard-coded values.

# Rate-limit middleware

Rate limiting may short-circuit before the controller:

```gnr
middleware RateLimitMiddleware {
    inject RateLimiter limiter;

    public handle(Request request, Next next) {
        if (!limiter.allow(request)) {
            return json({
                'message': 'Too Many Requests'
            }, 429);
        }

        return next(request);
    }
}
```

Rate-limit storage and algorithms belong to the runtime/service layer.

# Session middleware

Session middleware may establish request-scoped session state before continuing:

```gnr
middleware SessionMiddleware {
    public async handle(Request request, Next next) {
        await session.start(request);

        const response = await next(request);

        await session.commit(response);

        return response;
    }
}
```

Session persistence and lifecycle details belong in session documentation.

# Terminable work

Some systems need work after the response lifecycle has completed.

Gungnir should distinguish:

```text
response-side middleware work
```

from:

```text
post-send / terminable work
```

Normal code after `next(request)` runs while constructing/unwinding the response pipeline. It does not necessarily mean the network transport has finished sending the response.

If Gungnir exposes terminable middleware later, it should use an explicit lifecycle contract rather than pretending ordinary `handle()` code runs after bytes are sent.

# Error propagation

If downstream middleware or a controller raises a framework error, middleware may allow it to propagate to the global exception layer.

Middleware should not be required to wrap every call to:

```gnr
next(request)
```

Error-handling middleware may intentionally intercept errors when that behavior is part of its contract.

The global error/exception contract belongs in `errors.md`.

# Request lifetime

Middleware receives the same request-scoped request object used by the downstream action.

For async middleware, the runtime must preserve request state safely across suspension.

Application code should not need to manage native request-buffer lifetimes, references, pointers, or coroutine storage.

# What does not belong in middleware

Middleware should not contain:

- business workflows;
- database schema logic;
- controller actions;
- ORM model definitions;
- long-running background jobs;
- mail composition;
- large domain calculations;
- arbitrary persistent mutable state;
- native server-loop plumbing;
- C++ ownership management.

Middleware should remain focused on cross-cutting request/response behavior.

# Complete middleware example

```gnr
middleware AuthMiddleware {
    inject AuthService auth;

    public async handle(Request request, Next next) {
        const user = await auth.resolve(request);

        if (user == null) {
            return json({
                'message': 'Unauthenticated'
            }, 401);
        }

        auth.setCurrentUser(user);

        return await next(request);
    }
}
```

Usage:

```gnr
Route::middleware(AuthMiddleware)
    .group(() => {
        Route::get('/profile', ProfileController::show);
        Route::post('/logout', AuthController::logout);
    });
```

# Middleware grammar

The intended middleware grammar is approximately:

```text
middlewareDeclaration
  := 'middleware' Identifier '{'
       middlewareMember*
     '}'

middlewareMember
  := injectDeclaration
   | middlewareHandle

injectDeclaration
  := 'inject' Type Identifier ';'

middlewareHandle
  := 'public' 'async'? 'handle'
     '(' 'Request' Identifier ',' 'Next' Identifier ')'
     block
```

A middleware declaration must contain exactly one `handle` action.

There is no explicit return type because middleware context supplies the response contract.

# Middleware AST

Middleware should ultimately be represented structurally as:

```text
MiddlewareDeclaration
  name
  injections[]
  handle
```

Injection:

```text
InjectDeclaration
  type
  name
```

Handle action:

```text
MiddlewareHandle
  visibility = public
  async
  requestParameter
  nextParameter
  body
  responseContract = Response
```

Middleware should not be represented as an unrestricted generic C++ class.

# Semantic validation

The middleware semantic pass should validate at least:

- middleware names are valid and unique;
- only supported middleware members are present;
- exactly one `handle` action exists;
- the action is named `handle`;
- the action is public;
- the first parameter resolves to `Request`;
- the second parameter resolves to `Next`;
- injection types resolve;
- injection names are unique;
- `await` appears only in async middleware;
- all reachable returns are response-compatible;
- calls to `next` use a compatible request;
- arbitrary fields are rejected;
- constructors and destructors are rejected;
- unrelated arbitrary methods are rejected;
- route middleware references resolve to known middleware declarations where project information is available.

# Compiler contract

This middleware:

```gnr
middleware AuthMiddleware {
    inject AuthService auth;

    public async handle(Request request, Next next) {
        const user = await auth.resolve(request);

        if (user == null) {
            return response(null, 401);
        }

        return await next(request);
    }
}
```

should conceptually pass through:

```text
source
  -> lexer
  -> parser
  -> MiddlewareDeclaration AST
  -> InjectDeclaration / MiddlewareHandle AST
  -> dependency resolution
  -> Request / Next resolution
  -> async/await validation
  -> response-contract validation
  -> validated middleware AST
  -> middleware pipeline lowering
  -> C++23 generation
```

Before native generation, the compiler should already know:

```text
middleware        AuthMiddleware

dependency        auth
dependency type   AuthService

handle            handle
public            true
async             true

parameter         request
type              Request

parameter         next
type              Next

result contract   Response
```

The transpiler must not rediscover middleware structure by scanning raw source text.

# Generated C++ boundary

Gungnir source:

```gnr
public handle(Request request, Next next) {
    return next(request);
}
```

may lower to native middleware interfaces, callable continuations, response wrappers, or generated container wiring.

Async middleware may lower to a coroutine-backed native type such as:

```text
Task<Response>
```

Those types are implementation details.

Application code should not need to write:

```text
native middleware base classes
std::function continuation plumbing
Task<Response>
constructor injection boilerplate
request pointer/reference lifetime code
```

# Naming convention

Public middleware/application APIs use camelCase where multiple words are required.

The middleware declaration itself remains simple:

```text
middleware
inject
public
async
handle
```

Middleware references in routing use the middleware type directly:

```gnr
.middleware(AuthMiddleware)
```

rather than exposing internal registry or C++ type machinery.

# Design rule

The middleware contract is intentionally strict:

```text
middleware = injected dependencies + one public handle(Request, Next)
```

A middleware may:

```text
inspect request
short-circuit
call next
inspect/modify response
```

It should not become a general-purpose application class.

