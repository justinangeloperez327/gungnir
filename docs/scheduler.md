# Scheduler

> **Status: experimental, pre-1.0.** This page describes the current implementation. Native C++ APIs and `.gnr` syntax are identified separately. Proposed contracts are in the [design specification](design/scheduler.md).

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
