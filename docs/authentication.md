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

        if (!auth.attempt(request, credentials, request.input("remember"))) {
            return redirect("/login");
        }

        return redirect("/dashboard");
    }
}
```

Successful session authentication rotates the session identifier.

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

Identity providers define the public attributes and resolve identities from user models. Password hashes remain in credential providers. JSON serialization includes the identity identifier, sorted roles, and public attributes. Snapshots remain valid after the request finishes; they do not modify the request's authentication context. Policies resolve the identity to their declared actor model through the configured actor resolver.

## Logout

Logout clears authenticated state, rotates or invalidates the session as appropriate, and revokes remember-me credentials associated with the session.

## Passwords

Gungnir provides versioned password hashing and verification. Applications store password hashes rather than plaintext credentials.

## Remember me

Session guards can issue remember tokens. Tokens are stored as digests, rotated when consumed, and revoked on logout. Production applications should use a persistent remember-token store.

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
