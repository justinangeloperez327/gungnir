# Storage

> **Status: Gungnir 1.0 current contract.** This page describes the current implementation. Native C++ APIs and `.gnr` syntax are identified separately. Proposed contracts are in the [design specification](design/storage.md).

## Current behavior

Manager registers named Disk instances and selects a default disk. The native Disk contract uses `exists`, `get`, `put`, `remove`, `move`, `copy`, `size` and `files`. `get` returns an optional string; LocalDisk roots object paths under a configured directory.

Cancellation overloads check cancellation around operations. Keep paths logical and let the adapter resolve them.

## Limits and planned work

The public names are not `read`/`write`/`delete`/`list`. Enable `GUNGNIR_WITH_S3=ON` (libcurl 7.75+) for `S3Disk`, a path-style S3-compatible adapter with SigV4 authentication, optional session credentials, bounded transfers/listing, timeouts and cancellation. Configure `S3Options` and register the instance with `Manager`. HTTPS is required by default; `allow_http` is an explicit local-test option. Listing follows continuation tokens. Copy downloads and uploads an object within the configured size limit; move performs copy then delete and is not atomic. Live service behavior still depends on the chosen S3-compatible endpoint. Cancellation checks around synchronous operations do not guarantee interruption of a blocked filesystem call or undo a completed write.

## Implementation references

- [include/gungnir/storage/disk.hpp](../include/gungnir/storage/disk.hpp)
- [include/gungnir/storage/local_disk.hpp](../include/gungnir/storage/local_disk.hpp)
- [include/gungnir/storage/manager.hpp](../include/gungnir/storage/manager.hpp)

See the [documentation index](README.md), [getting started](getting-started.md), and [target design](design/storage.md).
