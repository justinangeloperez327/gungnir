# Compiler and Developer Tooling

Group 2 establishes compiler-facing infrastructure around the existing lexer, parser, AST and lowering pipeline.

The compiler surface now includes lexical symbol tables, core type inference/assignability primitives, module dependency ordering with cycle detection, token-aware source formatting, richer source diagnostics, incremental build foundations, and language-server services.

## Compiler pipeline

`.gnr -> lexer -> parser -> symbols/types -> lowering -> generated C++23`

Framework-specific lowerers remain responsible for Model, Controller, Middleware, Migration, Validation, View and async syntax while general language analysis is kept reusable.

The lexer classifies Gungnir language words such as `model`, `controller`,
`middleware`, `migration`, `async`, `await`, `inject`, and scalar field types as
keywords. The parser builds declaration AST nodes for first-class framework
declarations before the compatibility lowering path rewrites them into ordinary
C++ class declarations.

## Tooling

`gungnirc --check file.gnr` performs language validation.

`gungnirc --format file.gnr` formats Gungnir source without transpiling it. The formatter tokenizes first so braces in strings and comments are not interpreted as structural syntax.

Diagnostics can carry stable codes and hints and are rendered with source location and a source-line marker.

Framework declaration diagnostics use stable `GNR1xxx` codes. For example,
`GNR1001` reports a missing declaration name and `GNR1002` reports a missing
declaration body.

The LanguageServer service exposes compiler diagnostics and framework hover information as a foundation for a full JSON-RPC Language Server Protocol transport.

## Dependency analysis

The dependency graph topologically orders modules and rejects circular module dependencies. Module names remain dot-separated and resolve through the Group 1 module contract.

## Type analysis

The core type system recognizes null, boolean, integer, decimal and string literals and defines assignability rules, including integer-to-decimal widening and nullable/optional assignment.

This is compiler infrastructure, not a promise that every expression is fully statically typed yet. Framework-specific semantic passes can progressively consume these primitives.
