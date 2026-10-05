# Upgrading to 1.0.0

Install the new SDK/CLI together and rebuild existing applications. Historical preview packages such as 0.9.0 use older language/runtime contracts and should not be mixed with 1.0 headers or libraries.

1. Pin the released artifact and verify its SHA-256 checksum.
2. Check `gungnir --version`, `gungnirc --version` and `gungnirc --print-contract`.
3. Back up application data and preserve `.env`, bootstrap configuration and source files.
4. Run strict source checks and `gungnir build`; resolve diagnostics before deployment.
5. Run application tests and preview migration plans before applying migrations.

Version 1.0.0 reports language/compiler/diagnostic/native API contract 1.0 and ABI epoch 1. Regenerate generated C++ with the matching SDK. Review the [release scope](release-v1.md) for optional adapters and pending generated delivery APIs.

Use a separate build directory when changing toolchains or adapter features. Native code must also be rebuilt if ABI-relevant platform, standard library, runtime or dependency settings change.
