# Production and Deployment

Gungnir exposes a small health/readiness contract while keeping deployment policy outside the framework.

## Health

`production::Health::live()` represents process liveness. Readiness checks are explicit callbacks registered by the application. `ready()` succeeds only when every registered readiness check succeeds.

Readiness checks should cover dependencies that must be available before an instance receives traffic. Expensive diagnostics should not be placed on high-frequency health endpoints.

## Shutdown

The application already exposes lifecycle shutdown and server stop operations. Deployment environments should stop accepting new traffic before terminating the process and allow in-flight work to finish according to the server/runtime guarantees actually implemented.

Gungnir does not currently claim complete graceful draining across HTTP connections, queues, scheduler callbacks and arbitrary application coroutines.

## Reverse proxies

Applications deployed behind a reverse proxy must configure trust at the HTTP boundary. Forwarded headers must not be trusted merely because they are present. Proxy address ranges, forwarded host/protocol handling and client IP resolution require an explicit trust policy.

## Configuration

Production configuration should be supplied through the application's environment/configuration facilities. Secrets must not be committed to generated project files or emitted through diagnostics, HTTP error pages or logs.

## Current runtime limitation

The current HTTP runtime is not presented as a production-grade HTTP/2, TLS, WebSocket or high-concurrency server unless the concrete transport implements those capabilities. Deployments requiring those properties should use a suitable front-end server or transport integration and validate the runtime under representative load.

## Deployment responsibility

Container images, service managers, TLS termination, log shipping, metrics exporters, process supervision and zero-downtime orchestration are deployment concerns. Gungnir should provide integration points without pretending to replace those systems.
