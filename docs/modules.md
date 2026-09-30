# Modules

> **Status: experimental, pre-1.0.** This page describes the current implementation. Native C++ APIs and `.gnr` syntax are identified separately. Proposed contracts are in the [design specification](design/modules.md).

## Current behavior

ModuleResolver maps dotted names to .gnr paths; DependencyGraph and IncrementalBuildCache expose compiler helper APIs. Project builds index declarations across discovered sources.

## Limits and planned work

These helpers do not establish parsed module/import declarations, alias visibility, export rules or incremental native emission. ModuleUnit/ImportDeclaration are not current Program variants.

## Implementation references

- [include/gungnir/language/module.hpp](../include/gungnir/language/module.hpp)
- [include/gungnir/language/dependency_graph.hpp](../include/gungnir/language/dependency_graph.hpp)
- [src/cli/project.cpp](../src/cli/project.cpp)

See the [documentation index](README.md), [getting started](getting-started.md), and [target design](design/modules.md).
