# Application Lifecycle

> **Future design specification.** This document describes intended long-term architecture beyond the published `1.0` contract. Examples and requirements may exceed the current implementation. See the [current implementation guide](../application-lifecycle.md) before using an API.


## Release alignment

This document describes possible future architecture beyond 1.0. Current implementation guides and executable tests define the published contract.

Gungnir applications have one explicit lifecycle shared by the HTTP runtime, queue workers, scheduler, providers, database services, observability, and other long-lived framework components.

The lifecycle exists so startup and shutdown remain deterministic and application code does not depend on native static initialization order.

# Lifecycle phases

Canonical phases are:

~~~text
created
configuring
registering
booting
ready
running
stopping
stopped
~~~

A phase may expose framework hooks, but an application must not move backward through the lifecycle.

# Bootstrap sequence

The intended application bootstrap is:

~~~text
Application::create
  -> environment/configuration
  -> provider registration
  -> container/service registration
  -> framework/database/runtime boot
  -> boot hooks
  -> readiness checks
  -> ready hooks
  -> runtime start
~~~

Application-facing Gungnir source should not need to manually orchestrate native constructors or global objects.

# Providers

Providers are the extension point for framework/package bootstrapping.

A provider may:

- register services;
- configure bindings;
- participate in boot;
- participate in readiness;
- release resources during shutdown.

Providers must be installed before boot.

Runtime mutation of the provider graph after the application is running should be rejected unless a future hot-reload contract explicitly supports it.

# Dependency ordering

Registration happens before boot so dependencies can be resolved deterministically.

A provider that depends on another service should resolve it only after the dependency has been registered.

Circular provider/service graphs should fail with a clear diagnostic rather than recurse indefinitely.

# Application mode

Application mode is derived from configuration such as APP_ENV and normalized to a stable semantic value.

Typical modes are:

~~~text
development
testing
staging
production
~~~

Framework behavior should use semantic mode checks rather than repeated string comparisons.

# Ready state

An application becomes ready only after required boot work and readiness checks succeed.

Readiness is distinct from process liveness.

A process may be alive but not ready to receive traffic when required dependencies are unavailable.

# Running state

The running phase owns long-lived runtimes such as:

- HTTP server;
- queue workers;
- scheduler;
- background runtime services explicitly registered with the application.

Long-lived work should be registered with the application lifecycle instead of being detached through unmanaged threads.

# Shutdown

Shutdown is cooperative.

The application enters:

~~~text
stopping
~~~

before:

~~~text
stopped
~~~

Shutdown should:

1. stop accepting new work;
2. signal cancellation;
3. allow active work to drain within configured deadlines;
4. flush telemetry/logging where configured;
5. stop providers/services in dependency-safe order;
6. release runtime resources.

Dependent components should normally stop before the services they depend on.

# Reverse shutdown order

Provider/service teardown should use reverse dependency/registration order where appropriate.

This prevents infrastructure such as logging, database pools, or executors from disappearing while higher-level components still require them.

# Failure during startup

A startup failure prevents the application from entering ready/running state.

Already-created resources must still be released.

The framework must not report readiness after a required provider or runtime failed to boot.

# Failure during shutdown

Shutdown should continue attempting to release independent resources even if one shutdown callback fails.

Failures should be reported through diagnostics/logging and returned to the lifecycle owner according to the runtime contract.

# Async lifecycle

Lifecycle state must integrate with async-runtime cancellation.

Long-lived coroutines should receive application/request cancellation context rather than depend on process-global flags.

The lifecycle must not assume a coroutine resumes on the same OS thread.

# Application-facing boundary

Normal .gnr controllers, models, policies, events, and other application declarations do not manage the lifecycle directly.

Lifecycle mechanics belong to:

- application bootstrap;
- providers/extensions;
- runtime configuration;
- executable entry points.

# Design rule

~~~text
one application
one lifecycle
explicit startup
explicit readiness
cooperative shutdown
no hidden global initialization dependency
~~~

