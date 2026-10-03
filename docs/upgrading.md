# Upgrading Gungnir

> **Status: Development.** Development builds have no public compatibility version. Pin exact commits or artifacts when evaluating main and rebuild generated/native code after framework changes.

## Historical releases

Published historical releases such as `0.9.0` remain valid historical artifacts, but they do not describe the current development contract on `main`.

## Moving from a historical release to development

1. Read [CHANGELOG.md](../CHANGELOG.md).
2. Install/build the exact development revision you intend to test.
3. Verify:

   ```sh
   gungnir --version
   gungnirc --version
   gungnirc --print-contract
   ```

4. Development builds should report `development` contract identities.
5. Run `gungnirc <entry>.gnr --check`.
6. Regenerate/rebuild all generated C++.
7. Rebuild native application code.
8. Run the complete application test suite.
9. Plan database migrations before applying them.
10. Re-run workload-specific integration, security and performance tests.

## Development compatibility

Do not assume source or ABI compatibility between arbitrary development commits.

The project tries to evolve coherently and regression tests protect implemented behavior, but the framework remains free to make necessary design changes until the 1.0 completeness gate is closed.

## Future 1.0

When the framework is complete, a final coordinated release change will assign the stable 1.0 package/language/compiler/native API/ABI contracts.

See [Development Status](development-status.md) and [Stability](stability.md).
