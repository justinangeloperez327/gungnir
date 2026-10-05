# Compiler profiles

The structured compiler is the canonical Gungnir compiler. `gungnirc` now uses `language::Compiler` by default.

```sh
gungnirc application.gnr -o generated.cpp
gungnirc application.gnr --check
gungnirc modules --project -o modules.cpp
gungnirc modules --project --check
gungnirc application.gnr --dump-validated-ast
gungnirc application.gnr --dump-cpp-ir
```

`--strict` remains accepted as a backwards-compatible alias for the structured default. It no longer selects a different compiler.

## Compatibility profile

Legacy/native-C++-compatible source is available only through the explicit compatibility profile:

```sh
gungnirc legacy.gnr --compat -o generated.cpp
gungnirc legacy.gnr --compat --check
```

The compatibility profile uses `language::CompatibilityTranspiler`, the legacy token/source-edit lowering pipeline. The old `language::Transpiler` name remains as a source-compatible alias, but new framework and language features must not be implemented there.

`--compat` cannot be combined with `--project` or `--dump-validated-ast`.

`--project` compiles every `.gnr` module below the root, excluding `.git`, `.gungnir`, `build` and `vendor`. Explicit module declarations match the relative dotted file path. Imported declarations are exported by default. Generated C++ includes the framework runtime support header.

New application projects use `profile=structured`. Projects without that marker retain their legacy project-build behavior so existing applications are not silently reinterpreted.

| Capability | Structured compiler |
| --- | --- |
| Functions, literal defaults, named arguments | Implemented |
| Imports, aliases, dependency checks | Implemented |
| Structured AST and validated-only emission | Implemented |
| Single/double quoted strings, numeric separators | Implemented |
| Typed arrows, collections, migration callbacks | Implemented |
| Model attribute descriptors and metadata | Implemented; lifecycle metadata and supported casts are semantically enforced |
| Immutable events, sync/async listeners | Implemented |
| Job payload/worker registration, injected services | Implemented |
| Middleware contracts | Public `handle(Request, Next) -> Response`; sync and async supported |
| Actor/resource policies | Public synchronous `Decision(actor, resource)` bindings with model actor/resource types |
| Mail message composition | Subject/text/html plus `content() -> Response`; conflicting HTML body sources are rejected |
| Recipient-bound notifications | `via`, `toMail`, and `toDatabase` contracts are validated; native channels must still be registered |
| General classes/interfaces/enums | Unsupported |
| Arbitrary C++, preprocessor, overload resolution | Compatibility profile only |
| String interpolation, advanced null-flow analysis | Unsupported |
| Incremental compilation/application bootstrap | Structured projects emit per-module C++; editable native bootstrap hooks register services |

Phase 13 adds authoritative framework-artifact semantic contracts for the already-supported declaration surface. These checks close previously partial runtime/compiler contracts rather than introducing general-purpose language constructs. See [Framework Semantic Contracts](framework-semantics.md).

The structured 1.0 profile reports `stable` compatibility and a frozen feature contract. Compatible additions may be introduced in later releases. Generated C++ remains a rebuild artifact; native source and ABI guarantees are defined by the [native policy](native-api-abi.md).

Use `gungnirc --print-contract` to inspect the package, language, compiler, diagnostic, and freeze metadata. See [Stability](stability.md), [generated runtime tests](../tests/structured_generated.cpp), and [compiler correctness](compiler-correctness.md).
