# Statements

## Overview
MethodStatement represents returns, bindings, expression statements, blocks, conditionals, loops, break and continue. Semantics checks selected scope, mutability, condition, return and loop-control rules.

`for (item in items)` accepts a `List`, `Collection`, or `Map`. Lists and collections yield elements; maps yield string keys in unspecified order. Loop bindings are immutable. Guard an optional collection against `null` before iterating it.

After a null guard, assigning to a mutable optional still replaces the optional value, so `value = null;` clears it. Numeric compound assignments and increments preserve the guard's non-null guarantee.

## Scope
The target grammar is stricter and more complete than current parsing. Full definite assignment and all-path return analysis are not guaranteed for arbitrary native escape forms.

## Structured exceptions

Structured `.gnr` projects support `try { ... } catch (Error error) { ... }`,
specific error catches, and `throw;` inside a catch. Catch variables are
immutable. Both try and catch bodies may use `await` in an async callable.
Use `throw Error("message");` for an application failure and `abort(404);` for
an HTTP failure. See [Error handling](errors.md) for reporting, rendering, debug
responses, supported catch types, and exception control flow.



- [include/gungnir/language/ast.hpp](../include/gungnir/language/ast.hpp)
- [src/language/parser.cpp](../src/language/parser.cpp)
- [src/language/semantic.cpp](../src/language/semantic.cpp)

See [Documentation](README.md) and [Getting Started](getting-started.md).
