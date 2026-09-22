# Developer Experience

Gungnir keeps framework mechanics behind the source language while preserving a direct path to generated C++.

## Diagnostics

Compiler diagnostics use a stable location, severity, optional diagnostic code, message and optional hint. The command-line compiler renders these fields consistently.

Generated C++ retains line directives by default so downstream C++ diagnostics can refer back to Gungnir source locations. Use `--no-line-directives` only when inspecting generated output requires it.

## Source inspection

`gungnirc file.gnr` writes generated C++ to standard output. `-o` writes it to a file, `--check` validates without emitting generated code, and `--format` formats Gungnir source.

## Principles

Developer experience features must not hide failures, invent runtime capabilities, or generate syntax unsupported by the transpiler. Diagnostics should point to application source whenever enough source information exists.

Hot reload is not advertised until the development runtime can reliably watch files, rebuild, restart and report failures without orphaning processes.
