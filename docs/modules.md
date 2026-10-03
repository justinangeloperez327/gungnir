# Modules

## Overview
`gungnirc ROOT --project` compiles `.gnr` modules together. A dotted module name maps to its path relative to ROOT; an explicit `module` declaration must match. Imports may use aliases, for example `import billing as Billing;` followed by `Billing::invoice()`. Top-level declarations are exported by default; `export` is accepted explicitly. Missing imports, cycles, duplicate aliases/declarations and ambiguous imported names are diagnosed. Dependency order and file order are deterministic.

## Application builds and incremental output

New `gungnir` projects use the same validated module graph for application builds. All imports are checked on every assembly, including transitive dependencies and deleted modules. `CppEmitter::emit_units` emits a shared declaration header and one implementation file per module. Files are rewritten only when their content changes. CMake tracks the shared interface, native bootstrap headers, and implementation dependencies across CLI invocations: body-only edits usually rebuild one module; interface changes conservatively rebuild dependents through the shared header. Removed modules are removed from the build's source list. Failed validation leaves previously generated output intact.

This is incremental native compilation, not a persisted parser/semantic cache. Changes that alter generated symbol identities may also regenerate other implementation files. Standalone `gungnirc ROOT --project` still expects only structured modules. Application route/bootstrap handling belongs to `gungnir build`; see [CLI and code generation](cli-codegen.md).



- [Structured compiler API](../include/gungnir/language/compiler.hpp)
- [Structured compiler tests](../tests/structured_language.cpp)

- [include/gungnir/language/module.hpp](../include/gungnir/language/module.hpp)
- [include/gungnir/language/dependency_graph.hpp](../include/gungnir/language/dependency_graph.hpp)
- [src/cli/project.cpp](../src/cli/project.cpp)

See the [documentation index](README.md), [getting started](getting-started.md), and [target design](design/modules.md).
