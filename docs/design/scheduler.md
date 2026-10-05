# Scheduler

> **Future design specification.** This document describes intended long-term architecture beyond the published `1.0` contract. Examples and requirements may exceed the current implementation. See the [current implementation guide](../scheduler.md) before using an API.


## Release alignment

This document describes possible future architecture beyond 1.0. Current implementation guides and executable tests define the published contract.

Gungnir's scheduler runs named application tasks according to interval or cron schedules.

Scheduling is a runtime service, not a language control-flow feature.

# Tasks

A scheduled task has:

~~~text
stable name
schedule
action/handler
timezone
overlap policy
optional distributed lock policy
~~~

Names should be unique within one scheduler configuration.

# Interval schedules

Interval scheduling runs a task after a defined duration cadence.

Intervals must be positive and use a monotonic timing source where appropriate.

# Cron schedules

The scheduler may support standard five-field cron:

~~~text
minute hour day-of-month month day-of-week
~~~

Supported cron syntax should be documented and tested explicitly.

Do not silently accept unsupported cron extensions.

# Timezones

A scheduler has an explicit timezone policy.

UTC is a safe default.

Timezone handling must define daylight-saving behavior.

Fixed offsets are not a complete replacement for named regional timezone rules.

# Execution

The scheduler evaluates due tasks against an injected clock.

A task should be marked complete for that occurrence only after its handler finishes according to the scheduler contract.

Failures should be observable and should not silently count as successful execution.

# Production runner

The long-running scheduler runtime should:

- sleep until the next relevant deadline;
- wake on cancellation;
- avoid busy polling;
- stop scheduling new tasks during shutdown;
- drain the currently owned task according to policy.

# Overlap

Overlap behavior must be explicit.

Possible policies include:

~~~text
allow overlap
prevent overlap in process
prevent overlap using distributed lock
~~~

An in-process lock only protects one process.

# Distributed execution

In multi-instance deployments, single execution requires shared coordination.

A distributed lock must define:

- owner identity/token;
- TTL/lease;
- renewal;
- safe release;
- failure behavior.

Do not claim cluster-wide single execution from a local mutex.

# Async tasks

A scheduled handler may be async.

The scheduler owns its lifecycle and cancellation.

Async suspension does not by itself provide distributed coordination.

# Queue integration

Long or retryable scheduled work may dispatch a queue job rather than execute all work inline.

The scheduler and queue remain separate subsystems.

# Clock abstraction

Tests should use an injectable/fake clock so schedules can be tested deterministically.

Production uses a system clock plus configured timezone rules.

# DST behavior

For regional timezones, the scheduler must define behavior for:

- skipped local times during spring-forward;
- repeated local times during fall-back.

The same scheduled occurrence must not run unpredictably twice unless that is the explicit policy.

# Observability

Record task name, scheduled time, start/end, duration, outcome, and overlap/lock decisions where useful.

# Shutdown

Scheduler shutdown participates in the application lifecycle.

It should stop launching new tasks, signal cancellation, and drain the active owned task within configured policy.

# Design rule

~~~text
scheduler decides when
handler decides what
queue may own deferred execution
distributed coordination is explicit
time semantics are deterministic
~~~

