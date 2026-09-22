# Command Line and Code Generation

Gungnir's command line is project-aware and generates Gungnir source rather than exposing C++ plumbing.

## Project commands

- `gungnir new <name> [path]`
- `gungnir build [--release]`
- `gungnir run [--release]`
- `gungnir dev`

Project discovery walks upward from the current path until it finds `.gungnir-project`.

## Generators

The CLI supports:

- `make:model`
- `make:controller`
- `make:middleware`
- `make:migration`
- `make:request`
- `make:job`

Generated names are normalized consistently and existing files are never overwritten. The generator fails instead of silently replacing application code.

Generators emit Gungnir source files. They should remain aligned with syntax supported by the transpiler; code generation must not become an alternate language implementation.

## Database development

Migration commands remain available for migrate, rollback, reset, status and plan operations.

## Scope

Code generation is deliberately deterministic. Interactive scaffolding, plugin generators and application-specific templates can be layered later without changing the core source conventions.
