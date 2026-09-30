# Modules

> **Status: experimental, pre-1.0.** This page describes the current implementation. Native C++ APIs and `.gnr` syntax are identified separately. Proposed contracts are in the [design specification](design/modules.md).

## Current behavior

`gungnirc ROOT --project` compiles `.gnr` modules together. A dotted module name maps to its path relative to ROOT; an explicit `module` declaration must match. Imports may use aliases, for example `import billing as Billing;` followed by `Billing::invoice()`. Top-level declarations are exported by default; `export` is accepted explicitly. Missing imports, cycles, duplicate aliases/declarations and ambiguous imported names are diagnosed. Dependency order and file order are deterministic.

## Limits and planned work

This is a structured module compilation profile, not the native application bootstrap/build workflow. Project roots should contain structured modules; compatibility sources/routes require the existing build path. Incremental native compilation is not provided.

## Implementation references

- [Structured compiler API](../include/gungnir/language/compiler.hpp)
- [Structured compiler tests](../tests/structured_language.cpp)

- [include/gungnir/language/module.hpp](../include/gungnir/language/module.hpp)
- [include/gungnir/language/dependency_graph.hpp](../include/gungnir/language/dependency_graph.hpp)
- [src/cli/project.cpp](../src/cli/project.cpp)

See the [documentation index](README.md), [getting started](getting-started.md), and [target design](design/modules.md).
