# Gungnir

Gungnir is an experimental C++23 web framework and source-language toolchain focused on expressive application code with native C++ interoperability.

The project includes a Gungnir-to-C++ transpiler, application lifecycle and dependency container, routing and HTTP primitives, validation, database abstractions, ORM, migrations, authentication and authorization foundations, sessions, cache, views, storage, events, queues, mail and notifications, scheduling, CLI/code generation, testing helpers, structured logging, production health contracts and extension/provider APIs.

## Status

Gungnir is under active development. The repository version is currently 0.1.0 and the public API is not yet declared stable.

Several subsystems intentionally expose foundations rather than claiming production completeness. In particular, production database adapters, high-concurrency networking, full graceful draining, distributed queue/cache/session backends, complete relationship-query APIs, telemetry exporters and dynamic plugin loading require further work.

## Language

Application source may use Gungnir syntax and be lowered to C++23. The generated C++ remains inspectable, and line directives are emitted by default to preserve source locations where supported by the downstream compiler.

## Build

Gungnir uses CMake 3.25 or newer and C++23. Build tools and tests are controlled by `GUNGNIR_BUILD_TOOLS` and `GUNGNIR_BUILD_TESTS`.

## Documentation

Subsystem documentation is maintained under `docs/`. Documentation describes implemented behavior and should not be treated as a promise for capabilities explicitly marked as future work or limitations.

## Stability

Until a stable release policy is published, consumers should pin the exact Gungnir version or commit they validate against.
