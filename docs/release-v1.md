# Version 1.0.0

Gungnir 1.0.0 packages the merged framework for building and running applications. The release includes the canonical structured compiler, models/ORM/relationships, migrations, routing and binding, requests and responses, validation, authentication and policies, views, configuration and DI, events/jobs/scheduling, cache/storage and observability.

## Install and run

Download from [GitHub Releases](https://github.com/justinangeloperez327/gungnir/releases):

| Platform | Asset |
| --- | --- |
| Windows x86_64 | `gungnir-v1.0.0-windows-x86_64-setup.exe` |
| Windows x86_64 portable SDK | `gungnir-v1.0.0-windows-x86_64.zip` |
| Linux x86_64 SDK | `gungnir-v1.0.0-linux-x86_64.tar.gz` |
| Integrity | `SHA256SUMS.txt` |

Packages contain `gungnir`, `gungnirc`, public headers, native libraries and the CMake package. They require a C++23 compiler and CMake 3.25 or newer to build an application. Windows users need Visual Studio Build Tools with C++ support; reopen the terminal after installation to refresh PATH. Use a Visual Studio developer terminal if CMake cannot locate the compiler.

```sh
gungnir --version
gungnirc --version
gungnir new hello
cd hello
gungnir build
gungnir dev
```

Open `http://127.0.0.1:8000`. Set `APP_PORT` in `.env` to choose another port. For an existing application, rebuild it using the new package. The CLI discovers the SDK alongside its executable; a custom SDK location can be supplied through `GUNGNIR_CMAKE_PREFIX`.

## Packaged core and optional adapters

The portable packages and Windows installer contain the dependency-light core. A basic application starts with `DB_CONNECTION` empty. Network/database adapters are optional source-build features, not bundled into these assets. For example, SQLite models require rebuilding/installing Gungnir with `GUNGNIR_WITH_SQLITE=ON` and SQLite development files; password hashing requires `GUNGNIR_WITH_PASSWORD=ON` and OpenSSL 3. See [Database](database.md), [Authentication](authentication.md) and the adapter guides for configuration and dependencies.

Memory queues, sessions, cache and locks are process-local. Separate servers and workers require configured shared adapters. Selecting a database in `.env` does not install or build its adapter.

## Release scope

The release freezes the existing documented language/native contracts. The design documents describe future capabilities and are not additional 1.0 APIs. Generated C++ remains an artifact to regenerate and rebuild using the matching SDK.

Native mail composition, memory/SMTP transports and notification channels exist. Typed `.gnr` sending, recipient routing and generated queued mail/notification delivery acceptance remain follow-up work. They are not part of the release's generated application guarantee. Applications that need delivery can use the existing native bootstrap/adapters and ordinary jobs.

Optional adapters remain dependent on provisioned services and build flags. Live SQL Server/MongoDB/SMTP and external collector/cloud acceptance are not established by the core package tests. Production topology, TLS/proxy settings, persistence/durability, backups, monitoring and workload capacity must be configured for the deployed application.

See [Stability](stability.md), [Native API and ABI](native-api-abi.md) and [Upgrading](upgrading.md).
