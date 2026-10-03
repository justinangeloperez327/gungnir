# Async Runtime

> **Gungnir 1.x design specification.** This document describes intended long-term architecture beyond the frozen `1.0.0-rc.1` contract. Examples and requirements may exceed the current implementation. See the [current implementation guide](../async-runtime.md) before using an API.


## 1.0 RC alignment

The shipped baseline is `1.0.0-rc.1`. The 1.0 contracts are frozen; this design document may describe additive 1.x evolution or future-major work, but current implementation guides and executable tests remain authoritative.

This document defines the runtime responsibilities that implement the language-level async/await contract in async.md.

Application source uses:

~~~gnr
public async show(int id) {
    const user = await users.find(id);

    return json(user);
}
~~~

The runtime implements the native scheduling, coroutine, cancellation, timer, and context propagation required to make that syntax safe.

# Boundary

Language semantics:

~~~text
async
await
logical result T
cancellation
structured task lifetime
~~~

Runtime implementation:

~~~text
Task<T>
C++23 coroutines
executor scheduling
timers
reactor integration
coroutine frames
cancellation state
context propagation
~~~

Application code should not depend on the native implementation details.

# Executor

The runtime owns a bounded Executor for scheduled work and coroutine continuations.

The executor must not create an unbounded thread per request or suspension.

Scheduling should preserve:

- bounded resource usage;
- cancellation;
- observability context;
- request/application execution context;
- deterministic shutdown behavior.

# Timers

sleep/timer operations use a shared scheduler.

A timer should not create a detached OS thread per await.

When the HTTP reactor is active, timer deadlines may integrate with the reactor wake/poll cycle while continuations are dispatched through the executor.

# Cancellation

CancellationSource and CancellationToken provide cooperative cancellation.

Cancellation may originate from:

- client disconnect;
- request timeout;
- application shutdown;
- explicit parent cancellation;
- queue/scheduler shutdown.

Cancellation must propagate through owned async work.

It must not silently become successful completion.

# Structured lifetime

Async work must have an owner.

Valid owners include:

- current request;
- current middleware/action call;
- application runtime service;
- queue job;
- scheduler task;
- explicit structured task scope.

The runtime should avoid unstructured fire-and-forget work that can outlive request-scoped state.

# Request context

Across suspension the runtime must preserve logical request context including:

- request state;
- authentication identity;
- request-scoped dependency injection;
- tracing/request ID;
- cancellation.

This context must follow the coroutine, not the worker thread.

# Thread migration

A coroutine may resume on a different worker thread.

Therefore request-local framework state must not depend solely on thread_local storage unless a coroutine-aware propagation layer restores it correctly.

# Coroutine safety

Values retained across await must have safe lifetime.

The compiler/runtime may need to own or copy data that would otherwise lower to a short-lived native view/reference.

A direct native translation is not acceptable when it would create dangling references across suspension.

# HTTP integration

The HTTP runtime owns request/connection state while a route task is suspended.

Completion must wake the appropriate reactor/execution path before the response is serialized and written.

Request timeout and shutdown cancellation should propagate to suspended handlers.

# Blocking work

async does not make blocking work non-blocking.

Blocking database drivers, filesystem calls, CPU-heavy work, and legacy native APIs require:

- an explicit blocking executor;
- a true async backend;
- or a queue/background job.

They must not block the readiness reactor.

# Async database work

A database call is only truly async when its driver/runtime supports suspension without blocking the request executor.

Async transactions require coroutine-safe connection ownership and transaction context.

Thread-affine transaction state must not be carried across arbitrary await points.

# Async locks

Blocking native mutexes should not be held across await.

Where synchronization is required, the runtime should provide async-aware primitives or structure ownership so blocking locks are unnecessary.

# Shutdown

Runtime shutdown should:

1. stop accepting new work;
2. signal cancellation;
3. drain owned tasks within deadline;
4. stop timers/reactors;
5. stop executor workers.

Detached tasks must not prevent deterministic shutdown.

# Observability propagation

Tracing context should propagate across:

- executor scheduling;
- coroutine suspension/resumption;
- queue handoff where explicitly serialized;
- timer wakeups.

This is runtime plumbing, not application syntax.

# Current implementation

Gungnir already has foundations including:

- Task<T>;
- bounded executor;
- cancellation source/token;
- timer scheduling;
- HTTP readiness-reactor integration;
- coroutine request ownership.

The target architecture should continue removing token/source-based async lowering from the compiler and drive coroutine generation from the Validated AST.

# Design rule

~~~text
language async defines semantics
runtime async provides safe scheduling and lifetime
compiler lowering connects the two
~~~

