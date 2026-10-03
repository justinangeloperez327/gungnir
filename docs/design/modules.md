# Modules

> **Development design specification.** This document describes intended long-term architecture beyond the frozen `development` contract. Examples and requirements may exceed the current implementation. See the [current implementation guide](../modules.md) before using an API.


## Development alignment

Gungnir is under active development. No 1.0 compatibility contract is frozen; this document may describe intended future architecture, but current implementation guides and executable tests remain authoritative.

Gungnir modules organize application source files and control compile-time symbol visibility.

Modules are a language/compiler feature.

They are not C++ headers, C++20 modules, namespaces, runtime service containers, or dynamic package loaders.

The module system exists to provide:

- deterministic source organization;
- explicit cross-file dependencies;
- reliable symbol resolution;
- cycle detection;
- incremental compilation;
- IDE/language-server indexing;
- scalable code generation without exposing C++ include mechanics.

# Core design rule

The module contract is:

~~~text
file path
    -> module identity
    -> imports
    -> exported top-level symbols
    -> dependency graph
    -> semantic analysis
    -> compilation order
~~~

Application developers work with Gungnir modules.

The compiler decides how those modules become C++23 translation units, generated headers, internal namespaces, or another native layout.

# Source files

Gungnir source files use the `.gnr` extension.

Example:

~~~text
app/
├── models/
│   └── user.gnr
├── controllers/
│   └── user_controller.gnr
├── services/
│   └── user_service.gnr
└── events/
    └── user_registered.gnr
~~~

Each source file has one module identity.

# Canonical module names

Module names are dot-separated.

For example:

~~~text
app.models.user
app.controllers.user_controller
app.services.user_service
app.events.user_registered
~~~

The module:

~~~text
app.models.user
~~~

maps conventionally to:

~~~text
app/models/user.gnr
~~~

under the configured Gungnir source root.

This convention already matches the compiler's module resolver and should remain the canonical rule.

# File-to-module mapping

Given source root:

~~~text
<project>/
~~~

this file:

~~~text
app/models/user.gnr
~~~

has inferred module identity:

~~~text
app.models.user
~~~

Likewise:

~~~text
app/controllers/admin/user_controller.gnr
~~~

becomes:

~~~text
app.controllers.admin.user_controller
~~~

The final `.gnr` extension is not part of the module name.

# Module declaration

A file may state its module explicitly:

~~~gnr
module app.models.user;

model User {
    table = 'users';
}
~~~

The module declaration is optional when the module can be inferred from the file path.

Therefore this is also valid in:

~~~text
app/models/user.gnr
~~~

~~~gnr
model User {
    table = 'users';
}
~~~

The compiler infers:

~~~text
app.models.user
~~~

# Explicit module validation

If a file contains an explicit module declaration, it must match the module identity resolved from the configured source root unless project configuration explicitly defines an alternate mapping.

This should be rejected:

~~~text
file:
app/models/user.gnr

source:
module app.controllers.user;
~~~

The compiler should diagnose the mismatch before lowering.

This prevents source paths and declared identities from silently diverging.

# Why module declarations are optional

Gungnir is convention-first.

For normal framework applications, repeating this in every file:

~~~gnr
module app.controllers.user_controller;
~~~

adds little value when the file path already communicates the same information.

Explicit declarations remain useful for:

- generated source;
- libraries;
- unusual source roots;
- compiler tests;
- clearer standalone examples.

# Imports

Use `import` to access symbols from another module:

~~~gnr
import app.models.user;
import app.services.user_service;

controller UserController {
    inject UserService users;

    public show(int id) {
        const user = users.find(id);

        return json(user);
    }
}
~~~

Imports are compile-time dependencies.

They do not execute code.

# Import position

Imports are top-level declarations.

Canonical file structure:

~~~gnr
module app.controllers.user_controller;

import app.models.user;
import app.services.user_service;

controller UserController {
    // ...
}
~~~

Imports should appear before ordinary application declarations.

This should not be allowed:

~~~gnr
public show() {
    import app.models.user;
}
~~~

There are no function-local dynamic imports in the initial module contract.

# Absolute module names

The first stable module system uses absolute dot-separated module names:

~~~gnr
import app.models.user;
~~~

Avoid relative path imports such as:

~~~text
import ../models/user;
import ./helpers;
~~~

Absolute module identities are easier to:

- index;
- cache;
- refactor;
- diagnose;
- resolve across platforms.

A relative import syntax may be added later only if it provides clear value.

# Import aliases

An import may use `as`:

~~~gnr
import app.services.billing as Billing;
~~~

The alias represents the imported module namespace inside the current module.

Qualified access may use:

~~~gnr
Billing::InvoiceService
~~~

or another finalized module-qualification node that lowers to the same semantic module access.

The important contract is that aliases are symbols, not text substitutions.

# Unaliased imports

An unaliased import exposes the imported module's exported top-level symbols for normal resolution:

~~~gnr
import app.models.user;

function User loadUser(int id) {
    return User::findOrFail(id);
}
~~~

If two imports expose the same name, the compiler must report ambiguity instead of silently selecting one.

# Resolving ambiguous names

Example:

~~~gnr
import app.admin.user;
import app.customers.user;
~~~

If both modules export `User`, then:

~~~gnr
const user = User::findOrFail(id);
~~~

is ambiguous.

Use aliases:

~~~gnr
import app.admin.user as AdminModels;
import app.customers.user as CustomerModels;

const admin = AdminModels::User::findOrFail(adminId);
const customer = CustomerModels::User::findOrFail(customerId);
~~~

The exact chained qualification representation may evolve internally, but ambiguity must never be resolved by import order.

# Import aliases must be unique

This should be rejected:

~~~gnr
import app.models.user as Models;
import app.models.post as Models;
~~~

An alias may not conflict with another visible module alias, local declaration, or incompatible top-level symbol.

# Framework prelude

Normal Gungnir applications should not need imports for core framework symbols such as:

~~~text
Request
Response
Next
Route
Auth
Gate
Mail
Notification
Validator
Password
Collection
List
Map
~~~

These belong to the Gungnir framework prelude.

The prelude is conceptually imported automatically into every application module.

This keeps ordinary framework source concise:

~~~gnr
controller UserController {
    public index() {
        return json(User::all());
    }
}
~~~

without:

~~~text
import gungnir.http.request;
import gungnir.http.response;
import gungnir.routing.route;
...
~~~

# Prelude discipline

The implicit prelude should remain small.

Only universally applicable language/framework symbols should be included.

Application-specific services, models, events, controllers, and third-party modules must still be imported or otherwise resolved through project conventions.

A huge implicit namespace would make name resolution unpredictable.

# Top-level declarations

A module may contain top-level declarations such as:

~~~text
model
controller
migration
middleware
policy
event
listener
notification
mail
function
~~~

Future first-class declarations such as `enum` or `interface` may also live at module scope once their language contracts are finalized.

# Export visibility

For the first stable module contract, top-level named application declarations are importable by default.

For example:

~~~gnr
model User {
    // ...
}
~~~

exports the symbol:

~~~text
User
~~~

from its module.

This avoids requiring repetitive `export` keywords for the normal one-declaration-per-file application style.

# Private top-level declarations

Explicit private/module-local top-level declarations are not required for the first stable module system.

When privacy is introduced later, it should use a Gungnir-native visibility contract rather than C++ `static`, anonymous namespaces, or preprocessor tricks.

Until then, application authors should prefer one primary public declaration per file/module and keep implementation details inside services/functions where appropriate.

# One primary declaration per file

Gungnir should **recommend**, but not necessarily require, one primary framework declaration per module file:

~~~text
app/models/user.gnr
    -> model User

app/controllers/user_controller.gnr
    -> controller UserController

app/events/user_registered.gnr
    -> event UserRegistered
~~~

This improves:

- navigation;
- naming consistency;
- project indexing;
- diagnostics;
- incremental builds.

Small helper functions may coexist where the language/module style permits it.

# Same-module visibility

Declarations within the same module can reference each other without importing the module itself.

Self-import should be diagnosed or ignored as invalid configuration:

~~~gnr
module app.models.user;
import app.models.user;
~~~

There is no reason for a module to depend on itself.

# Cross-module model usage

Example:

~~~text
app/models/user.gnr
app/controllers/user_controller.gnr
~~~

Controller:

~~~gnr
import app.models.user;

controller UserController {
    public show(User user) {
        return json(user);
    }
}
~~~

The parser produces the identifier `User`.

The module/symbol resolver binds it to:

~~~text
app.models.user::User
~~~

before semantic analysis continues.

# Cross-module services

~~~gnr
import app.services.user_service;

controller UserController {
    inject UserService users;

    public show(int id) {
        return json(users.find(id));
    }
}
~~~

Dependency injection relies on the resolved service type symbol.

The transpiler should not guess service types from spelling.

# Events and listeners

Event:

~~~text
app/events/user_registered.gnr
~~~

~~~gnr
event UserRegistered {
    User user;
}
~~~

Listener:

~~~gnr
import app.events.user_registered;

listener SendWelcomeNotification {
    public handle(UserRegistered event) {
        // ...
    }
}
~~~

The listener's event dependency becomes part of the module dependency graph.

# Model relationships

If a model relationship references another model from another module, that model should resolve through imports or a deliberately documented project-index convention.

Preferred explicit form:

~~~gnr
import app.models.post;

model User {
    posts() {
        return hasMany('posts');
    }
}
~~~

Because relationship declarations currently use resource names such as `'posts'`, semantic analysis may resolve conventional model targets through the project index.

That framework convention does **not** make every application symbol globally visible.

Normal typed references still use module resolution.

# Routes

Route files/modules may import their controllers:

~~~gnr
import app.controllers.user_controller;

Route::get('/users', UserController::index);
~~~

If Gungnir adopts a conventional route bootstrap file, compiler project indexing may load it as an entry module.

Routing should not depend on C++ header include order.

# Entry modules

A Gungnir application may define one or more entry modules.

Examples:

~~~text
routes.web
routes.api
console
migrations
tests
~~~

The build/application configuration determines which entry modules participate in a target.

The module dependency graph expands from those entry points.

# Migrations

Migrations may be compiled for the migration CLI target without being linked into the web application target.

Module identity and dependency resolution remain the same.

This allows:

~~~text
web application target
migration target
test target
CLI target
~~~

to share language/module rules while producing different native binaries.

# Static dependency graph

All imports are statically discoverable.

Given:

~~~text
A imports B
B imports C
~~~

the dependency graph is:

~~~text
A
└── B
    └── C
~~~

The compiler can therefore determine compilation order before executing application code.

# Compilation order

Dependencies are analyzed before dependents.

For:

~~~text
app.controllers.user_controller
    imports app.services.user_service

app.services.user_service
    imports app.models.user
~~~

the dependency order is conceptually:

~~~text
app.models.user
app.services.user_service
app.controllers.user_controller
~~~

The exact native build strategy may differ, but semantic indexing must respect dependency relationships.

# Circular dependencies

Circular module imports are rejected.

Example:

~~~text
A imports B
B imports A
~~~

or:

~~~text
A -> B -> C -> A
~~~

must produce a compiler diagnostic.

Gungnir already has dependency-graph cycle detection; this behavior should remain part of the language contract.

# Why cycles are rejected

Rejecting cycles provides:

- deterministic analysis;
- simpler symbol initialization;
- easier incremental compilation;
- clearer architecture;
- simpler generated C++ boundaries;
- better diagnostics.

If two modules require each other, the application design should usually extract the shared contract/value into a third module.

# Breaking a cycle

Instead of:

~~~text
orders -> payments
payments -> orders
~~~

prefer:

~~~text
orders  -> billing_contracts
payments -> billing_contracts
~~~

or another dependency direction that reflects the actual architecture.

Future interfaces may provide another clean boundary once `interface` is finalized.

# Duplicate module names

Two different source files may not resolve to the same module identity in the same compilation target.

The compiler should report both paths.

Case-only differences should be treated carefully for cross-platform compatibility.

# Case sensitivity

Canonical module names should be case-sensitive at the language level but project tooling should strongly recommend lowercase path/module segments:

~~~text
app.models.user
app.controllers.user_controller
~~~

not:

~~~text
App.Models.User
~~~

This avoids differences between case-sensitive and case-insensitive filesystems.

# Naming convention

Recommended module segments use lowercase snake_case:

~~~text
app.models.user
app.controllers.user_controller
app.services.payment_service
~~~

Type declarations remain PascalCase:

~~~text
User
UserController
PaymentService
~~~

Functions and values remain camelCase.

# Module grammar

Conceptually:

~~~text
moduleDeclaration
  := 'module' moduleName ';'

importDeclaration
  := 'import' moduleName ('as' Identifier)? ';'

moduleName
  := Identifier ('.' Identifier)*
~~~

Module/import declarations occur only at top level.

# File grammar

Conceptually:

~~~text
sourceFile
  := moduleDeclaration?
     importDeclaration*
     topLevelDeclaration*
     EOF
~~~

Imports should precede ordinary declarations.

# Module AST

~~~text
ModuleUnit
  moduleName
  sourcePath
  explicitModule
  imports[]
  declarations[]
~~~

Import:

~~~text
ImportDeclaration
  moduleName
  alias?
  sourceSpan
  resolvedModule?
~~~

# Symbol table

Each module owns a symbol table containing its top-level declarations.

Conceptually:

~~~text
ModuleSymbolTable
  module
  exports
    User
    helperFunction
    ...
  imports
    app.services.user_service
~~~

The project index combines module tables without flattening them into one accidental global namespace.

# Name resolution order

A simple initial resolution order should be:

1. local lexical symbols;
2. parameters;
3. members/injected dependencies where relevant;
4. current-module top-level declarations;
5. explicitly imported symbols/modules;
6. framework prelude;
7. explicit native interoperability bindings.

Ambiguous matches are errors.

Import order must not decide which symbol wins.

# Wildcard imports

Wildcard imports are not part of the initial contract.

Avoid:

~~~text
import app.models.*;
~~~

They make dependencies and name collisions less clear.

Explicit module imports are preferred.

# Selective symbol imports

A dedicated syntax such as:

~~~text
import User from app.models.user;
~~~

is not required initially because application modules commonly expose one primary declaration.

If selective imports become useful later, they should be added without changing the meaning of existing module imports.

# Re-exports

Module re-export syntax is not required for the first stable contract.

Dependency aggregation should remain explicit until there is a clear library/package use case.

# Dynamic imports

Dynamic imports do not exist:

~~~text
import(moduleNameAtRuntime)
~~~

Module loading is compile-time/static.

Runtime plugin loading is a separate subsystem if Gungnir ever supports it.

# Imports are not dependency injection

This:

~~~gnr
import app.services.user_service;
~~~

makes the symbol known to the compiler.

This:

~~~gnr
inject UserService users;
~~~

asks the application container for an instance.

They solve different problems.

# Imports are not runtime initialization

Importing a module must not execute arbitrary module-level application code.

Gungnir should avoid implicit global initialization side effects.

Application startup belongs to explicit application/bootstrap lifecycle hooks.

# No header/include model

Normal Gungnir code does not write:

~~~text
#include "user.hpp"
#include <vector>
#pragma once
~~~

Module dependencies are semantic imports.

The compiler decides which native declarations/includes/generated units are necessary.

# No namespace boilerplate

Application developers should not need:

~~~text
namespace app::models {
    ...
}
~~~

to prevent native symbol collisions.

Module identity provides the source-level namespace boundary.

Generated C++ may use namespaces or mangled/internal names.

# C++23 module boundary

Gungnir modules are not required to lower one-to-one to C++20/23 modules.

The compiler may use:

- one generated application translation unit;
- multiple generated translation units;
- generated headers;
- internal namespaces;
- native C++ modules in the future.

That decision is an optimization/build architecture detail.

The Gungnir module contract should remain stable across those implementation strategies.

# Incremental compilation

Because module dependencies are explicit, the compiler can track affected dependents.

Conceptually:

~~~text
user.gnr changes
    -> rebuild/recheck app.models.user
    -> recheck modules that depend on app.models.user
    -> leave unrelated modules cached
~~~

The existing incremental build cache is a foundation, but robust incremental compilation should use stable content/interface hashes rather than rely only on process-local state.

# Public interface fingerprints

A future optimized compiler should distinguish:

~~~text
implementation changed
public module interface unchanged
~~~

from:

~~~text
exported symbol/signature changed
~~~

so unrelated dependents do not require unnecessary recompilation.

# Project index

Before full semantic analysis, the compiler should build a project index containing at least:

~~~text
module names
source paths
imports
top-level declaration names
framework declaration kinds
function signatures where available
source spans
~~~

This allows cross-file references such as:

- controller actions;
- model relationships;
- policies;
- event/listener bindings;
- dependency injection types;
- routes;

to resolve without raw source searching.

# Module and framework discovery

Framework declarations should be indexed by semantic identity.

Example:

~~~text
app.models.user::User
  kind = model

app.controllers.user_controller::UserController
  kind = controller
~~~

The compiler should not identify declarations through filename heuristics alone.

File conventions help resolution, but the parsed declaration remains authoritative.

# Module and package boundary

A module is a source unit.

A package/library is a larger distribution concept containing multiple modules.

Do not overload the word "module" to mean an installable third-party package.

Package/dependency management should be documented separately if introduced.

# Third-party modules

Third-party Gungnir libraries may eventually expose module roots such as:

~~~text
vendor.payments.stripe
vendor.audit
~~~

or package-defined aliases.

Resolution must come from package/build configuration, not arbitrary filesystem traversal from source code.

# Standard modules

Framework services should generally come from the prelude or stable `gungnir.*` modules when explicit import is desirable.

Application code should not depend on generated internal module names.

# Tests

Test modules should use the same import semantics as production code.

A test may import:

~~~gnr
import app.services.user_service;
~~~

Test-only symbols should not become visible to production targets merely because they exist in the project tree.

Compilation targets determine which roots/modules are included.

# Diagnostics

Module diagnostics should include stable errors for cases such as:

~~~text
module not found
explicit module does not match source path
duplicate module identity
duplicate alias
ambiguous imported symbol
circular dependency
self import
invalid module segment
import after application declarations
unknown qualified module alias
~~~

Cycle diagnostics should show the dependency chain when possible:

~~~text
app.services.orders
  -> app.services.payments
  -> app.services.orders
~~~

# Semantic validation

The module phase should validate at least:

- inferred module names are valid;
- explicit declarations match resolved identities;
- imported modules exist;
- aliases are unique;
- imports occur at module scope;
- self imports are rejected;
- dependency cycles are rejected;
- exported top-level names are unique within a module;
- unqualified imported names are not ambiguous;
- qualified alias references resolve;
- cross-module type/function references obey visibility rules;
- entry-target module sets are valid;
- module resolution cannot escape configured source/package roots.

# Compiler pipeline

Modules should be handled before deep body semantics:

~~~text
project source discovery
  -> parse module/import headers
  -> build module index
  -> resolve imports
  -> build dependency graph
  -> detect cycles
  -> index top-level declarations
  -> resolve cross-module symbols
  -> semantic/type analysis
  -> validated project AST
  -> lowering/code generation
~~~

This allows the compiler to understand the application as a project rather than isolated files.

# Compiler contract

Given:

~~~text
app/models/user.gnr
app/services/user_service.gnr
app/controllers/user_controller.gnr
~~~

and:

~~~gnr
// app/services/user_service.gnr
import app.models.user;

function User findUser(int id) {
    return User::findOrFail(id);
}
~~~

~~~gnr
// app/controllers/user_controller.gnr
import app.services.user_service;

controller UserController {
    public show(int id) {
        return json(findUser(id));
    }
}
~~~

the compiler should resolve:

~~~text
app.controllers.user_controller
    -> app.services.user_service
        -> app.models.user
~~~

before C++ generation.

# Generated C++ boundary

A Gungnir module may lower to native code using internal namespaces such as:

~~~text
generated::app::models::user
~~~

or generated translation-unit boundaries.

That representation is private compiler output.

Application source should never need to mirror generated native namespaces.

# Current implementation boundary

The compiler already contains foundations for:

- dot-separated module names;
- module-to-file resolution;
- optional import aliases;
- dependency graph ordering;
- circular dependency detection;
- incremental content-change tracking.

This document defines the fuller application-facing and semantic contract that those foundations should grow into.

# Canonical examples

Convention-only module:

~~~text
file: app/models/user.gnr
module identity: app.models.user
~~~

~~~gnr
model User {
    table = 'users';
}
~~~

Explicit module:

~~~gnr
module app.models.user;

model User {
    table = 'users';
}
~~~

Import:

~~~gnr
import app.models.user;

function User requireUser(int id) {
    return User::findOrFail(id);
}
~~~

Alias:

~~~gnr
import app.services.billing as Billing;

function void charge(Order order) {
    Billing::PaymentService::charge(order);
}
~~~

# Design rule

The module system is intentionally static and predictable:

~~~text
module = source identity + exported symbols + explicit dependencies
~~~

File conventions provide the identity.

Imports provide dependencies.

The project index resolves symbols.

The dependency graph determines analysis order.

C++ headers, namespaces, translation units, and native modules remain compiler implementation details.

