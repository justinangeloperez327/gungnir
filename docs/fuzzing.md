# Compiler Fuzzing

> **Status: Phase 8 robustness contract for the structured compiler.**

Gungnir uses two complementary robustness gates. The normal test suite runs a deterministic adversarial corpus on every compiler CI run, while Clang/libFuzzer continuously mutates raw lexer input and complete structured compiler input under sanitizers.

## Invariants

Arbitrary input must resolve to one of these outcomes:

```text
valid source   -> validated project
invalid source -> Gungnir diagnostic
```

Source input must not cause:

- segmentation faults or memory corruption;
- sanitizer findings;
- uncaught exceptions;
- assertion failures;
- unbounded parser recursion;
- invalid diagnostic source spans;
- generated C++ from a failed validation.

The structured parser limits nested syntax to 256 active recursive constructs. Inputs that exceed that bound fail with `GNR2004` instead of continuing toward stack exhaustion.

## Deterministic robustness gate

`tests/compiler_robustness.cpp` exercises lexer, parser and validation-only compilation over a fixed adversarial corpus and deterministic mutations. It includes truncation, delimiter mutation, embedded NUL bytes, invalid UTF-8 byte sequences, malformed literals, long token runs and deeply nested syntax.

This test is registered as:

```text
gungnir.compiler_robustness
```

It runs in the ordinary PR validation job and is intentionally deterministic so a failure can be reproduced exactly.

## libFuzzer targets

Enable fuzz targets with Clang:

```bash
cmake -S . -B build-fuzz -G Ninja \
  -DCMAKE_CXX_COMPILER=clang++ \
  -DGUNGNIR_BUILD_TESTS=OFF \
  -DGUNGNIR_BUILD_TOOLS=OFF \
  -DGUNGNIR_BUILD_FUZZERS=ON

cmake --build build-fuzz --target \
  gungnir_lexer_fuzzer \
  gungnir_compiler_fuzzer
```

The targets are:

- `gungnir_lexer_fuzzer` — tokenization, lexer diagnostics and source-span invariants;
- `gungnir_compiler_fuzzer` — structured parsing plus authoritative validation-only compilation.

When fuzzing is enabled, `gungnir-language` and the fuzz executables are instrumented with AddressSanitizer and UndefinedBehaviorSanitizer. libFuzzer provides the mutation engine.

Example local campaign:

```bash
./build-fuzz/gungnir_lexer_fuzzer tests/fuzz/corpus/lexer -max_total_time=300
./build-fuzz/gungnir_compiler_fuzzer tests/fuzz/corpus/compiler -max_total_time=300
```

## CI policy

Pull requests and pushes run a short sanitizer-backed fuzz campaign using the committed seed corpora. CI limits individual fuzz inputs and execution time so this gate remains suitable for normal development.

A fuzz failure is a compiler defect. Preserve the minimized reproducer as a corpus seed or focused regression test, fix the underlying invariant, and keep that reproducer permanently.

## Scope

Phase 8 fuzzes the compiler frontend and semantic firewall. Native backend cross-compiler behavior belongs to Phase 9 and is covered separately by compiler conformance work.
