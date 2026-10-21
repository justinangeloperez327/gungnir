# Expressions

## Overview
The structured frontend parses typed literals, calls, members, indexing, lists, objects, arrows, unary/binary operators, ternaries, `await`, safe field access and null coalescing. Arrows receive callback parameter types from migration/collection contexts and capture referenced outer values by value. Optional values can be narrowed by simple null comparisons in if/else branches.

Value captures are immutable inside an arrow, including captures of outer `let` bindings. Read their fields or create a local `let` copy when mutation is needed. A `const` binding also prevents writes through its fields and indexed elements. Injected service handles retain access to their service.

Arrows cannot implicitly capture an enclosing instance method by name, such as `() => helper()`, or capture an unqualified method reference. These uses are rejected with `GNR2208`. Call the method before creating the arrow and capture its result, or call a method on a mutable `let` local inside the arrow. Free functions, field values, and injected service handles remain available in arrows.

A null guard remains valid after an `if` only when every continuing branch preserves it. Assignments in a loop invalidate earlier guards for the assigned variable; check it in the loop condition or body before using its non-null value. Variables that the loop does not assign retain their guards. An arrow's guards apply within that arrow and do not establish guards in its caller; captured values retain the state they had when the arrow was created.

Indexing an optional `List`, `Collection`, `Map`, `Json`, or `Data` requires a null guard or a non-null fallback through `??`. This applies to reads and writes, including nested indexing when an element is itself an optional container. For example, `if (values == null) { return 0; } return values[0];` safely indexes a `List<int>?`.

## Scope
String interpolation, general native operators, safe optional method calls and general flow-sensitive narrowing are not supported. Await expressions inside null coalescing require a separate binding. Native APIs must be declared through compiler bindings. Framework compatibility expressions continue through the legacy parser.



- [Structured compiler API](../include/gungnir/language/compiler.hpp)
- [Structured compiler tests](../tests/structured_language.cpp)

- [include/gungnir/language/ast.hpp](../include/gungnir/language/ast.hpp)
- [src/language/parser.cpp](../src/language/parser.cpp)

See [Documentation](README.md) and [Getting Started](getting-started.md).
