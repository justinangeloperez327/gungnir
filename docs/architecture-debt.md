# Architecture Debt

This document records known limitations that should remain visible before a stable release.

- Dependency-injection scoped state and resolution tracking need coroutine-safe ownership.
- Suspended route handlers and `sleep_for` deadlines now share the HTTP platform readiness loop while the server is active; due continuations are dispatched through the bounded executor pool. Request cancellation propagates into database connection execution through scoped driver cancellation hooks. PostgreSQL and SQL Server implement native in-flight interruption; MySQL and MongoDB remain cooperative at the connection boundary, and filesystem/storage operations are not yet uniformly cancellation-aware.
- The HTTP server does not yet establish production-grade HTTP/2, TLS, WebSocket or asynchronous streaming guarantees.
- Database runtime state must avoid unsafe global or thread-local assumptions when coroutines can migrate threads.
- PostgreSQL, MySQL, SQL Server, and MongoDB have concrete client-library adapters. MongoDB replica-set transaction/session support remains incomplete.
- Parent-scoped ORM relationship queries cover direct, pivot and through relations. Many-to-many attach/detach/sync now execute real pivot mutations with transactional sync and rollback coverage. Broader concurrent relationship execution coverage remains incomplete.
- Session lifecycle/cookie integration, secure identifiers and regeneration cleanup remain incomplete.
- Cache, queue, mail and notification production adapters are not supplied by their in-memory contracts.
- View runtime global engine state and filesystem symlink containment require hardening.
- Storage path containment must account for symlinks; writes are not guaranteed atomic.
- Events currently use named polymorphic events rather than a fully typed dispatch surface.
- Queue visibility, leasing, crash recovery, delayed jobs and worker lifecycle need production contracts.
- Scheduler provides interval execution, not cron/timezone/distributed locking semantics.
- Logging provides structured records and sinks, not tracing/metrics exporters.
- Plugin compatibility expressions are not yet parsed or enforced.
- Generated CLI scaffolding must remain synchronized with source-language lowering and runtime contracts.

These are release blockers only when the intended release claims the affected capability. They must not be hidden by documentation or marketing.
