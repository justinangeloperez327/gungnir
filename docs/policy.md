# Policies and Authorization

Policies organize authorization rules around application models and actions.

Authentication answers **who is making the request**. Authorization answers **whether that actor may perform an operation on a resource**.

## Defining a policy

```gnr
policy ProjectPolicy {
    view(User actor, Project project) {
        return actor.id == project.owner_id
            ? allow()
            : deny("You cannot view this project.");
    }

    update(User actor, Project project) {
        return actor.id == project.owner_id
            ? allow()
            : deny("You cannot update this project.");
    }
}
```

Policy abilities receive a typed actor and resource and return an authorization decision.

## Authorizing a request

```gnr
controller ProjectController {
    show(Request request, Project project) {
        authorize(request, "view", project);
        return json(project);
    }
}
```

Authorization resolves the authenticated actor and the policy registered for the resource type.
The route binds `project` using the model's primary key before calling the action.
Missing or malformed resources return HTTP 404.

## Decisions

`allow()` grants the ability. `deny(message)` rejects it and may carry a user-facing or diagnostic reason.

## HTTP behavior

Unauthenticated access and authenticated-but-denied access remain distinct conditions. Applications can customize how those authorization failures are represented to clients.
Default statuses are HTTP 401 for unauthenticated requests and HTTP 403 for
denied abilities, missing policies, unresolved actors, or ambiguous actor bindings.

## Registration

Policies are registered during application bootstrap. The framework uses typed actor/resource contracts so invalid policy signatures can be rejected during compilation.

Generated applications register policies automatically. A model actor such as
`User` is looked up by `AuthIdentity.id` through the application's ORM connection.
An identity whose account has disappeared cannot satisfy the policy.
`authorize` requires a nonoptional model resource or an explicitly registered
native resource type; scalars, service handles, and optional models are rejected.

When identities use external identifiers, register an explicit mapping in
`bootstrap::configure`. Explicit mappings take precedence over default lookup:

```cpp
app.on_boot([](gungnir::Application& app) {
    app.container().resolve<gungnir::auth::ResourceAuthorization>()->actor<User>(
        [](const gungnir::auth::Identity& identity) {
            return User::query().where("external_id", identity.id).first();
        });
});
```

Configure mappings and policies before concurrent requests. Injected policy
dependencies resolve during registration and must be application services.
