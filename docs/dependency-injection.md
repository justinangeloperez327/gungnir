# Inversion of Control and Dependency Injection

Gungnir's container supports transient, singleton, instance, and scoped service lifetimes.

Transient bindings create a value for each resolution. Singleton bindings create once and reuse the instance. Scoped bindings reuse an instance only inside an explicit request or operation scope. Resolving a scoped binding outside a scope is rejected instead of silently changing its lifetime.

Interfaces can be bound to implementations or factories. Aliases can forward one service contract to another registered target. Unregistered concrete types remain auto-constructible when they accept `Container&` or are default constructible.

The container detects recursive resolution and reports circular dependency graphs instead of recursing indefinitely.

Bindings can be replaced, and `forget<Service>()` removes a binding. This is the basis for test doubles and application-specific overrides.

A scope is controlled through `begin_scope()` and `end_scope()`. HTTP request integration should own these calls so application controllers do not manage scopes manually.
