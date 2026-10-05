# Events, queues and scheduling contracts

This follow-up starts from merged PR #181. It connects `.gnr` applications to the
existing native dispatcher, queue drivers/worker, scheduler, locks and runtime
lifecycle. It adds no parser phase, source scanner, alternate lowerer or version
change. The historical product baseline remains in [the baseline audit](product-contract-audit.md).

| Contract | Application path and acceptance |
| --- | --- |
| Typed events | Inject `Events`; dispatch accepts non-optional events. Async dispatch requires `await` and owns its event/dispatcher through real suspension. |
| Listeners | Generated registration resolves injections; constant `priority` defaults to zero. Tests verify descending order and native preflight rejection of synchronous dispatch to async listeners. Shutdown releases registrations to break injected `Events` ownership cycles. |
| Job publication | Inject `Queue`; construct immutable data payloads without resolving producer services. Worker reconstruction resolves injections from its owning application. Compiler rejects producer handler/injection access. |
| Attempts and delays | Typed attempt counts and millisecond delays use existing driver operations, with literal diagnostics and runtime range checks. Worker options remain native bootstrap configuration. |
| Transaction ordering | Generated publication is held until a real SQLite commit; rollback discards it. Explicit immediate publication bypasses deferral. This is not an outbox. |
| Failed jobs | Inspect metadata without payload disclosure, retry with reset attempts, or forget failures through the configured driver. Memory acceptance covers malformed payloads, backoff and retry success. |
| Scheduling | `routes.console::schedule(Scheduler)` is validated in the canonical project and invoked during boot. Intervals, cron and common frequencies accept jobs or synchronous zero-argument void callbacks. |
| Ownership | Service values retain native adapter owners. Scheduled task handles retain scheduler/clock owners; native tasks own selected IANA time zones. Callbacks cannot capture their scheduler/task handle. |
| Locks | Use the existing lock store and task semantics. Memory tests cover shared-store occurrence exclusion; the Redis app checks locks across fresh scheduler processes. Job overlap covers publication, not handler execution. |
| Lifecycle | Installed CLI `queue:work [--once]`, `schedule:run`, and `schedule:work` boot and activate the same generated application. Continuous runners use native cancellation and signals, complete current work, then shut down without an HTTP listener. `RuntimeHost` acceptance covers integrated startup, queued execution and shutdown. |
| Durable application | An installed generated Redis application publishes over HTTP, exits, and executes its payload in a separate worker process. Failure attempts survive successive worker processes; administrative retry succeeds after services are reconstructed. |

`tests/structured_background.cpp` runs generated C++ from the canonical fixture
and compares validation-only versus emission diagnostics. Installed SDK consumers
run the same harness. `tests/generated_project.py` exercises normal `.gnr`
event/job/controller/console modules through the installed CLI, including invalid
schedule signatures before generated-output mutation, worker/scheduler commands,
live HTTP and continuous-runner SIGTERM. `tests/background_redis.py` uses explicit
Redis bootstrap adapters, separate native processes and shared storage.

The PR gate includes Redis application acceptance and the existing live Redis
lease/retry/lock tests. GCC, Clang and MSVC compile/run the generated background
fixture and compare its validated AST, structural C++ IR and emitted C++ snapshots.
All public `.gnr` examples in the four guides are checked strictly.

Memory queues and locks are process-local. Separate processes require shared
adapters. UTC is portable; named zones require provisioned IANA data (`TZDIR` on
systems without a default zoneinfo directory). Calendar/DST behavior continues to
use the native scheduler tests. Continuous interval runners retain last-run state;
one-shot interval commands start with fresh state. No automatic HTTP background
threads, exactly-once delivery, transactional outbox, cancellation preemption,
mail/notification acceptance, or general production-completeness claim is added.
