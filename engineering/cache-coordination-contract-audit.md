# Group 4 distributed cache coordination

Baseline: main `3ba8c675a685ec039a12287ec3e6eb3fbae55d7b`, after database
acceptance PR #188 merged. Package, compiler and native contract metadata remain
at version 1.0.0 / contract 1.0 / ABI epoch 1.

## Existing support and proven gaps

The native string cache and canonical JSON facade already support memory/Redis
values and basic read-through factories. Sessions, queues and scheduler leases
already have Redis implementations. Cache locks were mentioned in the product
guide but had no canonical API or retained dependency registration. HTTP rate
limiting always allocated process-local counters; sharing Redis cache values
could not make it a global quota. Existing acceptance did not exercise owned
cache locks or global quotas through multiple installed application processes.

## Implementation

`CacheLock` is an opaque canonical handle backed by the existing owner-checked
scheduler lease interface. `cache.lock(key, ttlMilliseconds)` creates an owned
handle; `acquire`, `renew` and `release` are canonically validated and emitted.
Copies share a synchronized lease state, retain the adapter, and release once
the last handle dies. An explicit release reports backend errors, while
destruction relies on TTL after an unavailable backend. Missing lock-store
configuration does not silently select memory. Generated development bootstrap
sets an explicit memory store; shared bootstrap selects Redis with its own
non-overlapping namespace.

`rememberLocked(key, seconds, leaseMilliseconds, factory)` checks before and
after acquisition, does not invoke a factory on contention, validates the same
synchronous zero-argument JSON-compatible factory contract as `remember`, and
checks ownership before publishing. Exceptions and serialization failures
release the lease. Stored JSON null remains a hit. Lock handles, including
nested values, cannot become cached data or payload fields. Ordinary `remember`
retains its existing concurrent-miss behavior.

The original `http::rate_limit(options)` entry point remains available. A new
overload accepts an explicit `RateLimitStore` and policy namespace. The memory
store retains bounded capacity and monotonic first-hit windows; rejected
requests cannot overflow the counter. Redis uses a single-key Lua operation for
admission, remaining allowance and first-hit expiration, with backend time for
all instances. Denials do not extend expiration. Backend errors stop execution
before the handler; there is no fallback or uncertain-write replay. Policy and
client boundaries use a length delimiter to avoid concatenation collisions.

The live namespace test exposed scheduler lock cleanup interpreting literal
prefixes as Redis glob expressions. Cleanup now scans and filters literal
prefix bytes, matching cache cleanup. Expired memory lease release now returns
false, consistent with Redis owner/expiry behavior.

## Acceptance

- Generated service fixtures cover native/canonical lock copies, renewal,
  release, exception unwind, named locked factories, retained adapter ownership,
  missing configuration and matching invalid-program diagnostics.
- Memory tests exercise concurrent misses, contention without a factory,
  failed factories, observed lost ownership before publication and value flush
  preserving held locks. Public entry-point headers compile independently.
- Native Redis tests use independent clients and concurrent threads to admit
  exactly a shared limit, reject corrupt state and unavailable backends, preserve
  first-hit TTL on denials, recover after natural expiry, isolate literal
  prefixes and reject stale renewal/release after replacement.
- Installed `.gnr` applications run multiple live HTTP processes, share JSON
  including Unicode, uint64 and null, survive restart, protect awaited actions,
  coordinate a cold factory, resume an expired owner while its replacement
  remains held, and recover after a killed holder. HTTP quotas persist across
  instances/restart and reset after expiration. Shared sessions rotate/remove old
  IDs and expire. Fixture barriers and server cleanup are bounded by test timeouts.
- CI runs the new native and installed Redis acceptance on pull requests,
  alongside retained producer/worker/scheduler, authentication and delivery
  acceptance. The primary gate adds native cache/scheduler regression execution.
  GCC/Clang/MSVC conformance executes the generated handle fixtures and compares
  emitted C++, validated AST and IR. Documentation examples are strictly checked.

## Explicit limits

Acceptance covers standalone Redis; it does not establish topology/TLS/failover
guarantees. Leases expire and have no fencing tokens. Lock renewal, cache
publication and release are separate operations; applications must bound
protected work and handle possible publication followed by a backend error.
Redis eviction/failover losing coordination state remains a deployment concern.
The cache/lease adapters retain their existing single-endpoint transport.

Redis sessions remain complete last-writer-wins snapshots, with separate
load/rotation/save operations. Parallel stale requests can overwrite or recreate
old state; shared persistence alone is not request serialization. Existing queue
and scheduler contracts remain, including publication-only locking of queued
scheduled actions. The current guides describe these limits explicitly.
