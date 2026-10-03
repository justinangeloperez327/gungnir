# Support

Gungnir is currently in the 1.0 release-candidate stage. Support is provided through the repository and documentation; there is no guaranteed response-time SLA.

## Before asking for help

Check:

- [Getting Started](docs/getting-started.md)
- [Documentation Index](docs/README.md)
- [CLI and Code Generation](docs/cli-codegen.md)
- [Stability](docs/stability.md)
- [Production and Deployment](docs/production.md)

When troubleshooting a compiler issue, include:

```sh
gungnir --version
gungnirc --version
gungnirc --print-contract
```

Also include the operating system, compiler/toolchain, CMake version, exact command, and the smallest source example that reproduces the problem.

## Bug reports

Use a GitHub bug report when current documented behavior is incorrect, crashes, corrupts state, violates a compatibility contract, or behaves differently across a supported toolchain.

Do not use bug reports for design proposals or behavior documented only under `docs/design/`.

## Feature requests

Feature requests are welcome, but the structured language is feature-frozen for the 1.0 release-candidate line. New syntax or semantics may be deferred until the compiler contract is intentionally advanced.

## Security

Potential vulnerabilities should follow [SECURITY.md](SECURITY.md) rather than the public issue tracker.

## Production use

Gungnir 1.0.0-rc.1 is a release candidate, not the final stable release. Pin exact versions, review the [stability contract](docs/stability.md), run workload-specific tests, and use appropriate external supervision, TLS, database resilience, backup, monitoring, and capacity controls.
