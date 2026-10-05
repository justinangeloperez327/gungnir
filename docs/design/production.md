# Production and Deployment

> **Future design specification.** This document describes intended long-term architecture beyond the published `1.0` contract. Examples and requirements may exceed the current implementation. See the [current implementation guide](../production.md) before using an API.


## Release alignment

This document describes possible future architecture beyond 1.0. Current implementation guides and executable tests define the published contract.

Gungnir provides production runtime primitives, but deployment policy remains an operational responsibility.

This document defines the framework boundary for health, shutdown, configuration, networking, observability, and process supervision.

# Production principle

A production-capable framework must distinguish:

~~~text
implemented runtime primitive
deployment recommendation
environment-specific responsibility
unsupported capability
~~~

The framework should not imply that one deployment topology is universally correct.

# Liveness

Liveness answers whether the process or runtime is alive.

A liveness check should be cheap and should not fail merely because an optional external dependency is temporarily unavailable.

# Readiness

Readiness answers whether this instance should receive new work.

Readiness may include required dependencies such as database, critical cache or queue, required configuration, and successful application boot.

Expensive diagnostics should not run on every high-frequency readiness request.

# Graceful shutdown

Production shutdown should be coordinated across:

- HTTP server;
- queue workers;
- scheduler;
- application providers;
- observability exporters;
- other registered runtime services.

A supervisor/application lifecycle should stop new work, signal cancellation, drain active work within deadline, flush required telemetry, and release services.

# Process signals

Signal handling belongs to the process or application boundary.

Subsystems should receive cancellation or stop requests rather than each installing competing global signal handlers.

# Reverse proxies

Gungnir may run behind a load balancer, ingress controller, Nginx, Apache, cloud reverse proxy, or service mesh.

Forwarded headers are trusted only when the peer is configured as trusted.

# TLS

TLS may terminate in Gungnir when supported or at a trusted reverse proxy/load balancer.

Production deployments must validate certificates and should not normalize insecure development flags into production defaults.

# Configuration

Production configuration should be externalized through environment, secret manager, deployment configuration, or mounted configuration.

Do not embed credentials into generated C++.

# Logging

Production logging should be structured and bounded.

Sinks and exporters need defined buffering, retry behavior, rotation or retention, failure handling, and shutdown flushing.

# Metrics and tracing

Observability should have bounded queues and explicit exporter failure policy.

Telemetry must not create unbounded memory growth.

# Streaming and backpressure

Large responses, uploads, and streams require bounded buffering and backpressure.

Do not assume memory buffering is appropriate for arbitrary payload size.

# Queue workers

Production queue workers require a durable shared driver when work must survive process loss.

MemoryDriver is not a durable production queue.

Workers need retry policy, failed-job handling, visibility or lease semantics where applicable, graceful stop, and observability.

# Scheduler

One-process scheduling is not automatically safe in a multi-instance deployment.

Distributed single-execution requirements need explicit locking or coordination.

# Sessions and cache

Multi-instance deployments require appropriate shared stores when session or cache semantics must be shared.

A process-local memory store is not distributed state.

# Database

Production database settings should define pool size, acquisition timeout, operation timeout, TLS where applicable, credentials, migration process, and health/readiness policy.

# Storage

LocalDisk is suitable only when local persistent storage semantics match the deployment architecture.

Multi-instance and container deployments often require shared or object storage.

# Resource limits

Production configuration should bound connections, request sizes, queue concurrency, executor workers, scheduler work, telemetry queues, cache or session memory, and timeouts.

Unbounded defaults are not acceptable merely for convenience.

# Failure behavior

Subsystem failures should be observable.

A production runtime should not silently continue after critical startup failures while reporting readiness.

# Deployment responsibility

Gungnir does not replace an OS service manager, container orchestrator, load balancer, TLS/certificate operations, database administration, secret management, backup/disaster recovery, centralized observability, or capacity planning.

It provides framework hooks that integrate with those systems.

# Design rule

~~~text
framework owns deterministic runtime behavior
deployment owns infrastructure policy
production claims must match implemented guarantees
~~~

