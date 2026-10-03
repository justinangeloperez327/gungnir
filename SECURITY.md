# Security Policy

## Supported line

Gungnir is under active development. Security defects remain high-priority correctness issues and must be resolved before the eventual 1.0 release.

Security fixes may require behavior changes when preserving prior behavior would leave a material vulnerability. Such changes should be documented explicitly.

## Reporting a vulnerability

Do **not** publish exploit details in a public issue.

Use GitHub's private vulnerability reporting/security advisory mechanism for this repository when available. Include:

- affected Gungnir version or commit;
- affected platform/toolchain;
- minimal reproduction;
- expected security boundary;
- observed behavior;
- impact;
- whether the issue is remotely reachable;
- any known workaround.

If private repository security reporting is unavailable, avoid posting exploit details publicly until a private contact path is available through the repository owner.

## Scope

Relevant reports include vulnerabilities in:

- HTTP parsing and request framing;
- headers, cookies, CORS, CSRF, sessions, authentication, and authorization;
- trusted-proxy handling;
- compiler crashes that indicate memory-safety defects;
- generated-code safety boundaries;
- database/ORM transaction or isolation defects with security impact;
- path traversal, storage, upload, template escaping, or secret exposure;
- installer/package integrity;
- denial-of-service through unbounded framework resource behavior.

Deployment misconfiguration alone is not necessarily a framework vulnerability, but documentation that encourages an unsafe configuration should still be reported.

## Security baseline

See:

- [Security](docs/security.md)
- [Security Hardening](docs/security-hardening.md)
- [Production Resilience](docs/production-resilience.md)
- [Native API and ABI Stability](docs/native-api-abi.md)

Gungnir does not claim a blanket security certification.
