# CLI and Code Generation

> **Status: experimental, pre-1.0.** This page describes the current implementation. Native C++ APIs and `.gnr` syntax are identified separately. Proposed contracts are in the [design specification](design/cli-codegen.md).

The structured profile (`gungnirc --strict`) supports typed functions and framework actions, structured callbacks, and validated C++ emission. See [compiler profiles](compiler-profiles.md) for usage and current limits. The compatibility profile retains the native syntax described below.

## Current behavior

| Command | Current purpose |
| --- | --- |
| `gungnir new <name> [path]` | Create a project |
| `gungnir build [--release]` | Generate native source and build with CMake |
| `gungnir run [--release]` | Build/run the project |
| `gungnir dev` | Run in development mode |
| `gungnir make:model <name>` | Generate a model |
| `gungnir make:controller <name>` | Generate a typed controller |
| `gungnir make:middleware <name>` | Generate middleware |
| `gungnir make:migration <name>` | Generate a migration |
| `gungnir make:request <name>` | Generate a native request-validation helper |
| `gungnir make:job <name>` | Generate a job |
| `gungnir migrate` | Apply migrations |
| `gungnir migrate:rollback` | Roll back migrations |
| `gungnir migrate:reset` | Reset migrations |
| `gungnir migrate:status` | Inspect applied state |
| `gungnir migrate:plan` | Inspect migration plans |
| `gungnir --version` | Show repository CLI version |

`gungnirc <input.gnr> [-o output.cpp] [--check] [--no-line-directives]` transpiles one file. `--format` formats one file and writes to stdout or `-o`.

Project discovery uses `.gungnir-project`. Generated files must not overwrite existing files silently. `GUNGNIR_CMAKE_PREFIX` supplies an installed package search prefix.

## Limits and planned work

The compiler does not expose the proposed AST/symbol/type/semantic/IR dump flags, `--emit-cpp`, or a project argument for `--check`/`--format`. Policy/event/listener/notification/mail generators are planned. `dev` currently delegates to development `run`; it is not a file watcher or hot-reload guarantee. Some generators still emit legacy `class ... : ...` syntax.

## Implementation references

- [tools/gungnir.cpp](../tools/gungnir.cpp)
- [tools/gungnirc.cpp](../tools/gungnirc.cpp)
- [src/cli/project.cpp](../src/cli/project.cpp)

See the [documentation index](README.md), [getting started](getting-started.md), and [target design](design/cli-codegen.md).
