# Scheduler

Gungnir's scheduler defines deterministic in-process schedules without pretending to provide distributed coordination.

## Tasks

`Scheduler::every()` registers a named action with a positive interval. Names must be unique within a scheduler.

`run_due()` evaluates all tasks against the injected clock and runs those that are due. A task is marked as run only after its action completes successfully. Exceptions propagate to the caller.

## Clock

Time is provided through the `Clock` interface. Production applications can use `SystemClock`; tests can provide deterministic clocks.

## Process lifecycle

The scheduler does not create detached threads. A command, worker, service manager or application runtime should decide how often `run_due()` is called and how shutdown is handled.

## Distributed deployments

This scheduler does not claim distributed no-overlap guarantees. Multiple application instances can run the same due task. Distributed single-run or no-overlap behavior requires a shared locking backend with explicit lease and failure semantics.

## Cron

Cron syntax is intentionally not included until a complete parser and timezone/DST policy exist. Fixed intervals are preferable to a partial cron implementation.
