# Cache

> **Development design specification.** This document describes intended long-term architecture beyond the frozen `development` contract. Examples and requirements may exceed the current implementation. See the [current implementation guide](../cache.md) before using an API.


## Development alignment

Gungnir is under active development. No 1.0 compatibility contract is frozen; this document may describe intended future architecture, but current implementation guides and executable tests remain authoritative.

Gungnir cache separates the application-facing cache repository from storage adapters.

# Core contract

A cache operation is based on:

~~~text
key
value
optional expiration
store
~~~

The repository owns common behavior. The store owns persistence.

# Stores

The core may provide:

- MemoryStore for development, tests, and single-process workloads;
- RedisStore when the optional Redis adapter is enabled.

Additional production stores can implement the same contract.

# Memory store

MemoryStore is process-local.

It is not suitable for:

- multi-instance coordination;
- durable cache state;
- distributed locks;
- shared sessions;
- shared queue semantics.

# Values

The low-level cache store should use an explicit serializable representation.

Application models or arbitrary native objects should not be persisted through unsafe memory/type erasure.

Higher-level serializers may encode strings, JSON/data, or explicit application payloads.

# Expiration

Entries may have no expiration or an explicit TTL.

Process-local expiration should use a monotonic clock where appropriate. Distributed stores use backend expiration semantics.

# Remember

A read-through helper may implement:

~~~text
get key
if found -> return
else compute
store
return
~~~

Basic remember does not automatically guarantee distributed stampede protection.

Atomic single-flight or locking behavior must be an explicit capability.

# Namespacing

Applications/stores may use prefixes or namespaces to prevent key collisions.

Secrets should not be placed directly in cache keys when keys may appear in logs or monitoring.

# Redis

The optional Redis adapter may provide shared process-independent cache storage.

Configuration may include host, port, authentication, database, prefix, timeouts, and TLS/backend capabilities when implemented.

Credentials belong in runtime configuration.

# Serialization compatibility

Persisted structured cache payloads are a compatibility boundary.

Applications should version long-lived payloads when schema changes can make old entries unreadable.

# Errors

Stores should distinguish:

- cache miss;
- backend/storage failure;
- serialization failure;
- timeout/cancellation.

A cache miss is not an exception.

# Security

Cache data may contain sensitive application values.

Production stores need appropriate access control, network encryption where required, secret management, and retention/TTL policy.

# Testing

Tests should use isolated stores or namespaces with deterministic cleanup.

# Design rule

~~~text
cache repository defines behavior
store defines persistence
serialization is explicit
distributed guarantees are never implied by MemoryStore
~~~

