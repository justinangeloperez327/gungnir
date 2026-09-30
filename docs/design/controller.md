# Controllers

> **Design specification.** This document preserves the intended contract. Examples and requirements below may exceed the current implementation. See the [current implementation guide](../controller.md) before using an API.

A Gungnir controller is the **HTTP orchestration layer** of an application.

Controllers receive request input, coordinate simple ORM or application-service work, and return HTTP responses.

They are intentionally simpler than general-purpose C++ classes.

A controller may contain only:

- injected dependencies;
- public HTTP actions.

Controllers do not declare arbitrary fields, constructors, destructors, or unrestricted helper methods.

## Basic controller

A controller is declared with the `controller` keyword:

```gnr
controller UserController {
    public index() {
        return json(User::all());
    }
}
```

The `public` keyword marks `index()` as a routable controller action.

Controller actions have an implicit HTTP response contract, so developers do not write `Response` repeatedly.

Do this:

```gnr
public show(int id) {
    return json(User::findOrFail(id));
}
```

not:

```gnr
Response show(int id) {
    return json(User::findOrFail(id));
}
```

The compiler still type-checks the action as response-producing even though `Response` is omitted from source.

# Controller responsibility

A controller should answer:

```text
Which HTTP action is being handled?
What request or route parameters does the action receive?
Which dependencies does the controller need?
Which ORM or application operation should be called?
Which HTTP response should be returned?
```

Controllers should remain thin orchestration boundaries.

Simple CRUD may call the ORM directly:

```gnr
public show(int id) {
    const user = User::findOrFail(id);

    return json(user);
}
```

Complex workflows should be delegated to an application/service layer:

```gnr
controller OrderController {
    inject OrderService orders;

    public store(Request request) {
        const data = request.validate({
            'customer_id': 'required',
            'items': 'required|array'
        });

        const order = orders.create(data);

        return json(order, 201);
    }
}
```

# Public actions

Every controller action is declared with `public`:

```gnr
controller UserController {
    public index() {
        // ...
    }

    public store(Request request) {
        // ...
    }

    public show(int id) {
        // ...
    }

    public update(Request request, int id) {
        // ...
    }

    public destroy(int id) {
        // ...
    }
}
```

For the initial controller contract, only `public` actions are supported.

Gungnir does not initially expose `private` or `protected` controller methods.

If logic is substantial enough to require reusable private behavior, it should normally move into an injected service rather than turning the controller into a general-purpose class.

# Resource controller conventions

Gungnir follows familiar resource-controller naming conventions:

```text
index
create
store
show
edit
update
destroy
```

Example:

```gnr
controller UserController {
    public index() {
        // list users
    }

    public create() {
        // show create form
    }

    public store(Request request) {
        // create user
    }

    public show(int id) {
        // show user
    }

    public edit(int id) {
        // show edit form
    }

    public update(Request request, int id) {
        // update user
    }

    public destroy(int id) {
        // delete user
    }
}
```

These names are conventions, not reserved keywords.

Custom actions are allowed:

```gnr
public activate(int id) {
    const user = User::findOrFail(id);

    user.update({
        'active': true
    });

    return json(user);
}
```

# Implicit response contract

All controller actions are expected to return a response-compatible value.

The source language does not require an explicit return type:

```gnr
public index() {
    return json(User::all());
}
```

Semantically, the compiler treats the action approximately as:

```text
ControllerAction
  expectedResult = Response
```

Normal response-producing helpers include:

```text
response(...)
json(...)
text(...)
view(...)
redirect(...)
file(...)
download(...)
```

The detailed response API belongs in `response.md`.

## Explicit response intent

Gungnir should prefer explicit HTTP response helpers.

Instead of returning a raw value:

```gnr
public hello() {
    return 'hello';
}
```

prefer:

```gnr
public hello() {
    return text('hello');
}
```

Instead of returning a raw object:

```gnr
public show(int id) {
    return User::findOrFail(id);
}
```

prefer:

```gnr
public show(int id) {
    return json(User::findOrFail(id));
}
```

This keeps HTTP behavior explicit and predictable.

# Action parameters

Controller actions may receive typed parameters.

```gnr
public show(int id) {
    // ...
}
```

Multiple parameters are supported:

```gnr
public update(Request request, int id) {
    // ...
}
```

Parameter syntax follows the normal Gungnir function parameter grammar:

```text
Type name
```

Examples:

```gnr
int id
string slug
Request request
User user
```

The compiler must resolve all parameter types before C++23 generation.

# Request injection

An action may request the current HTTP request:

```gnr
public store(Request request) {
    const name = request.input('name');

    return json({
        'name': name
    });
}
```

`Request` is framework-provided action input and does not need to be manually constructed.

The complete request API belongs in `request.md`.

# Validation in controllers

Controllers may coordinate request validation:

```gnr
public store(Request request) {
    const data = request.validate({
        'name': 'required|string',
        'email': 'required|email'
    });

    const user = User::create(data);

    return json(user, 201);
}
```

Validation is acceptable controller behavior because it belongs to HTTP input handling.

The validation language, rule set, errors, nested data, and custom messages belong in `validation.md`.

# Route parameters

Route values may bind to typed scalar action parameters:

```gnr
public show(int id) {
    const user = User::findOrFail(id);

    return json(user);
}
```

A route such as:

```text
/users/{id}
```

can bind the route value to `id`.

The routing contract determines how route names and action parameters are matched.

# Route model binding

Gungnir should support model binding directly:

```gnr
public show(User user) {
    return json(user);
}
```

The router and ORM are responsible for resolving the route value to the requested model.

This makes actions such as:

```gnr
public update(Request request, User user) {
    user.update(request.only([
        'name',
        'email'
    ]));

    return json(user);
}
```

possible without manual `findOrFail()` calls.

Missing-model behavior should resolve through the normal model-not-found / HTTP 404 contract.

Detailed binding rules belong in `routing.md`.

# Dependency injection

Controller dependencies use the `inject` declaration:

```gnr
controller OrderController {
    inject OrderService orders;

    public show(int id) {
        const order = orders.find(id);

        return json(order);
    }
}
```

The syntax is:

```text
inject Type name;
```

For example:

```gnr
inject UserService users;
inject PaymentService payments;
inject Logger logger;
```

Gungnir's dependency container resolves these dependencies and the compiler generates the required native constructor wiring.

Application code should not need to declare controller constructors solely for dependency injection.

# No explicit constructors

Do not write:

```gnr
controller UserController {
    UserController(UserService users) {
        this.users = users;
    }
}
```

Use:

```gnr
controller UserController {
    inject UserService users;
}
```

The controller declaration describes the dependency requirement. Constructor mechanics belong to generated/native code.

# No arbitrary fields

Controllers are not state containers.

This should not be part of the controller language:

```gnr
controller UserController {
    string title = 'Users';
    int counter = 0;
}
```

Controller members should be restricted to:

```text
inject declarations
public actions
```

This keeps controller instances predictable and prevents request-handling state from becoming hidden mutable application state.

# Synchronous actions

A normal action is synchronous at the language level:

```gnr
public index() {
    const users = User::orderBy('name')
        .paginate(25);

    return json(users);
}
```

The generated C++ implementation may still use framework/runtime abstractions internally, but synchronous source does not expose coroutine syntax.

# Asynchronous actions

Use `public async` when an action contains asynchronous work:

```gnr
public async import(Request request) {
    const file = request.file('users');

    const result = await importer.import(file);

    return json(result);
}
```

Canonical ordering is:

```text
public async actionName(parameters)
```

not:

```text
async public actionName(parameters)
```

An asynchronous controller action still has the same implicit HTTP response contract.

Conceptually:

```text
Gungnir source          generated/native contract

public index()           Response
public async import()    Task<Response>
```

The native coroutine type is an implementation detail.

# Await rules

`await` is only valid inside an asynchronous action:

```gnr
public async index() {
    const users = await service.users();

    return json(users);
}
```

This should be rejected:

```gnr
public index() {
    const users = await service.users();

    return json(users);
}
```

The semantic analyzer must report that `await` requires an async action.

The full async language contract belongs in `async.md`.

# Normal statements inside actions

Unlike models, controller actions contain normal executable Gungnir statements.

Examples include:

```text
const bindings
assignments
function calls
method calls
if / else
loops
return
await in async actions
object literals
list literals
ORM calls
service calls
```

Example:

```gnr
public show(int id) {
    const user = User::find(id);

    if (user == null) {
        return json({
            'message': 'User not found'
        }, 404);
    }

    return json(user);
}
```

These statements use the normal language AST rather than a special controller-only statement language.

# Direct ORM usage

Simple ORM access is valid inside controllers.

```gnr
public index() {
    const users = User::where('active', true)
        .orderBy('name')
        .paginate(25);

    return json(users);
}
```

```gnr
public destroy(int id) {
    const user = User::findOrFail(id);

    user.delete();

    return response(null, 204);
}
```

Gungnir does not require a service class for trivial CRUD operations.

# Application services

Use an injected service when an action coordinates substantial application behavior.

Prefer:

```gnr
controller OrderController {
    inject OrderService orders;

    public store(Request request) {
        const data = request.validate({
            'customer_id': 'required',
            'items': 'required|array'
        });

        const order = orders.create(data);

        return json(order, 201);
    }
}
```

over placing a large multi-system workflow directly inside the controller.

Controller thinness is an architectural convention rather than an arbitrary compiler line-count rule.

# What does not belong in a controller

Controllers should not become general-purpose application objects.

Avoid placing these directly in controllers when they represent substantial application behavior:

- domain/business workflows;
- complex transaction orchestration;
- reusable domain calculations;
- long-running jobs;
- infrastructure implementation;
- mail composition;
- event handling;
- persistent mutable state;
- database-driver plumbing;
- C++ ownership or coroutine plumbing.

The controller should normally coordinate those responsibilities through framework APIs or injected services.

# Complete CRUD example

```gnr
controller UserController {
    public index() {
        const users = User::orderBy('name')
            .paginate(25);

        return json(users);
    }

    public store(Request request) {
        const data = request.validate({
            'name': 'required|string',
            'email': 'required|email'
        });

        const user = User::create(data);

        return json(user, 201);
    }

    public show(User user) {
        return json(user);
    }

    public update(Request request, User user) {
        const data = request.validate({
            'name': 'required|string',
            'email': 'required|email'
        });

        user.update(data);

        return json(user);
    }

    public destroy(User user) {
        user.delete();

        return response(null, 204);
    }
}
```

# Complete service-oriented example

```gnr
controller ImportController {
    inject UserImportService importer;

    public async store(Request request) {
        const data = request.validate({
            'users': 'required|file'
        });

        const result = await importer.import(data.users);

        return json(result, 202);
    }
}
```

# Controller and routing separation

Controllers define actions.

Routes decide which HTTP request invokes those actions.

Controller:

```gnr
controller UserController {
    public show(User user) {
        return json(user);
    }
}
```

Routing:

```gnr
Route::get('/users/{user}', UserController::show);
```

Routing syntax, middleware attachment, route names, groups, prefixes, and route-model binding rules belong in `routing.md`.

# Controller and response separation

Controller actions return response-compatible values, but the response API itself belongs in `response.md`.

Examples include:

```gnr
return json(user);
return view('users/show', {'user': user});
return redirect('/users');
return text('OK');
return response(null, 204);
```

# Controller and request separation

Controllers may receive `Request request`, but request accessors belong in `request.md`.

Examples include:

```gnr
request.input('name');
request.query('page');
request.header('authorization');
request.cookie('session');
request.file('avatar');
request.only(['name', 'email']);
request.except(['password']);
```

# Controller grammar

The intended controller grammar is approximately:

```text
controllerDeclaration
  := 'controller' Identifier '{'
       controllerMember*
     '}'

controllerMember
  := injectDeclaration
   | controllerAction

injectDeclaration
  := 'inject' Type Identifier ';'

controllerAction
  := 'public' 'async'? Identifier '(' parameterList? ')' block
```

There is no explicit return type in a controller action declaration.

The return contract is supplied by the controller context.

# Controller AST

A controller should ultimately be represented structurally as:

```text
ControllerDeclaration
  name
  injections[]
  actions[]
```

An injection:

```text
InjectDeclaration
  type
  name
```

An action:

```text
ControllerAction
  name
  visibility = public
  async
  parameters[]
  body
  responseContract = Response
```

A parameter:

```text
MethodParameter
  type
  name
  defaultValue?
```

The controller should not be represented as an unrestricted generic C++ class.

# Semantic validation

The controller semantic pass should validate at least:

- controller names are valid and unique;
- only supported controller members are present;
- injection types resolve;
- injection names are unique;
- action names are unique within the controller;
- action parameter types resolve;
- route-model parameter types resolve to known models;
- `await` appears only in async actions;
- every reachable action return is response-compatible;
- invalid raw controller fields are rejected;
- constructors and destructors are rejected;
- non-public controller methods are rejected until such syntax is intentionally added;
- route handlers resolve to existing public actions when routing information is available;
- route parameters and action parameters are structurally compatible when routing information is available.

# Compiler contract

This controller:

```gnr
controller UserController {
    inject UserService users;

    public show(User user) {
        return json(user);
    }

    public async sync(User user) {
        const result = await users.sync(user);

        return json(result);
    }
}
```

should conceptually pass through:

```text
source
  -> lexer
  -> parser
  -> ControllerDeclaration AST
  -> InjectDeclaration / ControllerAction AST
  -> symbol resolution
  -> parameter and dependency type resolution
  -> async/await validation
  -> response-contract validation
  -> validated controller AST
  -> controller lowering
  -> C++23 generation
```

Before C++23 generation, the validated representation should already know:

```text
controller       UserController

dependency       users
dependency type  UserService

action           show
public           true
async            false
parameter        User user
result contract  Response

action           sync
public           true
async            true
parameter        User user
result contract  Response
native result    coroutine-backed Response
```

The transpiler must not rediscover controller actions, injection declarations, async behavior, or response expectations by scanning raw source text.

# Generated C++ boundary

A synchronous Gungnir action:

```gnr
public show(int id) {
    return json(User::findOrFail(id));
}
```

may lower to native C++ approximately equivalent to:

```cpp
Response UserController::show(std::int64_t id) {
    return json(User::find_or_fail(id));
}
```

An async action:

```gnr
public async import(Request request) {
    const result = await importer.import(request.file('users'));

    return json(result);
}
```

may lower to a native coroutine-backed return type such as:

```text
Task<Response>
```

The exact native representation is not part of the application language contract.

# Design rule

The controller contract is intentionally focused:

```text
controller = injected dependencies + public HTTP actions
```

Controller actions have an implicit response contract.

Normal source therefore uses:

```gnr
public index() {
    return json(...);
}
```

and:

```gnr
public async import(Request request) {
    const result = await service.import(...);

    return json(result);
}
```

rather than repeating native response or coroutine types in every action declaration.

