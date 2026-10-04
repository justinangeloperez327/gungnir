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

Distributed cache stores can provide locks for operations that must be coordinated across application instances.

Cache is an optimization and coordination service; application correctness should not depend on stale cached data being impossible.
