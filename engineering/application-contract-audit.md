# Application integration contracts

This audit follows merged PR #182. The work completes views, bound-resource
authorization, persistent authentication, typed configuration and dependency
scopes, and application observability through the canonical structured pipeline.
Existing native adapters and the editable bootstrap surface are reused.

| Contract | Acceptance evidence |
| --- | --- |
| Views | Generated rendering covers layouts, sections/yields, partials, components/slots, conditions, loops/metadata, helpers, escaped/raw values, model visibility, unsigned 64-bit values, locale-independent decimals, recursion, root escapes, and output limits. |
| Resource authorization | Generated model policies install a default ORM actor lookup without replacing explicit mappings. Bound routes exercise anonymous 401, denied/unresolved actor 403, authorized views, and missing resources 404 using SQLite. Scalar, optional and service resources fail canonical validation. |
| Persistent authentication | The installed application migrates SQLite and uses native password providers, Redis sessions, and Redis remember tokens. Sessions survive restart and work across independent servers. Concurrent token recall authenticates exactly one process; replay and logout revocation fail. Redis inspection verifies digest keys, expiry and absence of raw browser tokens/passwords in session records. |
| Store failures | Token publication precedes authentication state changes. A failing store leaves the guest session and response unchanged. Redis adapter acceptance covers expiry, revoke, namespace isolation and independent-client consume. |
| Configuration | Config retains the native repository, preserves missing versus stored null, and exposes typed getters/defaults. Invalid arguments fail the same canonical validation-only/emission path. Native conversion rejects nonfinite and out-of-range numbers. |
| Dependency scopes | Both generated factories and installed CLI assembly preserve request scopes. Controllers and awaited middleware share a request dependency; successive requests differ. Each job execution gets a new scope. A deliberately suspended middleware retains owners after its Application leaves scope. |
| Observability | Logger and Telemetry retain native owners. Explicit exporters belong to application execution contexts. Installed HTTP/database/custom spans, metric/log trace correlation, scalar attribute validation and redaction are verified. Independent applications have isolated exporters. Failing log sinks do not prevent later sinks; flush failures still attempt shutdown. |
| Process lifecycle | Generated HTTP uses native SignalWatcher and cancellation, drains the listener, and calls application shutdown. The installed app verifies SIGTERM exits successfully and tracing/metric sinks receive cleanup. |
| Compiler and packaging | The application fixture participates in GCC/Clang/MSVC compilation and deterministic validated/IR/C++ snapshot comparison. Public headers and the installed SDK compile the new APIs. All 13 language examples in the six application guides are checked strictly. |

`tests/structured_application.cpp` uses emitted code and real native services;
`tests/application_redis.py` builds an installed CLI application using ordinary
modules, migrations and templates. Its native bootstrap configures adapters,
providers and sinks; generated C++ is never manually repaired. Redis acceptance
is conditional on the explicit adapter, SQLite and password build flags.

## Release handoff

Local verification completed with all 105 configured checks passing, including
live Redis and both installed application processes. Eleven installed SDK
consumers passed against the same build. Public header isolation, documentation
integrity, development metadata and workflow YAML checks also passed. GitHub
platform and packaging jobs verify the published PR head separately.

This work does not change development version/API/ABI metadata. Retain the
existing security, platform, optional-adapter, packaging and installer gates.
The historical [product audit](product-contract-audit.md) and subsequent audits
must be read together: its old authentication/view/configuration findings are
superseded by this and earlier follow-ups.

Mail/notification generated delivery acceptance and the final release identity,
artifact matrix, dependency/license review and release automation remain
separate handoff requirements. Remote OTLP propagation, selected Redis topology
and durability configuration, workload/load testing, and operational deployment
checks require their corresponding environment. Model actors must use an
identity mapping compatible with their model key. Facade redaction does not
sanitize arbitrary messages or values under nonsensitive field names.
