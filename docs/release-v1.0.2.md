# Gungnir 1.0.2

Gungnir 1.0.2 fixes compiler validation and generated C++, development-server
restart recovery, process startup errors and database-pool responsiveness.
Core and application SDKs are available for Windows and Linux. The application
SDK bundles SQLite persistence and OpenSSL password hashing.
The language, compiler, diagnostic and native API contracts remain `1.0`;
the native ABI epoch remains `1`.

## Downloads

Download the matching package from
[GitHub Releases](https://github.com/justinangeloperez327/gungnir/releases/tag/v1.0.2).

| Platform | Core SDK | Application SDK |
| --- | --- | --- |
| Windows setup (x86_64) | `gungnir-v1.0.2-windows-x86_64-setup.exe` | `gungnir-v1.0.2-windows-x86_64-application-setup.exe` |
| Windows portable (x86_64) | `gungnir-v1.0.2-windows-x86_64.zip` | `gungnir-v1.0.2-windows-x86_64-application.zip` |
| Linux portable (x86_64) | `gungnir-v1.0.2-linux-x86_64.tar.gz` | `gungnir-v1.0.2-linux-x86_64-application.tar.gz` |

Check downloads against `SHA256SUMS.txt`. Both profiles include the CLI,
compiler, headers, libraries and CMake package. Install a supported **C++23
compiler** and **CMake 3.25+** to build applications. On Windows, use Visual
Studio C++ Build Tools and reopen a developer terminal after installation.
Linux packages target Ubuntu 24.04. See [SDK Packages](sdk-packages.md).

## Start an application

```sh
gungnir --version
gungnirc --version
gungnir new hello
cd hello
gungnir build
gungnir run
```

Open `http://127.0.0.1:8000/`. Use `gungnir dev` while editing.

## Included fixes

- Added `gungnirc --verify-ir` to validate and verify C++ IR without emitting C++.
- Compiler profile flags and formatting failures now report consistent diagnostics;
  generated-output write and flush failures return an error.
- Null guards remain valid only across paths that preserve them. Loops and arrows
  no longer leak stale non-null facts into later expressions.
- Indexing a nullable list, collection, map, JSON or Data value requires a guard
  or a non-null fallback before code generation.
- Guarded compound assignments and postfix updates to nullable model fields now
  generate valid C++ and retain dirty tracking.
- Arrows reject implicit instance-method captures with `GNR2208`; field captures,
  free functions, injected services and mutable local receivers remain supported.
- Immutable values and captures reject invalid mutations. Map loops yield keys,
  including guarded optional maps and temporary maps.
- Development-server replacement failures preserve the last runnable application.
  Windows restarts request graceful shutdown before forced termination.
- POSIX process startup reports working-directory and executable failures before
  returning and reaps failed children.
- Database connection validation runs outside the pool lock so slow checks do not
  block unrelated pool operations.

SQLite persistence and password hashing require the application SDK. Other
native adapters, including Redis, SMTP and external database drivers, require
custom SDK builds with their native dependencies. Process-local sessions,
queues and caches do not share state across application processes.

Regenerate generated native code and rebuild applications against the selected
SDK. Binary compatibility also requires matching toolchains, platforms, runtime
models and build features. See [Stability](stability.md) and
[Production](production.md) for operating requirements.
