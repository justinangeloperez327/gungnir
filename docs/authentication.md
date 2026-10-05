# Authentication

Gungnir authentication connects requests, sessions, user models, credential verification, guards, and authorization.

## Authentication flow

A guard resolves the authenticated user for the current request. Session-based applications persist the authenticated identity in the session and restore it on later requests.

## Login

Applications authenticate credentials through the configured guard:

```gnr
controller LoginController {
    store(Request request) {
        const credentials = request.validate({
            "email": "required|email",
            "password": "required|string"
        });

        if (!auth.attempt(request, credentials, request.input("remember") == "1")) {
            return redirect("/login");
        }

        return redirect("/dashboard");
    }
}
```

Successful session authentication rotates the session identifier.

`auth.attempt(request, credentials, remember = false)` accepts an object with
string `email` and `password` fields and a boolean remember choice. Invalid
credentials return `false`; malformed credential objects raise HTTP 400. Input
validation errors use the validation response. Request input values are strings,
so compare a submitted remember choice explicitly.

## Guard configuration

Build Gungnir with `GUNGNIR_WITH_PASSWORD=ON` to enable the OpenSSL Crypto password
backend, or install the [application SDK](sdk-packages.md), which includes it.
Register one shared `SessionGuard` in `ServiceOptions.authentication`.
Install session middleware, identity-restoration middleware, and guard middleware
in that order:

```cpp
auto guard = std::make_shared<gungnir::auth::SessionGuard>(
    credential_resolver, identity_resolver, remember_store);
app.provider<gungnir::ServicesProvider>(
    gungnir::ServiceOptions{.authentication = guard});
app.router().use(gungnir::session::middleware(session_store));
app.router().use(gungnir::auth::session(identity_resolver));
app.router().use(gungnir::auth::guard(guard));
```

The credential resolver returns a `PasswordIdentity` containing the public
identity and its stored hash; the identity resolver restores an identity by id.
Guard middleware recalls valid remember tokens before the action and attaches
guard cookies to the final response after awaited middleware completes. The
request's auth context owns these staged cookies. Native callers can also use the
guard overloads that accept an explicit response.

Use the same guard instance for service registration and middleware. Place guard
middleware before actions and middleware that call the authentication APIs.
Request-only guard calls reject a missing, mismatched, or finished guard scope.

## Current user

Authenticated requests expose the resolved user:

```gnr
const user = request.user();
```

Applications can also test whether the request is authenticated or a guest.

`user()` returns an owned `AuthIdentity?` snapshot. Guard a missing identity before accessing `id`, public `attributes`, or `role`:

```gnr
const user = request.user();
if (user != null) {
    const id = user.id;
    const editor = user.role("editor");
    const name = user.attributes["name"];
}
```

Identity providers define public attributes and resolve identities from user models. Password hashes remain in credential providers. JSON serialization includes the identity identifier, sorted roles, and public attributes. Snapshots remain valid after the request finishes and do not modify authentication state. Model policies default to looking up the actor by identity id; applications can register a different mapping.

## Logout

Logout clears authenticated state, rotates or invalidates the session as appropriate, and revokes remember-me credentials associated with the session.

```gnr
controller LogoutController {
    destroy(Request request) {
        auth.logout(request);
        return redirect("/login");
    }
}
```

## Passwords

Gungnir provides versioned password hashing and verification. Applications store password hashes rather than plaintext credentials.

```gnr
const encoded = Password::hash("example-password");
const valid = Password::verify("example-password", encoded);
const needsUpdate = Password::needsRehash(encoded);
```

Hash a submitted password when creating or changing credentials. Verify against
the stored hash; do not hash again and compare the two randomly salted strings.

## Remember me

Tokens are stored as digests, rotated when consumed, and revoked on logout.
`MemoryRememberStore` is process-local. Use `RedisRememberStore` with Redis 6.2
or later for persistent, atomic recall across application instances. Build with
`GUNGNIR_WITH_REDIS=ON` and link `gungnir::redis`; generated applications link
available Redis adapters.

Configure persistent sessions and remember tokens together in `bootstrap/app.hpp`:

```cpp
#include <gungnir/auth/redis_remember_store.hpp>
#include <gungnir/session/redis_store.hpp>

gungnir::session::RedisSessionSettings sessions;
sessions.redis.host = "redis.internal";
sessions.redis.prefix = "myapp:session:";
auto session_store = std::make_shared<gungnir::session::RedisStore>(sessions);

gungnir::auth::RedisRememberSettings tokens;
tokens.client.nodes = {{"redis.internal", 6379}};
tokens.prefix = "myapp:remember:";
auto remember_store = std::make_shared<gungnir::auth::RedisRememberStore>(tokens);
```

Pass these stores to the guard and middleware configuration above. The remember
store uses the existing Redis client's credentials, timeouts, TLS, and topology
options. Load deployment secrets in native bootstrap and use application-specific
key prefixes. Session and remember cookies default to Secure and HttpOnly.
Configure HTTPS, SameSite, proxy trust, and CSRF protection for the deployment.

Remember records contain the public identity id, keyed by the token digest, with
an absolute expiry. Recall atomically consumes the record before rotating it;
replay cannot authenticate a second process. Logout revokes the associated token
and invalidates that session. A new token must persist before login installs
authenticated state. Store failures propagate through application error handling.

Register each store's `ping()` method with required readiness checks. Persistence
across server restarts also depends on Redis retention and durability settings.

## Authentication middleware

Protected routes use authentication middleware:

```gnr
Route::get("/account", AccountController::show)
    .middleware("auth");
```

Unauthenticated access follows the application's configured authentication response behavior.

## Authorization

Authentication establishes identity. Access to a specific resource or operation is handled by policies and authorization rules.

See [Policies and Authorization](policy.md).
