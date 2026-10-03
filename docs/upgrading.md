# Upgrading Gungnir

> **Current line: 0.9.x.** Gungnir remains pre-1.0. Pin an exact release in production and review this page plus [Stability](stability.md) before upgrading.

## 0.9 patch upgrades

The 0.9 line treats these as stabilized contracts:

- structured language version 0.9;
- compiler semantic contract 0.9;
- diagnostic contract 0.9;
- representative native C++ API contract 0.9;
- native ABI epoch 0, within a compatible platform/toolchain/standard-library ABI.

A patch upgrade should not require source changes for the stabilized native API set or accepted 0.9 structured-language programs except for documented correctness/security corrections.

## Recommended upgrade procedure

1. Read [CHANGELOG.md](../CHANGELOG.md).
2. Update the installed Gungnir package.
3. Verify:

   ```sh
   gungnir --version
   gungnirc --version
   gungnirc --print-contract
   ```

4. Run semantic validation:

   ```sh
   gungnirc <entry>.gnr --check
   ```

5. Rebuild all generated C++ with the upgraded package.
6. Run the application's native build and test suite.
7. Run database migration planning before applying migrations.
8. Re-run workload-specific performance/security tests for production deployments.

Generated C++ is not a stable cross-version artifact. Always regenerate/rebuild it with the matching framework/compiler package.

## 0.9 to a future 0.10 line

Do not assume a future 0.10 package is source-compatible merely because the major version remains zero.

The CMake package intentionally uses same-minor compatibility for the pre-1.0 line. Review release notes and migration guidance before moving to a new minor contract.

## Native binary compatibility

ABI epoch equality is necessary but not sufficient. Rebuild native applications when changing compiler family, standard library ABI, runtime model, architecture, or incompatible build options.

See [Native API and ABI Stability](native-api-abi.md).
