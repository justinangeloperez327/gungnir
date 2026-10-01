# Compiler profiles

The experimental structured frontend is available through `language::Compiler`, `TranspileOptions::structured_frontend`, and `gungnirc --strict`.

```sh
gungnirc application.gnr --strict -o generated.cpp
gungnirc modules --project -o modules.cpp
gungnirc modules --project --check
gungnirc application.gnr --dump-validated-ast
```

`--project` compiles every `.gnr` module below the root, excluding `.git`, `.gungnir`, `build` and `vendor`. Explicit module declarations match the relative dotted file path. Imported declarations are exported by default. Generated C++ includes the framework runtime support header.

The default `Transpiler` and default `gungnirc` mode preserve native C++ compatibility for projects without `profile=structured`. New application projects use the structured compiler. They do not provide the structured validator's closed-world guarantees. Select a profile explicitly when adding new language features to existing projects.

| Capability | Structured profile |
| --- | --- |
| Functions, literal defaults, named arguments | Implemented |
| Imports, aliases, dependency checks | Implemented |
| Structured AST and validated-only emission | Implemented |
| Single/double quoted strings, numeric separators | Implemented |
| Typed arrows, collections, migration callbacks | Implemented |
| Model attribute descriptors and metadata | Implemented; runtime casting/serialization enforcement remains separate |
| Immutable events, sync/async listeners | Implemented |
| Job payload/worker registration, injected services | Implemented |
| Actor/resource policies | Public synchronous two-argument bindings |
| Mail message composition | Subject/text/html composition; rendering view responses needs native integration |
| Recipient-bound notifications | Implemented; native channels must be registered |
| General classes/interfaces/enums | Unsupported |
| Arbitrary C++, preprocessor, overload resolution | Compatibility profile only |
| String interpolation, advanced null-flow analysis | Unsupported |
| Incremental compilation/application bootstrap | New structured projects emit per-module C++; editable native bootstrap hooks register services |

All capabilities remain experimental. The feature flags in `spec.hpp` describe implemented syntax, not stable release guarantees. See [generated runtime tests](../tests/structured_generated.cpp) for the native integration contracts.
