# Transpiler and C++23 Lowering

> **Gungnir 1.x design specification.** This document describes the canonical lowering architecture beyond the frozen `1.0.0-rc.1` contract. The [current transpiler guide](../transpiler.md) is authoritative for shipped behavior.

## 1.0 RC alignment

The 1.0 compiler no longer treats raw-source rewriting as the canonical structured compilation path. The normal compiler boundary is:

```text
.gnr source
    -> lexer
    -> parser
    -> syntax AST
    -> semantic analysis
    -> validated AST
    -> typed structural C++ IR
    -> deterministic C++23 emitter
```

Compatibility/source-edit lowering is an explicit transitional path for compatibility behavior and must not become the architecture for new 1.x language features.

## Design objective

The lowering pipeline should be intentionally boring.

By the time lowering begins, parsing, symbol resolution, type checking, framework validation, control-flow checks, and semantic normalization have already completed.

Lowering answers:

```text
known validated construct
    -> known typed C++ IR
    -> deterministic C++23
```

It must not rediscover language meaning from source text.

## Canonical input

The normal lowerer consumes validated semantic structures, not raw source strings.

Conceptually:

```cpp
CppProgram LoweringPipeline::lower(
    const ValidatedProject& project,
    const LoweringContext& context
);

GeneratedFiles CppEmitter::emit(
    const CppProgram& program,
    const EmitOptions& options
);
```

Exact C++ signatures may evolve compatibly, but the architectural boundary is stable.

## Typed C++ IR

The C++ IR represents target-language structure directly:

- translation units;
- namespaces;
- classes/structs;
- functions and methods;
- parameters;
- variables;
- expressions;
- statements;
- calls;
- member access;
- control flow;
- coroutine operations;
- type references;
- framework-generated registration/metadata.

The IR should not encode arbitrary raw C++ strings when a structural node can represent the same construct.

Raw/native escape nodes are allowed only at explicit interoperability boundaries.

## Type lowering

A centralized type lowerer maps validated Gungnir types into C++23 types.

Examples:

```text
int             -> std::int64_t or contract-defined native integer
bool            -> bool
string          -> framework/native string representation
T?              -> std::optional<T>
array<T>         -> contract-defined sequence representation
Task<T>          -> gungnir::Task<T>
framework model -> generated/native model type
```

The semantic/type system decides what a value means. The emitter only renders the already-selected target type.

## Name lowering

Compiler-generated names must be:

- deterministic;
- collision-safe;
- independent from incidental traversal order;
- stable enough for reproducible builds;
- treated as implementation detail unless explicitly documented.

Applications must not depend on generated helper names or internal namespace spelling.

## Framework declarations

Framework declarations lower structurally.

Examples include:

- models and model metadata;
- controllers and actions;
- middleware;
- migrations;
- policies;
- events/listeners;
- notifications/mail;
- route registration;
- dependency-injection registration;
- validation metadata.

Framework lowering consumes validated declaration metadata. It must not inspect source substrings to infer semantics already established by earlier compiler phases.

## Expressions and statements

Ordinary expressions/statements should lower one-to-one where practical.

Examples:

```text
validated binary expression
    -> CppBinaryExpression

validated call
    -> CppCallExpression

validated if
    -> CppIfStatement

validated return
    -> CppReturnStatement

validated await
    -> CppCoAwaitExpression

return inside async function
    -> CppCoReturnStatement
```

Lowering may introduce temporaries or helpers where ownership, lifetime, coroutine, or framework semantics require them.

## Async lowering

Async is semantic, not textual substitution.

Validated async functions carry enough information for lowering to select:

- `gungnir::Task<T>`;
- `co_await`;
- `co_return`;
- cancellation/lifetime plumbing required by the runtime contract.

The emitter must not guess async behavior from token presence.

## Source mapping

Every emitted construct that corresponds to user source should retain source origin metadata.

The emitter may use:

- `#line` directives;
- sidecar source maps;
- structured generated-file metadata.

Diagnostics should resolve to the original `.gnr` source whenever possible.

## Generated file model

Project compilation may produce multiple generated files.

A generated file can track:

```text
path
contents
source map
content hash
source modules
validated declarations
required native dependencies
compiler/runtime contract metadata
```

This supports incremental builds, diagnostics, reproducibility, and tooling.

## Determinism

Given identical:

- source;
- compiler version/contract;
- target configuration;
- enabled features;

the structured compiler should emit byte-identical canonical output where the conformance contract requires it.

Do not include unstable timestamps, random identifiers, unordered-map traversal artifacts, or machine-local paths in canonical output unless explicitly normalized.

## Security boundary

Lowering must preserve validated security boundaries.

It must not:

- concatenate untrusted runtime data into generated source;
- bypass parameterized database APIs;
- remove authorization/CSRF/session boundaries;
- weaken escaping guarantees;
- drop cancellation/timeout propagation;
- generate unsafe headers/cookie metadata from unvalidated compile-time values.

Runtime user input remains runtime data.

## Native interoperability

Explicit native escape/interoperability features may emit or reference native C++ where the language contract permits it.

Such paths must remain visibly separate from ordinary structured lowering so they do not erode compiler guarantees.

## Compatibility path

A compatibility transpiler may remain for explicitly selected legacy/native-compatible source.

It is not the source of truth for the structured 1.0 language.

New stable structured features should be implemented through:

```text
parser
-> semantics
-> validated AST
-> typed C++ IR
-> emitter
```

rather than by adding new raw-source rewrite passes.

## Testing

Lowering is tested at several levels.

### IR unit tests

Input:

```text
validated node/project
```

Assert:

```text
typed C++ IR structure
```

### Emitter tests

Input:

```text
typed C++ IR
```

Assert deterministic C++23 text.

### End-to-end tests

Input:

```text
.gnr source
```

Run:

```text
parse
semantic validation
validated AST
lower
emit
native compile
execute where applicable
```

### Cross-compiler conformance

Canonical structured output is validated across GCC, Clang, and MSVC workflows, with byte-identical snapshots where required.

## Performance

Measure compiler stages separately:

```text
lex/parse
semantic analysis
validated AST construction
IR lowering
C++ emission
native compilation
runtime execution
```

Do not optimize total build time blindly without identifying the stage responsible.

The Phase 16 benchmark surface provides reproducible compiler measurements for the current implementation.

## Inspectability

Generated C++ should remain readable enough for diagnosis.

A developer inspecting generated code should be able to recognize:

- source-level parameters;
- control flow;
- service/ORM calls;
- response construction;
- framework metadata;
- coroutine suspension points.

Inspectability does not make generated spelling a stable API.

## 1.x evolution rule

During the 1.x line:

- preserve structured-language behavior;
- keep new lowering structural;
- avoid reintroducing semantic decisions into the emitter;
- keep compatibility rewriting isolated;
- version any change that would alter stable source semantics;
- regenerate application C++ with the matching package.

## Relationship to compiler specifications

```text
grammar.md
  legal source form

ast.md
  parsed structure

semantics.md
  meaning, symbols, types and validation

validated-ast.md
  normalized resolved compiler input

transpiler.md
  typed C++ IR lowering and deterministic C++23 emission
```

## Design rule

```text
understand the program once
record the meaning structurally
lower only validated meaning
emit deterministic C++23
never make the emitter guess
```
