# Errors

> **Status: experimental, pre-1.0.** This page describes the current implementation. Native C++ APIs and `.gnr` syntax are identified separately. Proposed contracts are in the [design specification](design/errors.md).

## Current behavior

Compiler diagnostics carry source location, level, message and code; gungnirc renders them and exits nonzero on errors. Native errors and exceptions cover HTTP, validation, ORM, database, view and storage failures. ExceptionHandler defines HTTP rendering at the router boundary.

## Limits and planned work

Semantic checking is partial and no dedicated Validated AST gate exists. Native compilation can still report errors not diagnosed by Gungnir. Exception rendering must be configured for production; complete target error syntax and cross-backend error normalization remain separate work.

## Implementation references

- [include/gungnir/language/diagnostic.hpp](../include/gungnir/language/diagnostic.hpp)
- [include/gungnir/language/diagnostic_renderer.hpp](../include/gungnir/language/diagnostic_renderer.hpp)
- [include/gungnir/http/exception_handler.hpp](../include/gungnir/http/exception_handler.hpp)
- [include/gungnir/errors/error.hpp](../include/gungnir/errors/error.hpp)

See the [documentation index](README.md), [getting started](getting-started.md), and [target design](design/errors.md).
