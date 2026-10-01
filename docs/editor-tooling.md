# Editor tooling

Start `gungnir lsp` over stdin/stdout using the [Language Server Protocol 3.17](https://microsoft.github.io/language-server-protocol/specifications/lsp/3.17/specification/). Stdout is reserved for framed JSON-RPC messages. The server supports local `file://` URIs, UTF-16 positions, initialization/shutdown, full and incremental document changes, versioned diagnostics, document symbols, definition, hover, basic completion, and document formatting.

The editor snapshot reuses the structured parser and validator. Unsaved buffers override disk modules. Saving or a watched-files notification reloads dependencies; changing an open module revalidates the snapshot. Closing a document clears its diagnostics. Valid snapshots also offer local variables and fields in completion. Definitions use resolved symbols, including imported functions, fields, parameters, and local bindings. Hover/navigation require a valid snapshot; document symbols and basic declaration/parameter completion remain available on partially parsed files. The server targets structured source modules. Compatibility routes and arbitrary C++ extensions use their respective tools.

Configure the editor's server command as `gungnir` with argument `lsp`, and select the project root as `rootUri`. No network server or editor-specific extension is required. A client may send `workspace/didChangeWatchedFiles` for external dependency edits.

```sh
gungnirc app/controllers/home_controller.gnr --format
gungnirc app/controllers/home_controller.gnr --strict --format -o formatted.gnr
gungnirc app/controllers/home_controller.gnr --format --check
```

Formatting preserves literal and comment bytes and verifies the significant token stream. It handles nested delimiters and `for` headers, retaining line-comment boundaries. `--strict --format` checks structured syntax first. `--format --check` reports whether a file is already formatted through its exit code, without writing. Invalid lexemes, unbalanced delimiters, and native preprocessor directives/line continuations fail before output is written. Use a C++ formatter for preprocessor-heavy compatibility sources. Formatting uses four spaces; editor tab-width options currently do not change that style.
