# CLI and Code Generation

> **Design specification.** This document preserves the intended contract. Examples and requirements below may exceed the current implementation. See the [current implementation guide](../cli-codegen.md) before using an API.

Gungnir's CLI is project-aware and should generate canonical .gnr source rather than exposing native C++ plumbing.

# Project commands

Core project commands include:

~~~text
gungnir new <name> [path]
gungnir build [--release]
gungnir run [--release]
gungnir dev
~~~

Project discovery may walk upward until the Gungnir project marker/configuration is found.

# Compiler commands

Compiler-facing commands should include:

~~~text
gungnirc --check <file/project>
gungnirc --format <file/project>
gungnirc --dump-ast
gungnirc --dump-symbols
gungnirc --dump-types
gungnirc --dump-semantic
gungnirc --dump-validated-ast
gungnirc --dump-cpp-ir
gungnirc --emit-cpp
~~~

The exact command spelling may evolve, but compiler phases should be inspectable independently.

# Generators

Generators should exist only for source constructs with a defined language contract and working lowering/runtime support.

Canonical generator families may include:

~~~text
make:model
make:controller
make:middleware
make:migration
make:policy
make:event
make:listener
make:notification
make:mail
~~~

Additional generators should be enabled only when their source-language contracts exist.

# Generated syntax

Generators must follow the canonical docs:

~~~text
model.md
controller.md
middleware.md
migration.md
policy.md
event.md
listener.md
notification.md
mail.md
grammar.md
~~~

A generator must never become an alternate parser/language definition.

# Safety

Generation must not overwrite an existing application file silently.

Default behavior should fail with a clear message.

A future explicit force/replace mode may exist, but it must be opt-in.

# Naming

Generators normalize names deterministically.

Example intent:

~~~text
UserController
  -> app/controllers/UserController.gnr

User
  -> app/models/User.gnr
~~~

Migration source remains descriptive snake_case:

~~~text
CreateUsersTable
  -> database/migrations/create_users_table.gnr
~~~

Declaration-based filenames should match their generated PascalCase type. Exact path conventions should follow the module contract.

# Module generation

Generated files should naturally map to their inferred module names.

An explicit module declaration is optional when path-based module identity is sufficient.

# Build

gungnir build should conceptually perform:

~~~text
project discovery
  -> source discovery
  -> parse
  -> semantics
  -> Validated AST
  -> C++ lowering/emission
  -> native C++ build
~~~

A failed semantic check should stop before native compilation.

# Development command

gungnir dev should only claim features that are actually implemented.

Hot reload/file watching should not be documented as reliable until the development runtime can:

- detect changes;
- rebuild;
- restart safely;
- report failures;
- avoid orphan processes.

# Generated C++ inspection

Developers should be able to retain/inspect generated C++ without making that generated code part of the normal source workflow.

Generated files are build artifacts.

# Database commands

Migration commands may include:

~~~text
migrate
rollback
reset
status
plan
~~~

They should operate through the canonical migration runtime and backend capability contracts.

# Determinism

Generators and compiler emission should be deterministic.

The same inputs/configuration should generate the same source/output ordering.

# Design rule

~~~text
CLI generates canonical Gungnir
compiler validates Gungnir
transpiler generates C++23
native toolchain builds the result
~~~

