# Storage

> **Status: experimental, pre-1.0.** This page describes the current implementation. Native C++ APIs and `.gnr` syntax are identified separately. Proposed contracts are in the [design specification](design/storage.md).

## Current behavior

Manager registers named Disk instances and selects a default disk. The native Disk contract uses `exists`, `get`, `put`, `remove`, `move`, `copy`, `size` and `files`. `get` returns an optional string; LocalDisk roots object paths under a configured directory.

Cancellation overloads check cancellation around operations. Keep paths logical and let the adapter resolve them.

## Limits and planned work

The public names are not `read`/`write`/`delete`/`list`. There is no universal remote object-storage implementation implied by Disk. Cancellation checks around synchronous operations do not guarantee interruption of a blocked filesystem call or undo a completed write.

## Implementation references

- [include/gungnir/storage/disk.hpp](../include/gungnir/storage/disk.hpp)
- [include/gungnir/storage/local_disk.hpp](../include/gungnir/storage/local_disk.hpp)
- [include/gungnir/storage/manager.hpp](../include/gungnir/storage/manager.hpp)

See the [documentation index](README.md), [getting started](getting-started.md), and [target design](design/storage.md).
