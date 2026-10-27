# Gungnir 1.0.3

Gungnir 1.0.3 completes model metadata support and fixes ORM persistence,
query scoping, field mutation tracking and Windows installer provisioning.
Core and application SDKs are available for Windows and Linux. The application
SDK bundles SQLite persistence and OpenSSL password hashing.
The language, compiler, diagnostic and native API contracts remain `1.0`;
the native ABI epoch remains `1`.

## Downloads

Download the matching package from
[GitHub Releases](https://github.com/justinangeloperez327/gungnir/releases/tag/v1.0.3).

| Platform | Core SDK | Application SDK |
| --- | --- | --- |
| Windows setup (x86_64) | `gungnir-v1.0.3-windows-x86_64-setup.exe` | `gungnir-v1.0.3-windows-x86_64-application-setup.exe` |
| Windows portable (x86_64) | `gungnir-v1.0.3-windows-x86_64.zip` | `gungnir-v1.0.3-windows-x86_64-application.zip` |
| Linux portable (x86_64) | `gungnir-v1.0.3-linux-x86_64.tar.gz` | `gungnir-v1.0.3-linux-x86_64-application.tar.gz` |

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

- Model metadata supports guarded mass assignment, pagination defaults, custom
  timestamp columns/formats, cast-aware defaults, appended accessors, automatic
  relationship loading/counts and parent touching.
- Persisted primary-key mutation is rejected before record writes or refreshes.
- SQL identifiers are escaped and automatic soft-delete scopes apply across OR
  branches in reads and bulk writes.
- Compound updates initialize fields and retain dirty tracking. Empty inserts,
  generated-ID failures, update-or-create failures and decimal aggregates are
  handled consistently.
- Model helper calls are available through `.gnr`; invalid model field types,
  metadata and mutable accessor calls are diagnosed before C++ generation.
- Windows packaging verifies each NSIS download before accepting it, using
  checksum-verified mirror fallback and retaining rejection of invalid files.

SQLite persistence and password hashing require the application SDK. Other
native adapters, including Redis, SMTP and external database drivers, require
custom SDK builds with their native dependencies. Process-local sessions,
queues and caches do not share state across application processes.

Regenerate generated native code and rebuild applications against the selected
SDK. Binary compatibility also requires matching toolchains, platforms, runtime
models and build features. See [Stability](stability.md) and
[Production](production.md) for operating requirements.
