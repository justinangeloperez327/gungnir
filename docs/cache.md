# Cache

> **Status: Development.** This guide describes the current implementation and documented limits. The 1.0 compatibility contract is not frozen yet.

## Current behavior

Native Repository wraps a Store. `get` returns an optional string; `put` accepts a string with optional TTL; `has`, `forget`, `flush` and `remember` provide convenience operations. MemoryStore is process-local; RedisStore uses the optional Redis adapter.

## Limits and planned work

Values are strings, not arbitrary typed objects. `remember` is a get/factory/put sequence and does not provide a distributed lock or single-flight guarantee. Application code owns serialization. Enable `GUNGNIR_WITH_REDIS` and link `gungnir::redis` for Redis integration.

## Implementation references

- [include/gungnir/cache/repository.hpp](../include/gungnir/cache/repository.hpp)
- [include/gungnir/cache/store.hpp](../include/gungnir/cache/store.hpp)
- [include/gungnir/cache/memory_store.hpp](../include/gungnir/cache/memory_store.hpp)
- [include/gungnir/cache/redis_store.hpp](../include/gungnir/cache/redis_store.hpp)

See the [documentation index](README.md), [getting started](getting-started.md), and [target design](design/cache.md).
