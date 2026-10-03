# Production Resilience

> **Status: experimental, pre-1.0.** Phase 15 defines bounded failure and overload behavior for Gungnir's current production runtime. It does not replace infrastructure-level redundancy, autoscaling, load balancing, backups, or disaster recovery.

## Health

`production::Health` supports named liveness and readiness checks.

Health checks now fail closed:

- a callback returning `false` marks that probe unhealthy;
- a callback throwing an exception also marks that probe unhealthy;
- probe evaluation does not propagate dependency exceptions into the HTTP/request path;
- duplicate or invalid check registrations are rejected.

`liveness_report()` and `readiness_report()` return named per-check results for operational adapters.

An empty liveness or readiness set is healthy. Applications should explicitly register dependencies that are required for their deployment.

## Runtime host health

`RuntimeHost::health()` exposes the host health registry.

The host registers an internal `runtime-host` probe:

- liveness fails when a supervised runtime records an unhandled fatal failure;
- readiness requires the application to be booted, the host not to be stopping, and no fatal supervised failure to be recorded.

Applications may add required dependency checks before serving production traffic.

## HTTP overload admission

`http::RuntimeOptions::max_active_dispatches` bounds active cooperative HTTP work independently from `max_connections`.

The limit covers active handler dispatches and cooperative stream/WebSocket work tracked by the HTTP runtime. When the server is at capacity:

- new HTTP/1 requests receive `503 Service Unavailable`;
- the response includes `Retry-After: 1`;
- the overloaded connection is closed after the response;
- HTTP/2 streams receive a 503 response rather than creating more application work;
- `http.server.overload.count` is incremented.

This is an admission-control guardrail, not an autoscaler. Capacity still requires workload-specific measurement.

## Bounded retry

`production::retry` provides explicit, bounded retry for operations that the application has classified as transient.

The caller supplies:

- maximum attempts;
- initial and maximum backoff;
- backoff multiplier;
- a predicate deciding whether the captured failure is retryable;
- an optional cancellation token.

Cancellation is authoritative and interrupts retry backoff. `OperationCancelled` is never converted into a retry.

Gungnir intentionally does not retry arbitrary operations automatically. Retrying non-idempotent work without a domain-specific policy can duplicate side effects.

## Shutdown and failure behavior

Phase 11 shutdown guarantees remain authoritative:

- admission stops first;
- in-flight work drains within the configured deadline;
- cancellation propagates when the deadline expires;
- supervised services share the process cancellation boundary;
- fatal supervised runtime failures are retained and surfaced by `RuntimeHost`.

Phase 15 adds health visibility and overload admission on top of those lifecycle guarantees.

## CI resilience gate

Primary pull-request CI runs:

- `gungnir.production_resilience`
- `gungnir.http_overload`
- `gungnir.runtime_supervisor`
- `gungnir.runtime_host`
- `gungnir.runtime_lifecycle`

The overload test proves that one retained in-flight request can consume the configured dispatch budget while the next request is rejected with 503, then confirms the retained request drains cleanly.

## Deployment boundaries

Framework resilience is only one layer. Production deployments still need appropriate:

- load balancers and autoscaling;
- container or service supervision;
- durable/shared queues;
- distributed scheduler locks;
- shared session/cache stores when required;
- database failover and backup policy;
- TLS/certificate operations;
- secrets management;
- telemetry retention/export policy;
- capacity testing and alerting.

See [Production and Deployment](production.md), [Runtime Correctness](runtime-correctness.md), and [Security Hardening](security-hardening.md).
