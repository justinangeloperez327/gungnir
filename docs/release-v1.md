# Install Gungnir 1.0.0

Install Gungnir from the published
[GitHub release](https://github.com/justinangeloperez327/gungnir/releases).
The binary downloads published on 5 October 2026 contain the **core SDK**;
building current source is necessary to use APIs and application SDK capabilities that are not contained in these downloads.

## Downloads

| Platform | Download |
| --- | --- |
| Windows installer (x86_64) | `gungnir-v1.0.0-windows-x86_64-setup.exe` |
| Windows portable (x86_64) | `gungnir-v1.0.0-windows-x86_64.zip` |
| Linux portable (x86_64) | `gungnir-v1.0.0-linux-x86_64.tar.gz` |
| Integrity checks | `SHA256SUMS.txt` |

These packages provide the `gungnir` CLI, `gungnirc` compiler, public
headers, native libraries and installed CMake package. To compile apps,
install a compatible **C++23 compiler** and **CMake 3.25+**.

On Windows, install Visual Studio C++ Build Tools. Reopen the terminal after
installing the SDK and use a C++ developer terminal if the compiler cannot
be located.

## First application

```sh
gungnir --version
gungnirc --version
gungnir new hello
cd hello
gungnir build
gungnir run
```

Visit `http://127.0.0.1:8000/`. Set `APP_PORT` in the application's
`.env` file to change the port. Use `gungnir dev` while editing.

## Choose the correct SDK

The published core downloads **do not bundle** SQLite persistence,
password hashing, Redis, SMTP or other optional native adapters. To use
them, build/install an SDK with the necessary native dependencies and
adapter flags. For example:

- SQLite: `GUNGNIR_WITH_SQLITE=ON` and SQLite dependencies.
- Password hashing: `GUNGNIR_WITH_PASSWORD=ON` and OpenSSL 3.
- Redis, SMTP and external database drivers: use the adapter-specific
  installation instructions.

Current source also supports an application SDK profile bundling SQLite
and password hashing; see [SDK Packages](sdk-packages.md). Updating
`.env` alone cannot add an adapter to an already compiled SDK.

Process-local memory sessions, queues and caches do not share state across
separate application processes. Configure persistent adapters for
multiple-server applications and production deployments.

Generated native source must be regenerated and rebuilt with the matching
Gungnir SDK. See [Getting Started](getting-started.md),
[Database](database.md), [Stability](stability.md) and
[Production](production.md).
