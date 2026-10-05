# SDK Packages

Choose the application SDK for SQLite-backed applications and password
authentication. Choose the core SDK for HTTP applications that do not need
those features.

The application SDK is available in current source. The published v1.0.0 assets
contain the core SDK; they are not replaced by source changes. The release
packaging workflow builds both profiles for the next release.

| Package | Included capabilities | Dependencies |
| --- | --- | --- |
| Core | CLI, compiler, HTTP/runtime, headers, ORM/query compiler, CMake package | Supported C++23 compiler and CMake |
| Application | Core plus SQLite connections, migrations/ORM persistence and OpenSSL scrypt password hashing | SQLite and OpenSSL Crypto static libraries, headers and notices are bundled; a compiler and CMake are still required |
| Custom source build | Selected database, Redis, SMTP, TLS, HTTP/2, storage or collector adapters | Install the selected adapters' native dependencies |

Application packages use an `-application` suffix, for example
`gungnir-v<version>-windows-x86_64-application.zip`,
`gungnir-v<version>-windows-x86_64-application-setup.exe`, and
`gungnir-v<version>-linux-x86_64-application.tar.gz`. Core asset names stay the
same. Windows applications use the MSVC x64 dynamic C/C++ runtime; Linux release
packages are built on Ubuntu 24.04. Use a matching compiler/platform runtime.

## Install and select an SDK

The Windows setup executable installs the complete selected SDK and updates the
current user's PATH. Open a new terminal after installation. For a portable
package, extract the whole directory and add its `bin` directory to PATH.

```sh
gungnir --version
gungnirc --version
gungnir new hello
cd hello
gungnir build
```

The installed CLI finds its adjacent SDK automatically. When selecting another
SDK explicitly, set its root directory, not its `bin` directory:

```powershell
$env:GUNGNIR_CMAKE_PREFIX = 'C:\SDKs\Gungnir-application'
gungnir build
```

```sh
export GUNGNIR_CMAKE_PREFIX=/opt/gungnir-application
gungnir build
```

SDKs contain `share/gungnir/sdk.txt`, which lists the profile, enabled adapters,
dependency versions, compiler and platform. Native CMake consumers can inspect
`Gungnir_SDK_PROFILE`, `Gungnir_WITH_SQLITE`, `Gungnir_WITH_PASSWORD` and
`Gungnir_BUNDLED_APPLICATION_DEPENDENCIES` after `find_package(Gungnir CONFIG)`.
Application dependencies use relocatable SDK targets; consuming applications do
not need vcpkg or separate SQLite/OpenSSL development installations.

## Use SQLite and password authentication

Configure a relative database path in the project's `.env`:

```dotenv
DB_CONNECTION=sqlite
DB_DATABASE=storage/app.sqlite
```

The parent directory must exist. The application SDK registers SQLite
automatically. Create models and migrations normally, then run:

```sh
gungnir migrate
gungnir migrate:status
gungnir run
```

Use `Password::hash`, `Password::verify` and the session guard as described in
[Authentication](authentication.md). The SDK supplies the password backend;
applications still configure their credential/identity resolvers and session
middleware. Development memory sessions do not persist across restarts. See
[Sessions](session.md) for persistent store configuration.

## Missing capabilities

If a structured application calls password or authentication APIs while its
selected SDK lacks password support, `gungnir build` reports the missing backend
before native compilation. Install/select the application SDK, or build/install
Gungnir with `GUNGNIR_WITH_PASSWORD=ON`, and rebuild the application.

An application configured for SQLite with a core SDK reports
`GUNGNIR_WITH_SQLITE=ON` and application SDK installation guidance at startup.
Changing the generated application's CMake options does not add an adapter to
an already compiled framework library. Select a framework SDK containing it.
Leave `DB_CONNECTION` empty when the application does not need a database.

Other databases and external services require their corresponding adapters;
SQLite/password packaging does not include Redis, SMTP or TLS automatically.

## Build an application SDK from source

Use a fresh build directory. `GUNGNIR_APPLICATION_SDK=ON` enables SQLite and
password support and bundles their static dependencies when installing or
creating an installer. Source builders need the dependency headers/libraries;
consumers of the resulting SDK do not.

On Ubuntu:

```sh
sudo apt-get update
sudo apt-get install -y ninja-build libsqlite3-dev libssl-dev
cmake -S . -B build-application -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DGUNGNIR_APPLICATION_SDK=ON \
  -DGUNGNIR_BUILD_TESTS=OFF
cmake --build build-application --parallel 2
cmake --install build-application --prefix "$PWD/sdk-application"
```

On Windows, from the repository in PowerShell:

```powershell
./cmake/windows/BuildApplicationDependencies.ps1 -WorkDir "$PWD/.gungnir-dependencies"
$deps = "$PWD/.gungnir-dependencies/installed/x64-gungnir-windows"
cmake -S . -B build-application -A x64 `
  -DGUNGNIR_APPLICATION_SDK=ON `
  -DGUNGNIR_BUILD_TESTS=OFF `
  "-DCMAKE_PREFIX_PATH=$deps" "-DOPENSSL_ROOT_DIR=$deps"
cmake --build build-application --config Release --parallel 2
cmake --install build-application --config Release --prefix "$PWD/sdk-application"
```

The Windows dependency manifest pins the vcpkg source and port baseline. Its
triplet builds static dependencies with the dynamic MSVC runtime used by generated
applications. Dependency notices are copied into `share/gungnir/licenses`.
For dependencies installed elsewhere, set `GUNGNIR_OPENSSL_NOTICE` and
`GUNGNIR_SQLITE_NOTICE` to their license/notice files. Bundling rejects shared
libraries and extra unbundled crypto dependencies. Rebuild the SDK and its
applications when upgrading a bundled dependency.
