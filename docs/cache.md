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

## Redis

Gungnir provides an optional hiredis-backed Redis store. It is not linked into the core target unless explicitly enabled:

```sh
cmake -S . -B build \
  -DGUNGNIR_WITH_REDIS=ON
```

Link applications that use it against `gungnir::redis`:

```cpp
#include <gungnir/cache/redis_store.hpp>

gungnir::cache::RedisSettings settings;
settings.host = "127.0.0.1";
settings.port = 6379;
settings.database = 0;
settings.prefix = "my-app:";

gungnir::cache::RedisStore store{
    settings
};

gungnir::cache::Repository cache{
    store
};
```

`RedisSettings` supports host, port, optional username/password authentication, logical database selection, key prefixes, connect timeout and command timeout.

The adapter uses binary-safe hiredis argv commands, native Redis expiration, reconnects after transport failure, and serializes access to the synchronous hiredis context because a context is not safe for concurrent command use.

A non-empty prefix scopes `flush()` to keys owned by that prefix. With an empty prefix, `flush()` intentionally maps to `FLUSHDB` and clears the selected Redis logical database.

The current adapter is a real single-node Redis implementation, but it does **not** yet claim Redis Cluster routing, TLS transport, Sentinel discovery, connection pooling or asynchronous hiredis execution. Those capabilities should be added explicitly rather than hidden behind the basic `Store` contract.

## Other production stores

Memcached and database-backed distributed cache adapters are not yet supplied. They require concrete clients and operational semantics rather than placeholder APIs.

## Flush

`flush()` clears the selected store. Applications should namespace cache keys when a shared production cache contains data from multiple applications or environments.
