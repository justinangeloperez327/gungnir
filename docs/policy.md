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

## Decisions

`allow()` grants the ability. `deny(message)` rejects it and may carry a user-facing or diagnostic reason.

## HTTP behavior

Unauthenticated access and authenticated-but-denied access remain distinct conditions. Applications can customize how those authorization failures are represented to clients.

## Registration

Policies are registered during application bootstrap. The framework uses typed actor/resource contracts so invalid policy signatures can be rejected during compilation.
