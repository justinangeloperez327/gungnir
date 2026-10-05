# Typed mail and notification delivery

Baseline: published v1.0.0 source `adb6f4f27b7c6023634db50455f30ab22bba861b`.
This additive source change completes the generated delivery handoff retained in
the [release audit](release-v1-audit.md). It does not replace published artifacts
or change version/API/ABI metadata.

| Contract | Acceptance path |
| --- | --- |
| Typed mail | Inject `Mail`; owned `PendingMail` supports addresses, reply-to, sender override and binary attachments. `send` accepts a non-optional mail declaration and invokes the existing instrumented native Mailer/Transport. |
| Notification addressing | Inject `Notifications`; recipient type must match `via`. Mail reads initialized string email; other channels use the model primary key. Bootstrap can register type/channel-specific routes. Hidden attributes remain available for internal routing while payload serialization respects visibility. |
| Channels | Reuse Manager, MailChannel and DatabaseChannel. The facade resolves routes and checks all configured channels before delivery. Selected mail/database/custom payloads are snapshotted. Missing payloads, duplicate channels and invalid mail envelopes fail before effects. |
| Queues | Versioned JSON snapshots hold composed data, routes and binary-safe attachment encoding. Native worker handlers resolve services through the worker's owning application, with a new dependency scope per attempt and weak context ownership. Queue/transaction ordering and retry/failed-job handling use the existing dispatcher/driver/worker. |
| Template composition | Existing mail `content()` uses the configured view engine; escaped template values execute in the application context. HTML strings retain their explicit application-controlled contract. |
| Canonical compiler | Typed calls, optional/wrong recipient/source types, arity, attempt ranges and opaque-handle serialization are checked by ProgramValidator and lowered through structural C++ IR. Validation-only and emission diagnostics/spans are compared. |
| Installed application | The installed CLI links optional SMTP and generates ordinary model/mail/notification/controller modules. Redis publication survives producer exit; fresh workers deliver through loopback libcurl SMTP, SQLite database storage and a custom channel. SMTP failure attempts survive worker restarts and administrative retry. |
| Platforms | Generated delivery participates in GCC/Clang/MSVC execution and deterministic C++/validated AST/IR comparison. Installed consumers and isolated public headers exercise the source SDK. |

Acceptance entry points are `tests/structured_delivery.cpp`,
`tests/fixtures/structured/delivery.gnr`, `tests/delivery_redis.py`, and
`tests/docs_delivery_examples.py`. CI includes the installed-package consumer
and the combined Redis/SMTP/SQLite application test.

Local GCC 13 Debug acceptance passed all 11 focused regressions, eight public
delivery examples, isolated public headers, the installed SDK smoke/delivery
consumers, and the fresh installed Redis/SMTP/SQLite application. Workflow step
schemas, documentation/release consistency and whitespace checks passed.
Remote exact-head compiler/platform/package results are supplied by the PR gates.

Delivery is at least once. Notification retries repeat the channel sequence;
partial success can be duplicated. There is no transactional outbox or atomic
multi-channel delivery. SMTP acceptance is loopback protocol/MIME delivery,
not a claim about every external TLS/authenticated provider. Deployed Redis
durability, mailbox credentials, SQL schema and custom-channel deduplication
remain application configuration. Published v1.0.0 assets remain unchanged.

Main-branch development with the same published version skips automatic release
publication. Explicit tag/manual releases retain pinned-source and immutable-tag
checks; existing versioned artifacts cannot be overwritten.
