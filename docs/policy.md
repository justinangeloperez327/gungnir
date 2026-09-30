# Policies and Authorization

> **Status: experimental, pre-1.0.** This page describes the current implementation. Native C++ APIs and `.gnr` syntax are identified separately. Proposed contracts are in the [design specification](design/policy.md).

## Current behavior

Native `Authorization` registers named callbacks taking `Identity` and returning `Decision`. It exposes `inspect`, `allows`, `denies` and a `before` callback. Unknown abilities are denied.

`auth::authorize(authorization, context, ability)` throws when identity is missing or the decision denies access. The native `Policy` framework base is currently an empty polymorphic base.

## Example

```cpp
gungnir::auth::Authorization authorization;
authorization.define("admin", [](const gungnir::auth::Identity& user) {
    return user.role("admin")
        ? gungnir::auth::Decision::allow()
        : gungnir::auth::Decision::deny("Admin role required");
});
```

## Limits and planned work

A `.gnr` `policy` declaration does not automatically supply complete model-specific policy registration, resource arguments or the target implicit ability return contract. Register working authorization callbacks explicitly.

## Implementation references

- [include/gungnir/auth/authorization.hpp](../include/gungnir/auth/authorization.hpp)
- [include/gungnir/auth/authorize.hpp](../include/gungnir/auth/authorize.hpp)
- [include/gungnir/core/framework_artifacts.hpp](../include/gungnir/core/framework_artifacts.hpp)

See the [documentation index](README.md), [getting started](getting-started.md), and [target design](design/policy.md).
