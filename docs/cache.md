# Cache

Gungnir's cache service stores reusable values outside the primary database path.

## Basic operations

The cache API supports retrieving, storing, checking, forgetting, and flushing cached values.

```gnr
cache.put("dashboard:summary", summary, 300);
const summary = cache.get("dashboard:summary");
```

## Remember

Use `remember` to compute a value only when the cache entry is absent:

```gnr
const summary = cache.remember("dashboard:summary", 300, () => {
    return reports.summary();
});
```

## Stores

Gungnir provides an in-memory store for local development/tests and Redis-backed caching for shared deployments.

## Expiration

Cache entries can use explicit lifetimes. Applications should choose cache keys and lifetimes based on the consistency requirements of the underlying data.

## Locks

Distributed cache stores can provide locks for operations that must be coordinated across application instances.

Cache is an optimization and coordination service; application correctness should not depend on stale cached data being impossible.
