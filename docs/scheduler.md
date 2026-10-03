# Scheduler

> **Status: Development.** This guide describes the current implementation and documented limits. The 1.0 compatibility contract is not frozen yet.

## Current behavior

Native Scheduler registers interval tasks with `every` and cron tasks with `cron`; convenience methods include hourly/daily/weekly/monthly schedules. Task callbacks are synchronous `void()` actions.

Task exposes timezone selection, `without_overlapping` and `on_one_server` policies. Lock-backed coordination requires an appropriate LockStore; the optional Redis lock store enables shared coordination.

## Limits and planned work

Keep callback dependencies and timezone objects alive. In-memory locks cannot coordinate separate hosts. Scheduling is not automatic `.gnr` lowering or durable queue delivery; choose the runtime host and lock policy explicitly.

## Implementation references

- [include/gungnir/scheduler/scheduler.hpp](../include/gungnir/scheduler/scheduler.hpp)
- [include/gungnir/scheduler/task.hpp](../include/gungnir/scheduler/task.hpp)
- [include/gungnir/scheduler/cron.hpp](../include/gungnir/scheduler/cron.hpp)
- [include/gungnir/scheduler/redis_lock.hpp](../include/gungnir/scheduler/redis_lock.hpp)

See the [documentation index](README.md), [getting started](getting-started.md), and [target design](design/scheduler.md).
