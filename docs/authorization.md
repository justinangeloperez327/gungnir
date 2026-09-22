# Authorization

Authorization answers whether an authenticated identity may perform an ability. It is separate from authentication.

```cpp
auth::Authorization authorization;

authorization.define("posts.update", [](const auth::Identity& user) {
    return user.role("editor")
        ? auth::Decision::allow()
        : auth::Decision::deny("Editor role required");
});
```

## Decisions

Policies return an explicit `Decision`, allowing denial reasons to be preserved without coupling policy logic to HTTP responses.

Use `inspect()` when the denial reason matters, `allows()` for a boolean check, and `denies()` for its inverse.

## Enforcement

```cpp
auth::authorize(authorization, context, "posts.update");
```

Enforcement requires an authenticated request context. Missing identity and denied abilities raise `AuthorizationError`; the HTTP exception layer can translate that into the application's chosen response.

## Default deny

Undefined abilities are denied. Authorization must fail closed rather than silently grant access.

## Global override

`before()` can grant a global override such as a platform administrator. A negative result from the override does not automatically deny the ability; the named policy is still evaluated.

## Resource policies

This group establishes the stable ability/decision contract. Model-specific policy dispatch should be generated or registered explicitly rather than relying on runtime type-name magic.
