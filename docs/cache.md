# Cache

Gungnir cache separates the application-facing repository from storage adapters.

`cache::Store` defines the persistence contract. `cache::Repository` provides the common API and `remember()`. `MemoryStore` is the built-in process-local adapter for development, tests and single-process workloads.

## Values and expiration

The core contract stores opaque strings. Serialization of models or structured application values should be explicit rather than hidden behind unsafe type erasure.

Entries may be stored without expiration or with a `cache::Duration` time-to-live. The memory adapter uses a monotonic clock for expiration.

## Remember

```cpp
auto value = cache.remember(
    "users.count",
    std::chrono::seconds{60},
    [] { return load_user_count(); }
);
```

`remember()` is intentionally a simple read-through operation. It does not claim distributed stampede protection. Production adapters that need atomic locks or single-flight behavior should expose those capabilities explicitly.

## Production stores

Redis, Memcached, database and distributed cache adapters require concrete clients and operational semantics. The framework does not claim those backends until real adapters exist.

## Flush

`flush()` clears the selected store. Applications should namespace cache keys when a shared production cache contains data from multiple applications or environments.
