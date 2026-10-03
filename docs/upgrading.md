# Upgrading Gungnir

> **Current line: 1.0.0-rc.1.** The 1.0 language/compiler/native API contracts are frozen. Pin the exact RC in production evaluation environments and review [Stability](stability.md) plus [Release Candidate](release-candidate.md) before upgrading.

## From 0.9.0 to 1.0.0-rc.1

The accepted structured language profile is promoted from the stabilized 0.9 behavior into the 1.0 contract. Phase 19 does not intentionally add new syntax.

Contract identifiers change:

| Contract | 0.9.0 | 1.0.0-rc.1 |
| --- | ---: | ---: |
| Structured language | 0.9 | 1.0 |
| Compiler semantics | 0.9 | 1.0 |
| Diagnostics | 0.9 | 1.0 |
| Native C++ API | 0.9 | 1.0 |
| Native ABI epoch | 0 | 1 |

Because compiler/runtime contract identifiers change, regenerate and rebuild all generated C++ with the RC package.

Native applications should also be rebuilt for ABI epoch 1.

## Recommended upgrade procedure

1. Read [CHANGELOG.md](../CHANGELOG.md).
2. Install the exact Gungnir RC package.
3. Verify:

   ```sh
   gungnir --version
   gungnirc --version
   gungnirc --print-contract
   ```

   RC1 should report `1.0.0-rc.1`, language/compiler/diagnostic contract `1.0`, feature freeze `true`, and compatibility `stable`.

4. Run semantic validation:

   ```sh
   gungnirc <entry>.gnr --check
   ```

5. Regenerate/rebuild all generated C++.
6. Run the application's native build and full test suite.
7. Run migration planning before applying database migrations.
8. Re-run workload-specific security, integration, and performance tests for production deployments.

## RC to stable 1.0.0

The intended RC-to-stable transition changes release identity only:

```text
1.0.0-rc.1 -> 1.0.0
```

The language, compiler, diagnostic, native API, and ABI epoch contracts should remain at 1.0 / epoch 1.

If a release-blocking fix requires a contract change, it must be documented before stable promotion.

## Native binary compatibility

ABI epoch equality is necessary but not sufficient. Rebuild native applications when changing compiler family, standard library ABI, runtime model, architecture, linked dependency ABI, or incompatible build options.

See [Native API and ABI Stability](native-api-abi.md).
