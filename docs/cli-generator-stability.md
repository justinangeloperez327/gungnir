# CLI Generator Stability

The CLI must only generate source syntax that the Gungnir transpiler can lower into valid framework code.

`make:request` and `make:job` are currently guarded and report that they are unavailable. Earlier scaffolds emitted `ValidatedRequest` and `Job` classes even though dedicated source-language lowering for those framework bases is not implemented.

This guard prevents the CLI from creating files that appear supported but cannot satisfy the native runtime contracts.

The commands can be enabled again when:

- ValidatedRequest has a defined Gungnir source contract and lowering.
- Queue Job has a defined source contract that produces the required native job identity/payload behavior and execution integration.
- Generated fixtures are covered by transpile-and-compile tests.

Generator availability is a compatibility promise: generated code should be usable, not aspirational.
