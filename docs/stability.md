# Version Compatibility

Gungnir v1.0.3 publishes a stable language, compiler and native API
contract. Verify your installed tools with:

```sh
gungnir --version
gungnirc --version
gungnirc --print-contract
```

The package version is `1.0.3`, the language/compiler/diagnostic contract
is `1.0`, and the native API contract is `1.0` with ABI epoch `1`.

## Upgrade an application

Generated C++ must be **regenerated and rebuilt** using the matching
Gungnir compiler and native runtime SDK. Native compatibility additionally
depends on the operating system, architecture, compiler, C++ standard
library and compiled adapters.

Install the intended SDK, confirm its `PATH`/`GUNGNIR_CMAKE_PREFIX`,
then rebuild your project. Re-test database and external service
connections before deployment.

See [SDK Packages](sdk-packages.md),
[Native API and ABI](native-api-abi.md) and
[Upgrading](upgrading.md).
