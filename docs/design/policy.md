# Policies

> **Gungnir 1.x design specification.** This document describes intended long-term architecture beyond the frozen `1.0.0-rc.1` contract. Examples and requirements may exceed the current implementation. See the [current implementation guide](../policy.md) before using an API.


## 1.0 RC alignment

The shipped baseline is `1.0.0-rc.1`. The 1.0 contracts are frozen; this design document may describe additive 1.x evolution or future-major work, but current implementation guides and executable tests remain authoritative.

A Gungnir policy defines authorization rules for a resource or application concept.

Authentication answers who the current user is.

A policy answers whether that authenticated user may perform a specific action.

Policies are first-class Gungnir declarations and should contain authorization logic only.

## Basic policy

```gnr
policy PostPolicy {
    public view(User user, Post post) {
        return true;
    }

    public update(User user, Post post) {
        return user.id == post.user_id;
    }

    public delete(User user, Post post) {
        return user.id == post.user_id;
    }
}
```

Policy actions return an authorization decision.

For simple rules, a boolean is sufficient.

## Policy responsibility

A policy answers questions such as:

```text
May this user view this resource?
May this user create this resource?
May this user update this resource?
May this user delete this resource?
May this user restore this resource?
May this user permanently delete this resource?
```

Policies should not perform the operation itself.

## Conventional policy actions

Gungnir follows familiar resource-policy names:

```text
viewAny
view
create
update
delete
restore
forceDelete
```

Example:

```gnr
policy PostPolicy {
    public viewAny(User user) {
        return true;
    }

    public view(User user, Post post) {
        return post.published || user.id == post.user_id;
    }

    public create(User user) {
        return Auth::check();
    }

    public update(User user, Post post) {
        return user.id == post.user_id;
    }

    public delete(User user, Post post) {
        return user.id == post.user_id;
    }

    public restore(User user, Post post) {
        return user.id == post.user_id;
    }

    public forceDelete(User user, Post post) {
        return user.isAdmin;
    }
}
```

These are conventions, not reserved action names. Applications may define domain-specific abilities when required.

## Custom abilities

```gnr
policy OrderPolicy {
    public approve(User user, Order order) {
        return user.isManager;
    }

    public refund(User user, Order order) {
        return user.isFinance;
    }
}
```

## Authorization checks

A controller may check an ability:

```gnr
if (!Gate::allows('update', post)) {
    return response(null, 403);
}
```

Prefer enforcement when denial should stop normal execution:

```gnr
authorize('update', post);
```

Conceptually, Gungnir resolves:

```text
post
  -> Post
  -> PostPolicy
  -> update(user, post)
```

## Denial

Authorization must fail closed.

If a policy, ability, or required authenticated identity cannot be resolved, access is denied rather than granted implicitly.

## Explicit decisions

Policies may return a structured decision when a denial reason is useful:

```gnr
public update(User user, Post post) {
    if (user.id != post.user_id) {
        return deny('You do not own this post.');
    }

    return allow();
}
```

Both:

```text
bool
Decision
```

may be accepted as policy-action results.

A boolean false becomes a normal denial.

A `Decision` may carry a reason or application-specific denial metadata.

## Before hook

A policy may define one optional global pre-check:

```gnr
policy PostPolicy {
    public before(User user) {
        if (user.isAdmin) {
            return allow();
        }

        return null;
    }

    public update(User user, Post post) {
        return user.id == post.user_id;
    }
}
```

Returning an explicit allow or deny completes the authorization decision.

Returning `null` continues to the requested policy action.

The `before` hook should be used sparingly because it affects every ability in the policy.

## Policy registration

Where naming conventions are unambiguous:

```text
Post -> PostPolicy
Order -> OrderPolicy
User -> UserPolicy
```

Gungnir should resolve the policy automatically.

Explicit registration may exist for non-conventional mappings:

```text
Article -> ContentPolicy
```

The application/bootstrap documentation should own explicit registration syntax.

## Dependency injection

Policies may depend on authorization-oriented services:

```gnr
policy DocumentPolicy {
    inject PermissionService permissions;

    public update(User user, Document document) {
        return permissions.allows(
            user,
            'documents.update',
            document
        );
    }
}
```

Policies should contain only:

```text
inject declarations
public policy actions
optional public before(...)
```

They should not become general-purpose service classes.

## Authentication requirement

Most policy actions receive the authenticated user explicitly:

```gnr
public update(User user, Post post) {
    // ...
}
```

The framework supplies the current authenticated user during authorization.

If the ability allows guests, the user parameter may use an optional form once optional model typing is finalized:

```gnr
public view(User? user, Post post) {
    return post.published;
}
```

## Policies and controllers

Controllers coordinate the HTTP operation:

```gnr
public update(Request request, Post post) {
    authorize('update', post);

    const data = request.validate({
        'title': 'required|string'
    });

    post.update(data);

    return json(post);
}
```

The controller does not need to duplicate policy logic.

## Policies and middleware

Broad route-level authorization may use middleware, but resource-specific decisions belong naturally in policies.

Do not put detailed ownership rules into large generic middleware classes when the rule belongs to a resource.

## Policies and models

Models remain persistence metadata plus relationships.

Do not place authorization methods inside a model:

```gnr
model Post {
    canUpdate(User user) {
        // invalid model responsibility
    }
}
```

Use `PostPolicy`.

## Policies and validation

Validation asks whether input is valid.

Policy authorization asks whether the actor may perform an operation.

The two checks are independent.

## What does not belong in a policy

Policies should not:

- mutate database records as part of authorization;
- send mail or notifications;
- dispatch business events;
- perform HTTP redirects;
- generate views;
- run large workflows;
- hide persistence side effects behind permission checks.

A policy decision should remain predictable and side-effect-light.

## Policy grammar

Conceptually:

```text
policyDeclaration
  := 'policy' Identifier '{'
       policyMember*
     '}'

policyMember
  := injectDeclaration
   | policyAction

policyAction
  := 'public' Identifier '(' parameterList? ')' block
```

Policy actions have an implicit authorization-result contract.

## Policy AST

```text
PolicyDeclaration
  name
  injections[]
  actions[]
  before?
```

Policy action:

```text
PolicyAction
  name
  parameters[]
  body
  resultContract = AuthorizationDecision
```

## Semantic validation

The compiler should validate:

- policy names are unique;
- injection types resolve;
- action names are unique;
- policy parameter types resolve;
- resource parameters resolve to known application/model types;
- policy returns are boolean- or Decision-compatible;
- `before` has a compatible signature;
- referenced policies and abilities resolve where project information is available;
- arbitrary fields, constructors, and destructors are rejected.

## Compiler contract

```gnr
policy PostPolicy {
    public update(User user, Post post) {
        return user.id == post.user_id;
    }
}
```

should become:

```text
source
  -> parser
  -> PolicyDeclaration AST
  -> action/type resolution
  -> authorization semantic validation
  -> validated policy AST
  -> policy registration/lowering
  -> C++23 generation
```

The transpiler must not discover policies or abilities by rescanning raw source.

## Design rule

```text
policy = authorization decisions for a resource or ability
```

Authentication identifies the user.

Policies decide what that user may do.

Controllers perform the authorized operation.

