# Pre-1.0 Development Status

Gungnir is **not a release candidate** at this stage.

The project is completing the full framework feature surface and consistency work before declaring 1.0. The package uses an internal development identity, and the language, compiler, diagnostics, native API, and ABI contracts remain open to compatible changes required to complete that surface.

## Development contract

- package identity: `0.0.0-dev`;
- structured language contract: `development`;
- compiler semantic contract: `development`;
- diagnostic contract: `development`;
- structured feature freeze: `false`;
- compiler compatibility: `experimental`;
- native C++ API contract: `development`;
- native ABI epoch: `0`.

## 1.0 rule

Do not promote or freeze these contracts merely because a numbered implementation phase is complete. Stable `1.0.0` is reserved for the point where the intended framework features are implemented, cross-component behavior is consistent, documentation matches the real APIs, and release gates are green.

Until then, new implementation work may extend or correct the public surface where needed to achieve that complete 1.0 baseline.
