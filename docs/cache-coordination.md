# Distributed coordination

The installed application SDK supports shared JSON cache values, explicit cache leases, Redis HTTP quotas, shared sessions and the existing queue/scheduler adapters when Redis support is built. Enable `GUNGNIR_WITH_REDIS=ON`, install hiredis and link `gungnir::redis`. Generated applications link the installed adapter automatically. The core/application release package profiles do not bundle Redis or hiredis; select a custom SDK containing the adapter.

## Shared configuration

Every application instance participating in a shared operation must use the same Redis endpoint, credentials, database, namespace and policy. Configure adapters in `bootstrap/app.hpp`. Use separate, non-overlapping prefixes for cache values, cache locks, sessions, remember credentials, quotas, queues and scheduler locks. Selecting a database in `.env` does not configure these adapters. Keep credentials in runtime configuration.

Memory stores coordinate only objects sharing the same process-local backend. Redis stores retain state across application restarts subject to server retention, expiration and durability configuration. Adapter failures are errors, not misses or permission to continue with a memory backend. Reconnecting on a later request does not replay an uncertain write automatically.

## Shared HTTP quotas

The original `http::rate_limit(options)` configures a bounded process-local store. The overload accepting a backend and policy namespace uses that store explicitly:

```cpp
#include <gungnir/http/security.hpp>
#include <gungnir/http/redis_rate_limit.hpp>

http::RedisRateLimitSettings settings;
settings.client.nodes = {{"127.0.0.1", 6379}};
settings.prefix = "myapp:http:quota:";
auto backend = std::make_shared<http::RedisRateLimitStore>(settings);
app.router().use(http::rate_limit(
    {.requests = 60, .window = std::chrono::seconds{60}},
    backend, "public-api"));
```

Requests are keyed by the policy namespace and trusted `request.client_ip()`, with `unknown-client` for an absent address. Configure trusted proxies before the limiter. Separate policy names and backend prefixes isolate quotas. Instances sharing a policy must agree on request limit and window. Redis uses a single-key Lua script to decide admission and set first-hit expiration atomically; application clock differences do not create separate windows. Denied requests do not extend expiration or increment an exhausted counter. The window restarts when the key expires, rather than at calendar boundaries.

Allowed responses include `x-ratelimit-limit` and `x-ratelimit-remaining`. Denied responses return 429 with those headers and a positive, rounded-up `retry-after` in seconds. Backend or corrupt-record errors stop the request before the handler; normal HTTP exception handling produces an error response. `max_clients` controls only the default memory backend, while Redis stores expire on the server. Limits and windows reject zero, negative or unrepresentable values.

The quota adapter uses the existing pooled `redis::Client`, including its authentication, timeouts and explicitly selected topology/TLS configuration. Group 4 acceptance covers standalone Redis. It establishes no Sentinel/Cluster failover or TLS acceptance claim. A single-primary lease or counter does not guarantee coordination across loss of acknowledged Redis state; eviction, persistence and failover configuration remain application deployment concerns.

## Leases and other shared state

[Cache locks](cache.md#locks) and scheduler locks share the existing owner-checked lease contract. Acquiring a Redis lease uses `SET NX PX`; renewal and release atomically compare the owner before extending or deleting it. A stopped process relies on expiration. Renew before a long action exceeds its chosen lifetime. These locks provide no fencing of work continuing after expiration and no consensus across backend failure.

Cache value flushes preserve separately namespaced leases and sessions. Administrative lock-store flush intentionally removes every lease in that literal namespace and must be restricted to controlled cleanup. Do not overlap value prefixes with protected state prefixes. Neither operation is a transaction against concurrent writers.

Redis sessions provide shared serialization, lifetime and rotation cleanup. Saves replace the entire session, so concurrent requests use last-writer-wins semantics. Loading, regenerating/erasing and saving are separate operations; a request holding an old snapshot can overwrite newer values or recreate an erased identifier. Shared storage alone does not serialize requests. Applications needing concurrent session mutation guarantees must serialize the entire session load/action/save boundary or use a store with an appropriate stronger contract. See [Sessions](session.md).

Scheduler `withoutOverlapping` protects the scheduled action; a queued action protects publication rather than the eventual job handler. `onOneServer` coordinates an occurrence. Queues use their existing reservation, renewal and durable retry contracts. See [Scheduler](scheduler.md) and [Queues](queues.md).

## Acceptance

With `GUNGNIR_REDIS_INTEGRATION_TESTS=ON`, run:

```sh
ctest --test-dir build --output-on-failure --no-tests=error \
  -R '^gungnir\.(redis|cache_redis|cache_coordination_redis|background_redis|application_redis|delivery_redis)$'
```

The native coordination test checks concurrent admission against two independent clients, literal prefix isolation, expired owners, renewal, stored null, lost factory leases, corrupt counters, backend failure and fixed-window expiration. The installed `.gnr` acceptance starts multiple actual HTTP processes, verifies shared JSON and application restart, coordinates factories and awaited lock middleware, resumes a stale owner after replacement, kills a lock holder and recovers after expiration, shares a quota across restarted processes, and checks session rotation and expiry. Tests use isolated prefixes and literal cleanup. Existing installed producer/worker/scheduler and delivery tests continue to verify shared scheduling, durable retries and authentication persistence.
