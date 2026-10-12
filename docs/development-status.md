# Release and Source Status

**Published version:** Gungnir v1.0.0, released **5 October 2026**.
The downloads on [GitHub Releases](https://github.com/justinangeloperez327/gungnir/releases)
contain the core SDK described in [Version 1.0.0](release-v1.md).

**Current `main`:** Later features through
[PR #193](https://github.com/justinangeloperez327/gungnir/pull/193)
were merged by **12 October 2026**. These are **not included in the
published v1.0.0 assets**. The source package/version identity remains
`1.0.0`; a matching version string does not mean matching capabilities.
Published release assets have not been republished.

## Capability availability

| Area | Published v1.0.0 artifacts | Later merged `main` source |
| --- | --- | --- |
| Structured language/compiler, routing, HTTP, ORM and framework services | Included within the [release scope](release-v1.md) | Preserved and extended |
| SQLite and password support | Not bundled into the core SDK; requires custom build | Bundled application SDK profile (PRs #186–187) |
| Typed `.gnr` mail and notification sending/queueing | Outside the published generated-app contract | Integrated and accepted (PR #185) |
| Full database application acceptance | No bundled multi-database SDK | SQLite, PostgreSQL, MySQL, SQL Server and MongoDB acceptance (PR #188) |
| Shared cache coordination and Redis quotas | Later integration not shipped | Owned locks and shared rate limiting (PR #189) |
| Generated-app external telemetry | Later integration not shipped | OTLP and correlated collector acceptance (PR #190) |
| Linux reference deployment and recovery | Not in the published assets | Tested installed example (PR #191) |
| Live HTTP/ORM application load fixture | Not in the published assets | Installed load and durability verification (PR #192) |
| Blade-like `.spear` templates | Published views use the earlier HTML grammar | Spear parsing/rendering, caching and installed acceptance (PR #193) |

This table distinguishes **availability**, not production certification of
every combination. Redis, SMTP, TLS, OTLP and non-SQLite database adapters
still require optional build flags, native dependencies and externally
configured services. Read the relevant guide before deployment.

## Which SDK should I use?

- **Published core:** Install a v1.0.0 release asset, then follow
  [Getting Started](getting-started.md) and
  [Release and Installation](release-v1.md).
- **Post-release language or app services:** Build the appropriate SDK from
  current `main`; see [SDK Packages](sdk-packages.md), [Views](view.md)
  and [Mail](mail.md).
- **Optional native adapters:** Follow [Database](database.md),
  [Cache Coordination](cache-coordination.md),
  [Observability](external-observability.md) or
  [Linux Deployment](production-deployment.md), and provision dependencies.

## Compatibility identity

```text
package_version=1.0.0
language_version=1.0
compiler_contract=1.0
diagnostic_contract=1.0
structured_feature_freeze=true
compatibility=stable
native API contract=1.0
native ABI epoch=1
release channel=stable
```

Applications should regenerate and rebuild generated C++ with the matching
compiler/runtime SDK. Later source additions do not retroactively change
the tagged v1.0.0 compatibility or binaries.

The [Framework Completeness Audit](framework-completeness-audit.md) is a
historical pre-release baseline, not a current TODO list. Acceptance evidence
and contract details are in [Engineering Audits](../engineering/README.md),
[Testing](testing.md), [Stability](stability.md) and
[Upgrading](upgrading.md).
