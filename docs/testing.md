# Testing

## Overview
Native `testing::Http` dispatches through the real router and calls the resulting task. It exposes `get`, `post` and generic `send`; use `send` for other HTTP methods. Response assertions and application helpers are provided.

Use memory stores/transports/sinks to inspect application behavior. Compile `.gnr` examples to generated C++ and check that generated code compiles with real framework headers.

## Compiler correctness tests

Compiler testing is split by responsibility:

- **compile-pass** cases prove that valid structured programs parse, validate and emit;
- **compile-fail** cases prove that invalid programs stop before code generation and retain Gungnir source diagnostics;
- **run-pass/generated** cases compile emitted C++ against the real runtime and verify observable behavior;
- **regression** cases permanently cover fixed compiler defects;
- **robustness** cases feed malformed and adversarial source and require controlled diagnostics rather than crashes;
- **fuzzing** targets continuously mutate lexer and structured-compiler inputs under AddressSanitizer and UndefinedBehaviorSanitizer;
- **determinism** cases require identical validated dumps and emitted output for identical inputs, including reordered multi-file input;
- **cross-compiler conformance** requires the same structured fixtures to build, execute and emit byte-identical compiler snapshots under GCC, Clang and MSVC.

`tests/compiler_correctness.cpp` is the focused invariant gate for the structured compiler. It complements `structured_language.cpp`, `frontend_diagnostics.cpp` and generated/native execution tests. Gungnir adds a semantic-closure corpus that runs in validation-only mode and compares its accept/reject decisions and diagnostic codes with normal compilation, so `gungnirc --check` cannot silently become weaker than the full compiler pipeline. Gungnir adds `gungnir.compiler_robustness`, deterministic mutation coverage, bounded parser nesting, and Clang/libFuzzer targets for the lexer and authoritative structured compiler. Gungnir adds `gungnir.compiler_conformance_snapshot` plus a targeted GCC/Clang/MSVC CI matrix that compiles and executes generated structured code and compares compiler outputs byte-for-byte. See [Compiler Fuzzing](fuzzing.md) and [Compiler Conformance](compiler-conformance.md).

See [Compiler Correctness](compiler-correctness.md) and [Compiler Conformance](compiler-conformance.md).

## Framework semantic tests

Gungnir adds `gungnir.framework_semantics` to the primary PR gate. It verifies positive and negative contracts for middleware, migrations, listeners, jobs, policies, events, notifications, mail, and model lifecycle metadata. The generated structured-program test also compiles and executes async middleware forwarding and verifies timestamp/soft-delete model attributes.

See [Framework Semantic Contracts](framework-semantics.md).

## Runtime correctness tests

Gungnir promotes lifecycle behavior into the primary PR gate. The focused runtime set covers:

- request cancellation on disconnect and request deadline;
- graceful in-flight request drain after admission closes;
- rejection of a second listen lifecycle while the first is draining;
- bounded shutdown-deadline cancellation for cooperative handlers;
- cancellation propagation into response-stream producers;
- cancellation propagation into WebSocket message handlers;
- timer/coroutine ownership safety;
- supervisor and RuntimeHost shutdown behavior.

`tests/runtime_lifecycle.cpp` is the lifecycle invariant gate. HTTP reactor, streaming, WebSocket, runtime safety, timer, supervisor and RuntimeHost tests run beside it in primary CI.

See [Runtime Correctness](runtime-correctness.md).

## Database and ORM correctness tests

Gungnir adds focused behavioral gates:

- `gungnir.database_correctness` verifies transaction ownership, nested savepoint ordering, after-commit behavior, pool cancellation/timeout accounting, reconnect behavior, error context and lease cleanup;
- `gungnir.orm_correctness` verifies strict hydration, dirty-state preservation, insert/update transitions, batched eager loading and one-slot-pool lease cleanup;
- `gungnir.sqlite_correctness` runs a live in-memory SQLite baseline for commit/rollback, nested savepoints, foreign keys, prepared bindings, nulls and exact decimal text.

Existing database cancellation, scope-safety, transaction, relationship, soft-delete and timestamp tests run beside these focused invariants in the primary PR gate.

See [Database and ORM Correctness](database-correctness.md).

## Documentation and ecosystem tests

Gungnir adds a documentation contract gate. It verifies:

- required project ecosystem files are present;
- the root README release version matches `CMakeLists.txt`;
- stale 0.1-era release references do not reappear in current entry-point docs;
- relative links in current public guides resolve;
- every current top-level guide is indexed by `docs/README.md`;
- the canonical hello example files exist;
- the canonical hello controller passes `gungnirc --check` and full structured compilation.

This keeps public onboarding coupled to the same compiler contract users install.

## Scope
A successful `gungnirc --check` is not a full native type check or a live transport/integration test. Router-only tests do not cover TLS, HTTP parsing, database connectivity or distributed leases. Run relevant integration tests for the selected adapter.



- [include/gungnir/testing/http.hpp](../include/gungnir/testing/http.hpp)
- [include/gungnir/testing/response_assertions.hpp](../include/gungnir/testing/response_assertions.hpp)
- [include/gungnir/testing/application.hpp](../include/gungnir/testing/application.hpp)

See the [documentation index](README.md), [getting started](getting-started.md), and [target design](design/testing.md).
