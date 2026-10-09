# Cache

Inject `Cache` into a controller, middleware or handler to use the configured cache repository. Cache stores JSON-compatible values, including scalars, arrays, objects and visible model data. Service handles and callbacks are not cache values.

## Basic operations

```gnr
controller SummaryController {
    inject Cache cache;

    store(Request request) {
        const summary = request.json();
        cache.put("dashboard:summary", summary, 300);
        return noContent();
    }

    index(Request request) {
        const summary = cache.get("dashboard:summary");
        if (summary != null) { return json(summary); }
        return noContent();
    }
}
```

`get` returns `Json?`: a missing entry is absent. A stored JSON null is still an entry, so `has` returns true for it. Returned values own their data. Optional values are encoded as their value or JSON null, and model visibility rules apply before storage.

```gnr
function void clearSummary(Cache cache) {
    cache.forget("dashboard:summary");
    cache.flush();
}
```

`forget` returns whether an entry was removed. `flush` removes entries in the configured store.

## Remember

`remember` retrieves an existing entry or invokes a synchronous factory with no parameters, stores its JSON-compatible result, and returns it:

```gnr
controller ReportController {
    inject Cache cache;

    index(Request request) {
        const summary = cache.remember("dashboard:summary", 300, () => {
            return {"count": 42, "ratio": 3.0};
        });
        return json(summary);
    }
}
```

The factory can capture injected services and call declared functions. A factory failure does not store its result. Concurrent misses may each invoke the factory.

## Stores and configuration

Register cache and other adapters in the native application bootstrap before resolving generated controllers:

```cpp
Application app;
app.provider<ServicesProvider>(ServiceOptions{
    .cache = std::make_shared<cache::MemoryStore>()
});
app.boot();
```

Gungnir provides an in-memory store for development and tests and Redis-backed caching for shared deployments. `ServicesProvider` binds the canonical `Cache` service and retains its configured adapter. Native clients sharing its keys use JSON encoding for values; the native string repository remains available for direct use.

## Expiration

Lifetimes are whole seconds. Omitting the lifetime in `put` stores without an expiration. Zero expires immediately; negative or unrepresentable lifetimes are rejected before writing. Applications choose keys and lifetimes according to the consistency requirements of the data.

## Locks

`cache.lock(key, ttlMilliseconds)` returns a `CacheLock` handle. Acquisition is non-blocking: `acquire()` returns false when another owner holds the key. `renew()` extends the current lease using its original lifetime; `renew(milliseconds)` selects a new positive lifetime. `release()` removes only the current owner's lease and returns false for a missing, expired or replaced lease.

```gnr
controller ExportController {
    inject Cache cache;
    generate() {
        const lock = cache.lock('reports:export', 30000);
        if (!lock.acquire()) { return text('Export is busy', 409); }
        // Perform a bounded operation while the lease is active.
        return text('Export complete');
    }
}
```

Copies share ownership, including handles passed to functions or retained across `await`. The last handle releases the lease automatically, also when an exception unwinds the operation. Explicit release reports backend errors; destructor cleanup suppresses them and relies on expiration. Lock handles cannot be serialized into cache entries, models or job payloads.

Configure `ServiceOptions.cache_locks` separately from the cache value store. Generated development bootstraps use `cache::MemoryLockStore`, which coordinates only clients sharing that object in one process. An unconfigured lock store raises a configuration error. Shared applications can reuse the existing owner-checked Redis lease adapter through `<gungnir/cache/redis_lock.hpp>`:

```cpp
cache::RedisSettings values;
values.prefix = "myapp:cache:values:";
cache::RedisLockSettings locks;
locks.host = values.host;
locks.port = values.port;
locks.database = values.database;
locks.prefix = "myapp:cache:locks:";
ServiceOptions services;
services.cache = std::make_shared<cache::RedisStore>(values);
services.cache_locks = std::make_shared<cache::RedisLockStore>(locks);
app.provider<ServicesProvider>(std::move(services));
```

Use distinct, non-overlapping prefixes for values, locks, sessions and scheduler leases. `cache.flush()` affects the configured value namespace; it preserves locks in their separate namespace. Prefixes are literal bytes, including Redis glob characters. Redis cache and lease adapters use their configured single endpoint, credentials, database and timeouts; they do not use the topology/TLS options of the separate `redis::Client` API.

## Coordinated factories

`rememberLocked(key, seconds, leaseMilliseconds, factory)` uses the configured lock store to coordinate cache misses. It checks for a cached value, acquires `remember:` plus the key, checks again, and invokes the synchronous factory only on a remaining miss. Stored JSON null is a cache hit.

```gnr
controller CoordinatedReportController {
    inject Cache cache;
    index() {
        return json(cache.rememberLocked('dashboard:summary', 300, 30000, () => {
            return {count: 42, ratio: 3.0};
        }));
    }
}
```

Contention raises native `cache::LockUnavailable` immediately; this helper does not wait or run a second factory while the lease is held. A factory or serialization failure releases the lease and stores no result. Ownership is checked by renewal before publication, and an observed lost lease raises `cache::LockLost`. A later call can retry after the winner fills the cache or its lease expires.

These are expiring leases, not fencing tokens or a transaction combining locks and values. Choose a lease lifetime covering the factory and publication; expiration, eviction or backend failover can admit another owner while original work continues. External side effects need their own transaction, idempotency or fencing. Cache publication and owner-checked release are separate operations; a backend failure after publication may leave a filled entry even though the caller receives an error. See [Distributed coordination](cache-coordination.md) for deployment and acceptance details.

Cache is an optimization and coordination service; application correctness should not depend on stale cached data being impossible.
