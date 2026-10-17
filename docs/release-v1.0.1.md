# Gungnir 1.0.1

Gungnir 1.0.1 includes resource-lifetime and concurrency fixes, refreshed
Windows installers, and core/application SDK packages for Windows and Linux.
The application SDK bundles SQLite persistence and OpenSSL password hashing.
The language, compiler, diagnostic and native API contracts remain `1.0`;
the native ABI epoch remains `1`.

## Downloads

Download the matching package from
[GitHub Releases](https://github.com/justinangeloperez327/gungnir/releases/tag/v1.0.1).

| Platform | Core SDK | Application SDK |
| --- | --- | --- |
| Windows setup (x86_64) | `gungnir-v1.0.1-windows-x86_64-setup.exe` | `gungnir-v1.0.1-windows-x86_64-application-setup.exe` |
| Windows portable (x86_64) | `gungnir-v1.0.1-windows-x86_64.zip` | `gungnir-v1.0.1-windows-x86_64-application.zip` |
| Linux portable (x86_64) | `gungnir-v1.0.1-linux-x86_64.tar.gz` | `gungnir-v1.0.1-linux-x86_64-application.tar.gz` |

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

## Included fixes and capabilities

- Cancellation and container cleanup release user-owned resources outside
  registry locks, preventing destructor re-entry deadlocks.
- Logger dispatch tolerates sinks registering additional sinks.
- Coroutine waiters have explicit move-only ownership, and partial executor
  startup cleans up workers on failure.
- SMTP and Windows environment buffers use automatic resource cleanup.
- Windows setup validates upgrades, paths containing spaces and long user PATHs;
  PATH-update failures report a nonzero setup exit code.
- Both SDK profiles include current Spear templates and typed delivery APIs.

SQLite persistence and password hashing require the application SDK. Other
native adapters, including Redis, SMTP and external database drivers, require
custom SDK builds with their native dependencies. Process-local sessions,
queues and caches do not share state across application processes.

Regenerate generated native code and rebuild applications against the selected
SDK. Binary compatibility also requires matching toolchains, platforms, runtime
models and build features. See [Stability](stability.md) and
[Production](production.md) for operating requirements.
