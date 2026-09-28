# Architecture Debt

This document records known limitations that should remain visible before a stable release.

- Suspended route handlers and `sleep_for` deadlines now share the HTTP platform readiness loop while the server is active; due continuations are dispatched through the bounded executor pool. Request cancellation propagates into database connection execution through scoped driver cancellation hooks. PostgreSQL and SQL Server implement native in-flight interruption; MySQL and MongoDB remain cooperative at the connection boundary. Storage operations expose cancellation-aware overloads with buffered LocalDisk checkpoints; other filesystem-backed framework components are not yet uniformly cancellation-aware.
- The HTTP server does not yet establish production-grade HTTP/2, TLS, WebSocket or asynchronous streaming guarantees.
- Database runtime manager publication is atomic and transaction-scoped connection overrides are internal, thread-local, and restricted to synchronous thread-affine transaction closures. Async transactions remain unsupported until coroutine-local execution context and suspension-safe connection ownership are implemented.
- PostgreSQL, MySQL, SQL Server, and MongoDB have concrete client-library adapters. MongoDB replica-set transaction/session support remains incomplete.
- Parent-scoped ORM relationship queries cover direct, pivot and through relations. Many-to-many attach/detach/sync now execute real pivot mutations with transactional sync and rollback coverage. Broader concurrent relationship execution coverage remains incomplete.
- Session request lifecycle, cookie integration, CSPRNG identifiers, flash aging, regeneration, stale-ID cleanup, request-owned session authentication and session-bound CSRF protection are implemented. Durable/distributed session stores, expiry and garbage collection remain incomplete.
- Cache, queue, mail and notification production adapters are not supplied by their in-memory contracts.
- View runtime global engine state and filesystem symlink containment require hardening.
- Local storage rejects existing symlink traversal, canonicalizes containment, uses exclusive same-directory temporary files and atomically replaces completed writes. Race-free containment against concurrent hostile filesystem mutation and full crash-durable directory commits remain separate hardening work.
- Events currently use named polymorphic events rather than a fully typed dispatch surface.
- Queue visibility, leasing, crash recovery, delayed jobs and worker lifecycle need production contracts.
- Scheduler provides interval execution, not cron/timezone/distributed locking semantics.
- Logging provides structured records and sinks, not tracing/metrics exporters.
- Plugin compatibility expressions are not yet parsed or enforced.
- Generated CLI scaffolding must remain synchronized with source-language lowering and runtime contracts.

These are release blockers only when the intended release claims the affected capability. They must not be hidden by documentation or marketing.
