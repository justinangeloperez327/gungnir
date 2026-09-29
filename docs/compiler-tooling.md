# Compiler and Developer Tooling

Group 2 establishes compiler-facing infrastructure around the existing lexer, parser, AST and lowering pipeline.

The compiler surface now includes lexical symbol tables, core type inference/assignability primitives, module dependency ordering with cycle detection, token-aware source formatting, richer source diagnostics, incremental build foundations, and language-server services.

## Compiler pipeline

`.gnr -> lexer -> parser -> symbols/types -> lowering -> generated C++23`

Framework-specific lowerers remain responsible for Model, Controller, Middleware, Migration, Validation, View and async syntax while general language analysis is kept reusable.

The lexer classifies Gungnir declaration words such as `model`, `controller`,
`middleware`, `migration`, `policy`, `event`, `listener`, `notification`,
and `mail` as language keywords alongside `async`, `await`, `inject`, and
scalar field types. The parser builds semantic `FrameworkDeclaration` nodes for
all of these first-class artifacts before lowering them into ordinary C++ class
declarations.

Group 55 extends that frontend with parser-owned member nodes for model fields,
model configuration, controller methods, injected dependencies, and model
relationships. Model lowering consumes `ModelField`, `ModelConfiguration`, and
`ModelRelationship` nodes, including relationship kind, related/through model
types, key overrides, and source spans. Controller dependency lowering consumes
`InjectDeclaration` nodes. Route declarations are also parser-owned through
`RouteDeclaration`, including HTTP method, controller action, and optional
middleware metadata. Middleware, migration, policy, event, listener,
notification, and mail bodies also expose parser-owned `FrameworkMethod` nodes,
so their method signatures are available to later semantic passes without
re-scanning source text. Async expression lowering deliberately remains on the
existing token-aware compatibility pass until coroutine semantics can move into
the frontend without making generated C++ less inspectable.

## Tooling

`gungnirc --check file.gnr` performs language validation.

`gungnirc --format file.gnr` formats Gungnir source without transpiling it. The formatter tokenizes first so braces in strings and comments are not interpreted as structural syntax.

Diagnostics can carry stable codes and hints and are rendered with source location and a source-line marker.

Framework declaration diagnostics use stable `GNR1xxx` codes. For example,
`GNR1001` reports a missing declaration name and `GNR1002` reports a missing
declaration body. Model member diagnostics use `GNR11xx`, including `GNR1120`-
`GNR1124` for semantic relationship declarations, controller injection
diagnostics use `GNR12xx`, and route diagnostics use `GNR13xx`.

The LanguageServer service exposes compiler diagnostics and framework hover information as a foundation for a full JSON-RPC Language Server Protocol transport.

## Dependency analysis

The dependency graph topologically orders modules and rejects circular module dependencies. Module names remain dot-separated and resolve through the Group 1 module contract.

## Type analysis

The core type system recognizes null, boolean, integer, decimal and string literals and defines assignability rules, including integer-to-decimal widening and nullable/optional assignment.

This is compiler infrastructure, not a promise that every expression is fully statically typed yet. Framework-specific semantic passes can progressively consume these primitives.
