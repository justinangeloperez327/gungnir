# Async and Await

> **Future design specification.** This document describes intended long-term architecture beyond the published `1.0` contract. Examples and requirements may exceed the current implementation. See the [current implementation guide](../async.md) before using an API.


## Release alignment

This document describes possible future architecture beyond 1.0. Current implementation guides and executable tests define the published contract.

Gungnir exposes asynchronous programming through `async` and `await`.

The application language makes suspension explicit while hiding native C++ coroutine machinery.

Application code should reason about:

~~~text
async operation
awaited result
cancellation
request lifetime
logical return type
~~~

It should not need to reason about:

~~~text
co_await
co_return
promise_type
coroutine_handle
Task<T> plumbing
executor internals
native reference lifetimes
~~~

# Design principle

The central rule is:

~~~text
async is semantic, not cosmetic
~~~

A function or framework action is marked async only when its execution may suspend.

Gungnir must not label synchronous work as asynchronous merely to make an API look modern.

# Ordinary async functions

An ordinary asynchronous function uses:

~~~gnr
async function User loadUser(int id) {
    return await repository.find(id);
}
~~~

The declared return type is the **logical result type**:

~~~text
User
~~~

not:

~~~text
Task<User>
~~~

The generated C++ implementation may use:

~~~text
Task<User>
~~~

or another coroutine-backed type internally.

# Controller actions

Controller actions use `public async`:

~~~gnr
controller UserController {
    public async show(int id) {
        const user = await users.find(id);

        return json(user);
    }
}
~~~

The controller still has the implicit response contract.

Conceptually:

~~~text
Gungnir                 logical result       native implementation

public show(...)        Response             Response
public async show(...)  Response             Task<Response>
~~~

Application code never writes the native wrapper.

# Middleware

Async middleware uses:

~~~gnr
middleware AuthMiddleware {
    inject AuthService auth;

    public async handle(Request request, Next next) {
        const user = await auth.resolve(request);

        if (user == null) {
            return response(null, 401);
        }

        return await next(request);
    }
}
~~~

The logical result remains:

~~~text
Response
~~~

# Listeners

An in-process listener may suspend:

~~~gnr
listener SyncProfile {
    inject ProfileService profiles;

    public async handle(UserRegistered event) {
        await profiles.sync(event.user);
    }
}
~~~

An async listener is still part of the current event-dispatch lifecycle.

It is **not automatically queued**.

# Async does not mean background

These concepts are different:

~~~text
sync
    caller executes work directly

async
    caller may suspend while work completes

queued
    work belongs to another job lifecycle

background
    work executes independently from the current request/task lifecycle
~~~

Gungnir must not conflate them.

For example:

~~~gnr
public async store(Request request) {
    const result = await importer.import(request.file('data'));

    return json(result);
}
~~~

means the current request may suspend.

It does not mean the import was automatically moved to a queue worker.

# Await

`await` waits for an awaitable operation and produces its logical result:

~~~gnr
const user = await repository.find(id);
~~~

Conceptually:

~~~text
repository.find(id)    Async<User>
await ...              User
~~~

The exact compiler-internal async wrapper type is not part of the source language.

# Await is explicit

Potential suspension points must remain visible:

~~~gnr
const user = await repository.find(id);
~~~

Gungnir should not silently insert suspension around ordinary-looking calls.

This keeps:

- latency boundaries visible;
- control flow understandable;
- request lifetime easier to reason about;
- error propagation explicit;
- compiler analysis predictable.

# Await context

`await` is legal only inside an async context.

Valid:

~~~gnr
async function User loadUser(int id) {
    return await repository.find(id);
}
~~~

Invalid:

~~~gnr
function User loadUser(int id) {
    return await repository.find(id);
}
~~~

The compiler should report a dedicated diagnostic before C++ generation.

# Async without await

An async function that contains no reachable suspension point should normally produce a diagnostic.

Example:

~~~gnr
async function int add(int a, int b) {
    return a + b;
}
~~~

This should usually be written:

~~~gnr
function int add(int a, int b) {
    return a + b;
}
~~~

Initially this may be a warning rather than an error because abstraction boundaries can make an async signature intentional.

# Return from async functions

Return the logical result directly:

~~~gnr
async function User loadUser(int id) {
    const user = await repository.find(id);

    return user;
}
~~~

Do not write:

~~~text
return Task<User>(user);
co_return user;
~~~

The compiler emits native coroutine return syntax.

# Returning an awaited value

This is valid:

~~~gnr
async function User loadUser(int id) {
    return await repository.find(id);
}
~~~

# Awaited type checking

If:

~~~text
repository.find(id) -> async User
~~~

then:

~~~gnr
const user = await repository.find(id);
~~~

has type:

~~~text
User
~~~

If the async operation logically returns `User?`, the awaited result is:

~~~text
User?
~~~

Await removes the async wrapper, not nullability or other application semantics.

# Await only awaitable values

This should be rejected:

~~~gnr
const count = await 25;
~~~

Only values with a defined async/awaitable contract may be awaited.

# Chained async operations

A result may be awaited before normal chaining:

~~~gnr
const users = await repository.activeUsers();

const names = users
    .sortBy('name')
    .pluck('name');
~~~

The compiler should not allow synchronous collection calls on an unresolved async value unless the API intentionally defines such behavior.

# Await in arguments

~~~gnr
const result = transform(
    await service.fetch()
);
~~~

The awaited expression is evaluated before the call receives its argument.

# Await in conditions

~~~gnr
if (await permissions.allows(user, project)) {
    return json(project);
}
~~~

The awaited result must be boolean-compatible.

# Await in return

~~~gnr
return await next(request);
~~~

This is common for middleware.

# Multiple independent operations

Gungnir should not automatically run sequential `await` expressions concurrently.

This:

~~~gnr
const user = await users.find(userId);
const project = await projects.find(projectId);
~~~

means:

~~~text
await user
then
await project
~~~

Automatic parallelization would change observable behavior and resource usage.

# Explicit concurrency

When operations are independent, concurrency must be explicit.

A future/core concurrency helper may provide:

~~~gnr
const [user, project] = await all([
    users.find(userId),
    projects.find(projectId)
]);
~~~

or a structured equivalent.

The exact destructuring syntax depends on the finalized language grammar.

The semantic rule is mandatory:

~~~text
sequential source stays sequential
concurrency requires explicit syntax/API
~~~

# Async collections and database APIs

An ORM operation should only be asynchronous when the underlying runtime genuinely performs asynchronous I/O.

Gungnir must not expose fake async wrappers around blocking database work.

If a backend/runtime call is synchronous:

~~~gnr
const users = User::where('active', true).get();
~~~

it remains synchronous.

If a future backend exposes true async query execution, the API may provide an explicitly asynchronous operation:

~~~gnr
const users = await User::where('active', true).getAsync();
~~~

or another finalized contract.

Do not assume every ORM method becomes async merely because controllers can be async.

# Async I/O

Appropriate async operations include work that can genuinely suspend, such as:

- network I/O;
- HTTP client operations;
- asynchronous database drivers;
- timers;
- queue/network transports;
- file I/O when supported asynchronously;
- downstream middleware/controller continuations.

CPU-bound work does not become non-blocking merely by adding `async`.

# CPU-bound work

This is conceptually wrong if `calculateReport()` simply performs heavy CPU work on the request executor:

~~~gnr
const report = await calculateReport();
~~~

CPU-heavy work should use an explicit executor/job/offload mechanism when the runtime supports it.

Async syntax must not hide blocking CPU work.

# Cancellation

Async operations should support structured cancellation.

A request may be cancelled because:

- the client disconnected;
- a timeout expired;
- the server is shutting down;
- a parent operation was cancelled.

Cancellation should propagate through the async execution context.

Application APIs that need direct cancellation awareness may receive or access a framework cancellation token:

~~~gnr
public async import(
    Request request,
    CancellationToken cancellation
) {
    const result = await importer.import(
        request.file('data'),
        cancellation
    );

    return json(result);
}
~~~

The exact injection surface may be refined with request/context design, but cancellation must be a first-class runtime concept rather than a process-global flag.

# Cancellation is not failure swallowing

Cancellation should have a defined propagation path.

An operation must not silently convert cancellation into successful completion.

Cleanup may occur during cancellation, but code should preserve cancellation unless it intentionally handles it.

# Timeouts

Timeouts should be explicit.

A future/core helper may support:

~~~gnr
const result = await timeout(
    service.fetch(),
    5.seconds
);
~~~

Timeout semantics must integrate with cancellation so timed-out work is not left running indefinitely where cancellation is supported.

The exact duration syntax belongs to the type/function grammar.

# Request lifetime

Async controller and middleware code may suspend while retaining request-scoped values.

This must remain safe:

~~~gnr
public async store(Request request) {
    const email = request.input('email');

    const result = await service.lookup(email);

    return json(result);
}
~~~

The runtime must preserve the request context for the full async action lifetime.

Application developers should not manage raw request-buffer lifetimes.

# Authentication context

Request-scoped authentication must survive suspension:

~~~gnr
public async show() {
    const user = Auth::user();

    const project = await projects.forUser(user.id);

    return json(project);
}
~~~

The authenticated identity after resumption must still belong to the same request.

# Dependency injection scopes

Request-scoped injected dependencies must remain attached to the same logical request across suspension.

The runtime must not implement request scope as an unsafe process-global or thread-local assumption.

# Thread migration

A coroutine may resume on a different worker thread.

Therefore request state, authentication, scoped services, tracing, and other execution context must follow the logical coroutine/request rather than raw thread identity.

This is an important runtime requirement.

# Local variables across suspension

Locals used after an `await` must remain valid:

~~~gnr
public async show(int id) {
    const requestId = request.id();

    const user = await users.find(id);

    logger.info(requestId);

    return json(user);
}
~~~

The generated coroutine owns whatever storage is necessary.

Application code does not manage coroutine frames.

# References and pointers

Normal Gungnir source does not expose native C++ references or pointers.

This avoids common coroutine lifetime hazards such as retaining references to stack values that no longer exist after suspension.

Native bindings must still document and enforce whether values are safe to retain across an await boundary.

# Temporary lifetime

The compiler/lowering layer must not produce native code that stores dangling views/references into temporary data across suspension.

For example, request string/body access lowered to native `string_view` must be copied or otherwise lifetime-protected if the value survives an await.

This is a compiler/runtime responsibility.

# Async errors

Errors from an awaited operation propagate through the Gungnir error model.

Example:

~~~gnr
const user = await repository.find(id);
~~~

If the operation fails, the async function should not silently receive a default `User`.

Error handling belongs to `errors.md`.

Native C++ coroutine exception plumbing is not application syntax.

# Cleanup

Structured local values must be cleaned up correctly when:

- an async function returns;
- an awaited operation fails;
- cancellation occurs;
- the coroutine is destroyed.

Generated C++23 must preserve RAII/resource safety internally.

# Async middleware pipeline

The middleware continuation may itself be asynchronous:

~~~gnr
public async handle(Request request, Next next) {
    const response = await next(request);

    response.header(
        'X-Request-ID',
        request.id()
    );

    return response;
}
~~~

The middleware pipeline must preserve before/after execution order across suspension.

# Async event dispatch

An async listener does not imply fire-and-forget behavior.

If an event dispatcher supports async listeners, dispatch should either:

- await them according to a defined order; or
- expose a clearly separate async dispatch API.

It must not silently detach listener coroutines.

# Fire-and-forget

Unstructured fire-and-forget tasks are dangerous because they can outlive:

- request scopes;
- database transactions;
- injected services;
- cancellation contexts;
- application shutdown.

The first stable Gungnir language should not expose a casual `spawn()` that detaches arbitrary request code without an explicit lifecycle owner.

Use queued jobs or a structured application task scope instead.

# Structured concurrency

When Gungnir introduces concurrent task groups, child tasks should belong to a parent scope.

Conceptually:

~~~text
parent task
  -> child A
  -> child B

parent completes only after required children complete/cancel
~~~

This prevents orphaned work and makes cancellation predictable.

# Queue boundary

Queued/background work is not an async-language feature.

A queued job must serialize or otherwise safely carry the data required for another execution lifecycle.

Do not capture:

~~~text
Request
Response
Next
request-scoped services
native references
open database transaction scope
~~~

into queued work.

# Async and transactions

A database transaction held across `await` requires explicit runtime support.

The compiler/framework must not assume a synchronous transaction object is safe across arbitrary suspension or worker migration.

Where asynchronous transactions are supported, their scope must be coroutine-safe.

# Async and locks

Application code should avoid holding blocking native locks across `await`.

The Gungnir runtime should provide async-aware synchronization primitives if application-level synchronization is necessary.

Native mutex mechanics should not leak into normal framework code.

# Ordering

Within one async function, source order remains observable unless explicit concurrency is introduced.

~~~gnr
await first();
await second();
~~~

must execute as:

~~~text
first completes
then second begins/completes
~~~

# Async recursion

Async recursion may be supported:

~~~gnr
async function Node loadTree(int id) {
    const node = await repository.find(id);

    // ...
    return node;
}
~~~

The compiler/runtime must handle the coroutine allocation/lifetime implications safely.

# Async lambdas

Async callbacks may eventually use:

~~~gnr
async (item) => {
    return await service.process(item);
}
~~~

This syntax should only become stable when callback APIs can represent async callable types correctly.

Until then, named async functions or framework actions are preferable.

# Logical async type model

The compiler should distinguish:

~~~text
T
Async<T>
~~~

internally.

Source declarations expose logical `T`, while calls to async functions produce an awaitable expression until awaited.

Example:

~~~gnr
async function User loadUser(int id) {
    // ...
}
~~~

Call without await:

~~~gnr
const pending = loadUser(id);
~~~

Conceptually:

~~~text
pending: Async<User>
~~~

After:

~~~gnr
const user = await loadUser(id);
~~~

the type is:

~~~text
User
~~~

# Unawaited async values

An async call whose result is ignored should normally be diagnosed:

~~~gnr
loadUser(id);
~~~

because the operation may never be awaited or completed according to the runtime contract.

Prefer:

~~~gnr
const user = await loadUser(id);
~~~

or an explicit queue/task API when detachment is intended.

# Double await

This should be rejected when the first await already produced a non-awaitable result:

~~~gnr
const user = await await loadUser(id);
~~~

# Async function type

Semantic analysis may represent:

~~~text
AsyncFunctionType
  parameters[]
  logicalResult
~~~

or a normal function type with an `async` flag.

For:

~~~gnr
async function User loadUser(int id)
~~~

conceptually:

~~~text
async (int) -> User
~~~

# Async AST

The syntax AST should represent async explicitly.

Function:

~~~text
FunctionDeclaration
  name
  async = true
  parameters[]
  logicalReturnType
  body
~~~

Controller action:

~~~text
ControllerAction
  name
  async = true
  responseContract = Response
  body
~~~

Await:

~~~text
AwaitExpression
  operand
  resolvedAwaitedType
  resultType
~~~

Async behavior must not remain a token-rewrite pass indefinitely.

# Semantic validation

The async semantic pass should validate at least:

- `await` only appears in async contexts;
- awaited values are awaitable;
- async calls produce async/awaitable values;
- logical return values match declared contracts;
- unawaited async results are diagnosed where required;
- request-scoped values remain valid across suspension;
- native bindings declare whether returned awaitables are safe;
- framework actions preserve their specialized result contracts;
- async does not imply queued/background execution;
- cancellation-aware APIs receive compatible cancellation context;
- coroutine/request scope is not tied unsafely to a worker thread.

# Compiler pipeline

Async source should pass through:

~~~text
.gnr source
  -> lexer
  -> parser
  -> async/function/action AST
  -> symbol resolution
  -> type analysis
  -> awaitability analysis
  -> lifetime/context validation
  -> validated async AST
  -> coroutine lowering
  -> C++23
~~~

The transpiler must not discover `async` or `await` behavior by rescanning raw source after parsing.

# C++23 lowering

For:

~~~gnr
async function User loadUser(int id) {
    return await repository.find(id);
}
~~~

the generated implementation may conceptually use:

~~~text
Task<User>
co_await
co_return
coroutine promise/frame machinery
executor scheduling
~~~

These are implementation details.

The generated C++ should remain ordinary and inspectable, but application syntax must not expose coroutine boilerplate.

# Native interoperability

A native binding that exposes an asynchronous operation must register its logical result and awaitable behavior with the Gungnir type system.

Application code should see:

~~~text
async operation -> logical T
~~~

not an opaque unknown C++ task type.

# Diagnostics

Async diagnostics should be phase-specific and point to the actual source expression.

Examples:

~~~text
await used outside async context
expression is not awaitable
async result is not awaited
return type does not match logical async result
request-scoped value cannot safely cross suspension
unsupported async native binding
~~~

Diagnostics should use stable codes once the async semantic pass is implemented.

# Current implementation boundary

The existing compiler already has token-aware async/await lowering and can generate native coroutine-backed controller/middleware code.

This document defines the target semantic contract.

The next compiler evolution should move async behavior fully into:

~~~text
AST
symbol resolution
type analysis
validated AST
structured coroutine lowering
~~~

rather than leaving async semantics in compatibility/source-rewrite passes.

# Canonical syntax

Ordinary function:

~~~gnr
async function User loadUser(int id) {
    return await users.find(id);
}
~~~

Controller:

~~~gnr
public async show(int id) {
    const user = await users.find(id);

    return json(user);
}
~~~

Middleware:

~~~gnr
public async handle(Request request, Next next) {
    return await next(request);
}
~~~

Canonical framework ordering is:

~~~text
public async
~~~

not:

~~~text
async public
~~~

# Design rule

~~~text
async = operation may suspend
await = explicit suspension point
logical return type = application value
native Task/coroutine = implementation detail
queue/background = separate lifecycle
~~~

Gungnir should make asynchronous behavior visible and safe without making application developers write C++ coroutine plumbing.

