# Application SDK packaging contract

Group 2 continues the accepted post-v1 order with an installed SDK option for
SQLite and password authentication. It uses the existing database driver,
OpenSSL password backend, CLI and canonical validated compiler output. It does
not introduce a second compiler or change package/native contract versions.

## Installed profiles

`GUNGNIR_APPLICATION_SDK=ON` defaults both `GUNGNIR_WITH_SQLITE` and
`GUNGNIR_WITH_PASSWORD` to ON. An explicit conflicting cached value fails with
fresh-build guidance. Default source builds remain core; manually enabled
adapters produce a custom SDK.

The application install contains static SQLite and OpenSSL Crypto archives,
their headers and license/notice files. Its exported framework interface uses
`gungnir::sdk_sqlite` and `gungnir::sdk_crypto` with paths relative to the installed
prefix. It does not call dependency discovery for those libraries or reuse a
consumer's unrelated `SQLite::SQLite3` / `OpenSSL::Crypto` targets. Platform link
dependencies are retained. Shared archives, unsupported bundled platforms,
shared Gungnir builds and extra crypto dependencies are rejected rather than
silently producing a nonportable bundle.

Windows uses a pinned vcpkg source/port baseline and a Release-only static
dependency triplet with dynamic CRT, matching generated applications' /MD.
Linux packaging obtains maintained native packages on Ubuntu 24.04. Dependency
versions and the toolchain/platform are recorded in `share/gungnir/sdk.txt`.
This is an SDK bundle, not a compiler/OS runtime installer. Updating a statically
linked dependency requires rebuilding the SDK and its applications.

## Capability errors

Generated CMake reads `Gungnir_WITH_PASSWORD`. The CLI derives the requirement
from resolved password/authentication builtin bindings in `ValidatedProject`,
so comments, strings and similarly named application functions cannot trigger
the check. Semantic checks remain independent of an installed backend; the
package requirement runs before generated C++ compilation.

Missing SQLite and other adapters report their build/registration option.
Password runtime fallbacks and guard middleware identify the application SDK,
`GUNGNIR_WITH_PASSWORD=ON` and `GUNGNIR_CMAKE_PREFIX` recovery path. Diagnostics do
not echo database connection settings or credentials.

## Acceptance

`tests/sdk_application.py` copies an installed SDK into a path with spaces, uses
its CLI's automatic prefix discovery, and verifies:

- accurate profile/capability metadata and bundled notices;
- bundled dependency resolution while CMake OpenSSL/SQLite discovery is disabled;
- imported archive paths refer to the relocated SDK;
- selecting an SDK replaces a stale cached Gungnir package locator;
- native SQLite/OpenSSL headers compile and link, including vcpkg's SQLite companion header and Linux multiarch OpenSSL configuration headers;
- ordinary `.gnr` models, migrations and controllers build without generated C++ repair;
- migrations apply once, status matches, and rollback removes the table;
- SQLite persists a salted scrypt hash; serialized models hide the password;
- wrong passwords fail, login creates a session, logout clears authentication;
- a new server process authenticates against the stored hash, using development memory sessions;
- core password applications stop at configuration with installation guidance;
- core SQLite startup fails with adapter guidance while core HTTP remains usable.

`tests/release_smoke.py` retains the installed core/application HTTP acceptance.
The release matrix builds both profiles for Linux/Windows and creates both
Windows installers. Installer checks run SDK acceptance on files installed by
NSIS, alongside long user-PATH preservation and uninstall restoration.

Local evidence: GCC 13 built the application bundle; the relocated installed
database/auth application and disabled-discovery native header/link probe passed.
Core password/SQLite capability errors and an installed Release HTTP application
passed. Seven focused CLI/database/authentication/public API/documentation and
release-contract regressions passed. Platform,
packaging and exact-commit CI results are recorded in the pull request.

The published v1.0.0 core assets and their historical release audit are unchanged.
Main development skips republishing an existing version, and PR packaging cannot
publish. Other database acceptance, Redis/external adapters, production
persistence, deployment and load testing remain their existing roadmap groups.
