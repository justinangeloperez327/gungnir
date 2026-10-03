# Authentication

> **Gungnir 1.x design specification.** This document describes intended long-term architecture beyond the frozen `1.0.0-rc.1` contract. Examples and requirements may exceed the current implementation. See the [current implementation guide](../authentication.md) before using an API.


## 1.0 RC alignment

The shipped baseline is `1.0.0-rc.1`. The 1.0 contracts are frozen; this design document may describe additive 1.x evolution or future-major work, but current implementation guides and executable tests remain authoritative.

Gungnir authentication identifies the current application user.

Authentication answers:

```text
Who is making this request?
Is the request authenticated?
What authenticated user is associated with the request?
```

Authorization is separate. Authentication establishes identity; policies and authorization decide what that identity may do.

The public API follows familiar Laravel-style naming where it maps cleanly to Gungnir.

# Basic authentication state

Check whether a user is authenticated:

```gnr
if (Auth::check()) {
    // authenticated
}
```

Check whether the current request is unauthenticated:

```gnr
if (Auth::guest()) {
    // guest
}
```

Retrieve the authenticated user:

```gnr
const user = Auth::user();
```

Retrieve the authenticated user's identifier:

```gnr
const id = Auth::id();
```

Authentication state is request-scoped.

It must not live in process-global or unsafe thread-local state.

# Authenticated user

`Auth::user()` returns the authenticated application user for the current request:

```gnr
const user = Auth::user();
```

If the route requires authentication, middleware should normally guarantee that the user exists before the controller runs.

Example:

```gnr
Route::get('/profile', ProfileController::show)
    .middleware(AuthMiddleware);
```

```gnr
controller ProfileController {
    public show() {
        const user = Auth::user();

        return json(user);
    }
}
```

For routes where authentication is optional, the result may be empty/null according to the optional-value contract.

# Check

```gnr
if (Auth::check()) {
    return json(Auth::user());
}
```

`check()` returns true only when a valid authenticated identity is available for the current request.

# Guest

```gnr
if (Auth::guest()) {
    return redirect('/login');
}
```

`guest()` is the inverse of `check()`.

# ID

```gnr
const userId = Auth::id();
```

This returns the stable identifier of the authenticated user without requiring application code to extract it manually from the user model.

# Attempt login

For session-backed username/password authentication:

```gnr
const authenticated = Auth::attempt({
    'email': request.input('email'),
    'password': request.input('password')
});
```

Typical controller:

```gnr
controller LoginController {
    public store(Request request) {
        const credentials = request.validate({
            'email': 'required|email',
            'password': 'required|string'
        });

        if (!Auth::attempt(credentials)) {
            return json({
                'message': 'Invalid credentials'
            }, 401);
        }

        return json({
            'user': Auth::user()
        });
    }
}
```

`attempt()` verifies credentials through the configured authentication provider.

Application code should not compare stored password hashes manually.

# Remember authentication

Where persistent login is enabled:

```gnr
Auth::attempt(credentials, remember: true);
```

Persistent-login behavior must use secure, revocable credentials and must not store raw passwords.

The exact remember-token persistence strategy belongs to the authentication adapter/runtime.

# Login an existing user

An already-resolved user may be authenticated directly:

```gnr
Auth::login(user);
```

This is useful after registration or another trusted identity-resolution flow.

By identifier:

```gnr
Auth::loginUsingId(userId);
```

The identifier must resolve through the configured user provider.

# Logout

```gnr
Auth::logout();
```

For session authentication, logout should:

- remove the authenticated identity from the session;
- rotate or regenerate the session identifier where appropriate;
- preserve or invalidate unrelated session data according to the logout/session contract;
- prevent session-fixation reuse.

A full application session invalidation remains a separate session operation when required.

# Session regeneration

After successful credential authentication, the framework should rotate the session identifier automatically or through the authentication/session middleware contract.

Application code should not need to manipulate raw session IDs.

# Authentication middleware

Protected routes should use middleware:

```gnr
Route::middleware(AuthMiddleware)
    .group(() => {
        Route::get('/profile', ProfileController::show);
        Route::post('/logout', LoginController::destroy);
    });
```

Typical middleware:

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

Browser applications may redirect instead:

```gnr
return redirect('/login');
```

Authentication middleware behavior may be configured according to whether the request expects HTML or JSON.

# Guest middleware

Routes intended only for unauthenticated users may use guest middleware:

```gnr
Route::middleware(GuestMiddleware)
    .group(() => {
        Route::get('/login', LoginController::create);
        Route::post('/login', LoginController::store);
    });
```

Conceptually:

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

# Authentication provider

Authentication must resolve users through a configured provider.

Conceptually:

```text
credentials
    -> authentication provider
    -> user lookup
    -> password/credential verification
    -> authenticated identity
```

The provider may use the ORM:

```text
User model
users table
```

but the authentication contract should not be hard-coded to one persistence backend.

# User model

An authenticatable user is normally represented by an application model:

```gnr
model User {
    fillable = [
        'name',
        'email',
        'password'
    ];

    hidden = [
        'password',
        'remember_token'
    ];
}
```

Authentication does not require business logic to be placed inside the model.

The model remains persistence metadata plus relationships.

The authentication provider knows how to resolve the configured user model and credential fields.

# Configurable credential field

Email is common:

```gnr
Auth::attempt({
    'email': email,
    'password': password
});
```

Applications may configure another identity field such as:

```text
username
phone
employee_number
```

The authentication API should remain credential-map based rather than hard-code `email` into the language.

# Password verification

Passwords must be verified through a dedicated password-hashing adapter.

Gungnir must not implement a home-grown password hashing algorithm.

Supported production adapters should use established password hashing algorithms such as:

```text
Argon2id
bcrypt
```

Generic fast hashes such as SHA-256 are not password-storage algorithms.

Application code should use the authentication/password API instead of handling hashes directly.

# Password hashing

A password helper may expose:

```gnr
const hash = Password::hash(password);
```

Verification:

```gnr
const valid = Password::check(
    password,
    storedHash
);
```

Authentication providers normally perform this verification internally.

Direct use is useful for password changes or specialized credential flows.

# Password rehashing

The password subsystem should be able to determine whether a stored password hash should be upgraded:

```gnr
if (Password::needsRehash(user.password)) {
    // framework/application may replace the stored hash
}
```

This allows hashing parameters to evolve without forcing an immediate password reset.

# Guards

Applications may need more than one authentication mechanism.

Examples:

```text
web
api
admin
```

The default guard may be used implicitly:

```gnr
Auth::user();
```

An explicit guard may be selected:

```gnr
Auth::guard('admin').user();
```

```gnr
Auth::guard('api').check();
```

Guards describe authentication mechanisms or contexts, not authorization roles.

# Session authentication

Browser applications commonly use session-backed authentication.

Conceptually:

```text
login
  -> verify credentials
  -> store stable identity ID in session
  -> rotate session ID

next request
  -> read identity ID from session
  -> resolve current user through provider
  -> establish request auth context
```

Only stable identity information should be stored in the session.

The current user should be resolved from the configured provider so stale roles or account state are not permanently cached in authentication state.

# Stale identities

If a session contains an identity that no longer resolves:

```text
deleted user
disabled identity
invalid provider result
mismatched identifier
```

Gungnir should treat the request as unauthenticated and clear stale authentication state safely.

# CSRF and session authentication

State-changing browser routes that rely on session/cookie authentication should use CSRF protection.

Authentication does not replace CSRF protection.

The application middleware order should ensure session state exists before authentication and CSRF logic that depends on it.

# Token authentication

API token authentication is separate from session authentication.

Conceptually:

```text
Authorization: Bearer <token>
    -> token guard/provider
    -> token verification
    -> authenticated identity
```

The request helper provides:

```gnr
const token = request.bearerToken();
```

Normal application code should not need to parse the Authorization header manually.

# Token issuance

Token creation should use an explicit token/authentication API rather than overload session login.

A future stable API may resemble:

```gnr
const token = Auth::tokens()
    .create(user, 'api');
```

Token issuance must define:

- secure token generation;
- storage strategy;
- token hashing where applicable;
- expiration;
- revocation;
- rotation;
- scopes/abilities if supported.

Until that contract is finalized, session and token authentication must remain separate concepts.

# Token revocation

Token logout/revocation is not equivalent to browser session logout.

The token system must explicitly invalidate the relevant token or token family according to its storage contract.

Gungnir should not pretend that deleting local request state revokes a bearer token.

# Authentication context

Authentication state belongs to the current request/execution context.

Conceptually:

```text
RequestContext
  authenticatedIdentity?
  guard
  authentication metadata
```

The runtime must preserve this context across coroutine suspension.

Authentication state must not leak between simultaneous requests.

# Async safety

This must remain valid:

```gnr
public async show() {
    const user = Auth::user();

    const result = await service.load(user.id);

    return json(result);
}
```

After suspension/resumption, `Auth::user()` and the captured authenticated identity must still belong to the same request.

The generated C++ runtime must not depend on unsafe thread affinity for authentication state.

# Authentication and authorization

Authentication:

```gnr
Auth::check();
Auth::user();
```

answers:

```text
Who is the user?
```

Authorization:

```gnr
Gate::allows(...);
authorize(...);
```

or policies answer:

```text
May this user perform this action?
```

Do not treat roles embedded in authentication state as a replacement for the authorization layer.

Detailed authorization and policy behavior belongs in `policy.md` / authorization documentation.

# Authentication and validation

Credential validation and credential authentication are different.

Validation:

```gnr
const credentials = request.validate({
    'email': 'required|email',
    'password': 'required|string'
});
```

Authentication:

```gnr
if (!Auth::attempt(credentials)) {
    // invalid credentials
}
```

Validation checks input structure.

Authentication verifies identity.

# Authentication and models

Models remain persistence declarations.

Do not put login methods inside the model:

```gnr
model User {
    login() {
        // ...
    }
}
```

Instead:

```gnr
Auth::attempt(credentials);
Auth::login(user);
Auth::logout();
```

# Authentication and request

Authentication may be initialized from request credentials or session state, but the Request object should not become the authentication service.

Prefer:

```gnr
Auth::user();
```

rather than:

```text
request.login(...)
request.authenticate(...)
```

The request represents HTTP input/context.

`Auth` represents authentication.

# Authentication failures

Missing authentication on a protected route should produce a defined unauthenticated result.

For API requests, conventionally:

```text
HTTP 401
```

For browser requests, the application may redirect to the configured login route.

Authentication failure and authorization denial must remain distinct:

```text
401 -> unauthenticated
403 -> authenticated but forbidden
```

# Rate limiting login attempts

Login endpoints should be compatible with rate limiting:

```gnr
Route::post('/login', LoginController::store)
    .middleware(LoginRateLimitMiddleware);
```

The authentication core should not silently implement an undocumented global rate limiter, but framework defaults may provide a standard middleware configuration.

# Complete login controller

```gnr
controller LoginController {
    public store(Request request) {
        const credentials = request.validate({
            'email': 'required|email',
            'password': 'required|string'
        });

        if (!Auth::attempt(credentials)) {
            return json({
                'message': 'Invalid credentials'
            }, 401);
        }

        return json({
            'user': Auth::user()
        });
    }

    public destroy() {
        Auth::logout();

        return response(null, 204);
    }
}
```

# Complete browser login example

```gnr
controller LoginController {
    public create() {
        if (Auth::check()) {
            return redirect('/dashboard');
        }

        return view('auth/login');
    }

    public store(Request request) {
        const credentials = request.validate({
            'email': 'required|email',
            'password': 'required|string'
        });

        if (!Auth::attempt(
            credentials,
            remember: request.boolean('remember')
        )) {
            return redirect()
                .back();
        }

        return redirect('/dashboard');
    }

    public destroy() {
        Auth::logout();

        return redirect('/login');
    }
}
```

# Authentication API reference

The intended core API includes:

| API | Purpose |
| --- | --- |
| `Auth::check()` | Is current request authenticated? |
| `Auth::guest()` | Is current request unauthenticated? |
| `Auth::user()` | Current authenticated user |
| `Auth::id()` | Current authenticated user ID |
| `Auth::attempt()` | Authenticate credentials |
| `Auth::login()` | Authenticate an existing user |
| `Auth::loginUsingId()` | Authenticate by identifier |
| `Auth::logout()` | End current session authentication |
| `Auth::guard()` | Select an authentication guard |
| `Password::hash()` | Hash a password |
| `Password::check()` | Verify a password |
| `Password::needsRehash()` | Check whether hash should be upgraded |

Token APIs should be documented separately once their issuance/revocation contract is finalized.

# Authentication AST and semantics

Authentication APIs should resolve through normal framework call-expression semantics.

For:

```gnr
const user = Auth::user();
```

the compiler should understand approximately:

```text
Auth
  -> framework authentication symbol

user()
  -> authenticated User?
```

For:

```gnr
const valid = Auth::attempt(credentials);
```

the compiler should know:

```text
receiver      Auth
operation     attempt
arguments     credential object
result        bool
side effect   establishes authentication state on success
```

The exact configured user type may be resolved from application authentication configuration.

# Semantic validation

The compiler/framework should validate where possible:

- `Auth` operations resolve to the authentication framework symbol;
- credential maps are structured values;
- guard names/configuration are valid where statically available;
- `Auth::login()` receives an authenticatable user type;
- `Auth::loginUsingId()` receives an identifier compatible with the provider;
- password helper arguments are valid string-compatible values;
- authentication context is only accessed in request-capable execution contexts where required;
- authentication APIs are not confused with authorization policy APIs.

Credential correctness remains a runtime concern.

# Compiler contract

This source:

```gnr
public store(Request request) {
    const credentials = request.validate({
        'email': 'required|email',
        'password': 'required|string'
    });

    if (!Auth::attempt(credentials)) {
        return response(null, 401);
    }

    return json(Auth::user());
}
```

should conceptually pass through:

```text
source
  -> parser
  -> validation expression AST
  -> Auth symbol resolution
  -> authentication call analysis
  -> request-context semantics
  -> validated AST
  -> authentication/runtime lowering
  -> C++23 generation
```

Before native generation, the compiler/runtime should already know:

```text
operation         Auth::attempt
credential value  validated credential object
result            bool

operation         Auth::user
result            configured authenticated user type
context           current request
```

The transpiler must not identify authentication behavior through raw source scanning.

# Generated C++ boundary

Gungnir source:

```gnr
if (Auth::check()) {
    const user = Auth::user();
}
```

may lower to request-context authentication objects, provider calls, session middleware state, and typed native user handles.

Application code should not need to manage:

```text
native auth context pointers
thread-local identities
session storage internals
credential adapter plumbing
password hashing library APIs
container lookup boilerplate
coroutine-local authentication state
```

# Naming convention

Public authentication APIs use familiar camelCase naming:

```text
loginUsingId
needsRehash
```

Core methods remain concise:

```text
check
guest
user
id
attempt
login
logout
guard
hash
```

# Design rule

The authentication contract is intentionally focused:

```text
authentication = credential verification + request-scoped identity
```

Authentication identifies the user.

Authorization decides what the user may do.

Sessions and tokens transport authentication state.

Models persist user data.

Those responsibilities remain separate.

