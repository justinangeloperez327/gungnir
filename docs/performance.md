# Performance Baseline

> This guide describes the 1.0 contract and its documented limits. See [release scope](release-v1.md).

## Principles

Gungnir separates deterministic correctness gates from wall-clock measurements.

Ordinary CI should fail for deterministic problems such as incorrect output, broken lifecycle behavior, N+1 regressions covered by query-count tests, unbounded resource contracts, or benchmark programs that no longer compile/run.

Ordinary CI should **not** fail solely because a shared CI runner was temporarily slower.

Performance comparisons are meaningful only when the candidate and reference run use comparable:

- hardware;
- operating system;
- compiler and standard library;
- build type and optimization flags;
- feature set;
- benchmark source;
- iteration/sample policy.

## Benchmark suites

Phase 16 adds three Release-mode suites.

### Compiler

`gungnir_benchmark_compiler` measures a structured program containing a model, controller, and 128 functions:

- parse-only frontend cost;
- authoritative `--check`-equivalent semantic validation;
- full compile through generated C++.

This provides separate visibility into parsing/semantic work and backend emission.

### HTTP and routing

`gungnir_benchmark_http_routing` measures:

- HTTP/1 request parsing;
- HTTP/1 response serialization;
- static route matching near the tail of a populated route table;
- constrained parameterized route matching near the tail of a populated route table.

These are in-process microbenchmarks. They do not claim network requests/second because kernel networking, TLS, reverse proxies, concurrency, payload distribution and deployment topology materially affect end-to-end throughput.

### ORM query compiler

`gungnir_benchmark_orm` measures complex query-plan compilation for:

- PostgreSQL;
- MySQL/MariaDB;
- SQL Server;
- MongoDB.

The benchmark measures query construction/compilation only. It intentionally excludes database network and server execution time.

## Building

Benchmarks are opt-in:

```bash
cmake -S . -B build-bench -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DGUNGNIR_BUILD_TESTS=OFF \
  -DGUNGNIR_BUILD_TOOLS=OFF \
  -DGUNGNIR_BUILD_BENCHMARKS=ON

cmake --build build-bench --target gungnir_benchmarks
```

The umbrella `gungnir_benchmarks` target builds all benchmark executables.

## Running

Run the complete baseline:

```bash
python3 benchmarks/run.py \
  --build-dir build-bench \
  --output benchmark-results.json \
  --summary benchmark-summary.md
```

Run the benchmark contract quickly:

```bash
python3 benchmarks/run.py \
  --build-dir build-bench \
  --smoke \
  --output benchmark-smoke.json
```

Each executable also supports:

- `--json`;
- `--smoke`;
- `--iterations=N`;
- `--samples=N`;
- `--warmup=N`.

Results include median nanoseconds/operation, p95 nanoseconds/operation and operations/second. Median is the primary comparison value; p95 is retained to expose unstable runs.

## Comparing runs

Use two like-for-like result files:

```bash
python3 benchmarks/compare.py baseline.json candidate.json
```

This is informational by default.

A controlled environment may choose a regression budget:

```bash
python3 benchmarks/compare.py \
  baseline.json \
  candidate.json \
  --fail-above 1.25
```

That example fails if a comparable candidate median is more than 1.25x the reference median.

Gungnir does not enable a repository-wide wall-clock threshold by default because GitHub-hosted runner variance would make such a gate noisy.

## GitHub Actions

The **Performance Baseline** workflow:

1. configures a Release build with benchmarks enabled;
2. builds all benchmark targets;
3. executes smoke mode to validate the benchmark contract;
4. executes the measurement run;
5. publishes the table to the job summary;
6. uploads JSON and Markdown results as a 30-day artifact.

It runs on relevant pull requests, relevant pushes to `main`, and manual dispatch.

The artifact from a `main` run is the natural reference for a like-for-like pull-request comparison.

## What this phase does not claim

Phase 16 does not publish a universal requests/second number or claim superiority over another framework. Those claims require a separately defined workload, deployment topology, hardware, concurrency model, compiler configuration, database configuration and reproducible external load generator.

It also does not treat microbenchmarks as substitutes for profiling. A measured regression should be reproduced and profiled before changing architecture.

## Performance maturity contract

Before Gungnir 1.0:

- benchmark sources must remain buildable;
- benchmark JSON schema remains versioned;
- measurements must use optimized builds;
- compiler, HTTP/routing and ORM query generation remain represented;
- correctness gates continue to protect algorithmic/resource invariants;
- external comparative benchmarks must publish their complete methodology.

## Implementation references

- [benchmarks/benchmark.hpp](../benchmarks/benchmark.hpp)
- [benchmarks/compiler.cpp](../benchmarks/compiler.cpp)
- [benchmarks/http_routing.cpp](../benchmarks/http_routing.cpp)
- [benchmarks/orm.cpp](../benchmarks/orm.cpp)
- [benchmarks/run.py](../benchmarks/run.py)
- [benchmarks/compare.py](../benchmarks/compare.py)
- [.github/workflows/benchmark.yml](../.github/workflows/benchmark.yml)

See [HTTP Runtime](http-runtime.md), [ORM](orm.md), [Compiler Correctness](compiler-correctness.md), and [Production Resilience](production-resilience.md).
