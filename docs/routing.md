# Routing

Gungnir routing maps HTTP requests to controller actions or small inline handlers.

The routing API follows familiar Laravel-style conventions while remaining native to the Gungnir language.

Routes are application declarations. The router, container, middleware pipeline, parameter binding, and native C++ dispatch machinery are framework implementation details.

## Basic routes

A route associates an HTTP method and URI with a handler:

```gnr
Route::get('/', HomeController::index);
```

Additional HTTP methods:

```gnr
Route::post('/users', UserController::store);
Route::put('/users/{user}', UserController::update);
Route::patch('/users/{user}', UserController::update);
Route::delete('/users/{user}', UserController::destroy);
Route::options('/users', UserController::options);
Route::head('/users', UserController::head);
```

Controller handlers reference a controller and one of its `public` actions:

```gnr
controller UserController {
    public index() {
        return json(User::all());
    }
}
```

```gnr
Route::get('/users', UserController::index);
```

Only public controller actions may be used as route handlers.

# Supported HTTP methods

The core routing contract supports:

```text
GET
POST
PUT
PATCH
DELETE
OPTIONS
HEAD
```

Canonical Gungnir APIs are:

```text
Route::get()
Route::post()
Route::put()
Route::patch()
Route::delete()
Route::options()
Route::head()
```

# Route parameters

Dynamic path parameters use braces:

```gnr
Route::get('/users/{id}', UserController::show);
```

The matching controller parameter may be:

```gnr
controller UserController {
    public show(int id) {
        const user = User::findOrFail(id);

        return json(user);
    }
}
```

The route parameter name and action parameter should match unless an explicit binding rule says otherwise.

Multiple parameters are supported:

```gnr
Route::get(
    '/users/{user}/posts/{post}',
    PostController::show
);
```

```gnr
controller PostController {
    public show(User user, Post post) {
        return json(post);
    }
}
```

# Optional parameters

Optional URI parameters use `?`:

```gnr
Route::get('/users/{page?}', UserController::index);
```

The receiving action must use a compatible optional/default parameter contract.

For example:

```gnr
public index(int? page = null) {
    // ...
}
```

Optional route parameters must appear after required parameters in the same path segment sequence.

# Route model binding

A route parameter may bind directly to a model:

```gnr
Route::get('/users/{user}', UserController::show);
```

```gnr
controller UserController {
    public show(User user) {
        return json(user);
    }
}
```

The router and ORM resolve `{user}` to a `User` model before invoking the action.

Conceptually:

```text
/users/42
   -> route parameter user = 42
   -> resolve User
   -> User::findOrFail(42)
   -> invoke UserController::show(user)
```

A missing bound model uses the framework's model-not-found behavior and normally becomes HTTP 404.

# Custom model binding keys

A route may bind a model by a field other than its primary key.

Canonical shorthand:

```gnr
Route::get(
    '/posts/{post:slug}',
    PostController::show
);
```

With:

```gnr
controller PostController {
    public show(Post post) {
        return json(post);
    }
}
```

Conceptually:

```text
{post:slug}
   -> model Post
   -> lookup column slug
```

The binding key must be a valid mapped database attribute.

# Scoped model binding

Nested model bindings should support relationship-aware scoping.

For example:

```gnr
Route::get(
    '/users/{user}/posts/{post}',
    PostController::show
).scopeBindings();
```

The `post` binding should then be resolved within the parent user's relationship rather than globally where the relationship metadata allows it.

This prevents a nested URL from binding an unrelated child record accidentally.

# Named routes

Assign a route name with `name()`:

```gnr
Route::get('/users', UserController::index)
    .name('users.index');
```

```gnr
Route::get('/users/{user}', UserController::show)
    .name('users.show');
```

Route names must be unique.

Duplicate route names should be rejected during route registration or compilation.

# URL generation

Named routes may be used to generate URLs:

```gnr
const url = route('users.index');
```

Parameters are supplied for dynamic routes:

```gnr
const url = route('users.show', {
    'user': user.id
});
```

All required route parameters must be supplied.

Extra query values may be appended according to the route helper contract defined by the URL-generation layer.

# Redirects to named routes

A response may redirect using a route name:

```gnr
return redirect()
    .route('users.show', {
        'user': user.id
    });
```

The detailed redirect API belongs in `response.md`.

# Route constraints

Route parameters may be constrained.

## Generic constraint

```gnr
Route::get('/users/{id}', UserController::show)
    .where('id', '[0-9]+');
```

## Numeric constraint

```gnr
Route::get('/users/{id}', UserController::show)
    .whereNumber('id');
```

## UUID constraint

```gnr
Route::get('/users/{user}', UserController::show)
    .whereUuid('user');
```

## Alphabetic constraint

```gnr
Route::get('/categories/{category}', CategoryController::show)
    .whereAlpha('category');
```

## Alphanumeric constraint

```gnr
Route::get('/codes/{code}', CodeController::show)
    .whereAlphaNumeric('code');
```

## Multiple constraints

```gnr
Route::get(
    '/users/{user}/posts/{post}',
    PostController::show
)
    .whereNumber('user')
    .whereUuid('post');
```

Constraint names must correspond to parameters present in the route path.

# Middleware

Attach middleware to a route with `middleware()`:

```gnr
Route::get('/profile', ProfileController::show)
    .middleware(AuthMiddleware);
```

Multiple middleware may be supplied:

```gnr
Route::post('/orders', OrderController::store)
    .middleware([
        AuthMiddleware,
        VerifiedMiddleware
    ]);
```

Middleware order is significant and should be preserved.

Detailed middleware declaration and execution rules belong in `middleware.md`.

# Route groups

Related routes may share configuration through a group.

```gnr
Route::group(() => {
    Route::get('/users', UserController::index);
    Route::post('/users', UserController::store);
});
```

Groups become most useful when combined with prefixes, middleware, or route-name prefixes.

# Prefix groups

```gnr
Route::prefix('/admin')
    .group(() => {
        Route::get('/users', AdminUserController::index);
        Route::get('/orders', AdminOrderController::index);
    });
```

The resulting paths are:

```text
/admin/users
/admin/orders
```

# Middleware groups

```gnr
Route::middleware(AuthMiddleware)
    .group(() => {
        Route::get('/profile', ProfileController::show);
        Route::post('/logout', AuthController::logout);
    });
```

Multiple middleware may be applied:

```gnr
Route::middleware([
        AuthMiddleware,
        VerifiedMiddleware
    ])
    .group(() => {
        Route::get('/dashboard', DashboardController::index);
    });
```

# Combined groups

Group configuration may be chained:

```gnr
Route::prefix('/admin')
    .middleware([
        AuthMiddleware,
        AdminMiddleware
    ])
    .group(() => {
        Route::get('/users', AdminUserController::index);
        Route::get('/reports', ReportController::index);
    });
```

The group configuration applies to every route declared inside it.

# Route name prefixes

Route groups may share a route-name prefix:

```gnr
Route::prefix('/admin')
    .name('admin.')
    .group(() => {
        Route::get('/users', AdminUserController::index)
            .name('users.index');

        Route::get('/orders', AdminOrderController::index)
            .name('orders.index');
    });
```

The final route names are:

```text
admin.users.index
admin.orders.index
```

# Domain routing

Where host-based routing is enabled, a route group may constrain the domain:

```gnr
Route::domain('{account}.example.com')
    .group(() => {
        Route::get('/dashboard', DashboardController::index);
    });
```

The domain parameter may participate in normal parameter binding where supported.

Domain routing remains an HTTP routing concern and should not leak web-server-specific configuration into application code.

# Resource routes

Gungnir should provide resource routing for conventional CRUD controllers.

```gnr
Route::resource('/users', UserController);
```

Conceptually this expands to:

```text
GET     /users              -> index
GET     /users/create       -> create
POST    /users              -> store
GET     /users/{user}       -> show
GET     /users/{user}/edit  -> edit
PUT     /users/{user}       -> update
PATCH   /users/{user}       -> update
DELETE  /users/{user}       -> destroy
```

The controller actions remain ordinary public actions:

```gnr
controller UserController {
    public index() {}
    public create() {}
    public store(Request request) {}
    public show(User user) {}
    public edit(User user) {}
    public update(Request request, User user) {}
    public destroy(User user) {}
}
```

# API resource routes

API-only resource routing omits form-oriented `create` and `edit` routes:

```gnr
Route::apiResource('/users', UserController);
```

Conceptually:

```text
GET     /users         -> index
POST    /users         -> store
GET     /users/{user}  -> show
PUT     /users/{user}  -> update
PATCH   /users/{user}  -> update
DELETE  /users/{user}  -> destroy
```

# Restricting resource actions

Resource routes may expose only selected actions:

```gnr
Route::resource('/users', UserController)
    .only([
        'index',
        'show'
    ]);
```

Or exclude actions:

```gnr
Route::resource('/users', UserController)
    .except([
        'create',
        'edit'
    ]);
```

Requested resource actions must resolve to public controller actions.

# Multiple HTTP methods

When several methods intentionally share one handler:

```gnr
Route::match(
    ['get', 'post'],
    '/search',
    SearchController::index
);
```

# Any HTTP method

A route may intentionally accept any supported HTTP method:

```gnr
Route::any('/health', HealthController::handle);
```

This should be used sparingly because explicit HTTP methods provide clearer routing contracts.

# Inline handlers

Small routes may use an inline handler:

```gnr
Route::get('/health', () => {
    return json({
        'status': 'ok'
    });
});
```

Inline route handlers use the same implicit response contract as controller actions.

An inline handler may receive supported route/request parameters:

```gnr
Route::get('/hello/{name}', (string name) => {
    return text('Hello ' + name);
});
```

Substantial HTTP behavior should normally move to a controller.

# Redirect routes

Simple redirects may be declared directly:

```gnr
Route::redirect('/old-path', '/new-path');
```

A status may be supplied where supported:

```gnr
Route::redirect('/old-path', '/new-path', 301);
```

# View routes

Simple view-only routes may use:

```gnr
Route::view('/about', 'about');
```

With view data:

```gnr
Route::view('/about', 'about', {
    'title': 'About'
});
```

If a route needs substantial logic, use a controller instead.

# Fallback route

An application may define a fallback handler:

```gnr
Route::fallback(() => {
    return view('errors/404');
});
```

Or use a controller action:

```gnr
Route::fallback(ErrorController::notFound);
```

Only one effective application fallback should exist for a routing scope.

# Route precedence

Static and constrained routes should be compiled into deterministic precedence.

For example:

```gnr
Route::get('/users/create', UserController::create);
Route::get('/users/{user}', UserController::show);
```

must route `/users/create` to `create`, not treat `create` as a model identifier.

Route dispatch behavior must not depend on accidental source-map or hash-map iteration order.

Ambiguous route declarations should be diagnosable where possible.

# Trailing slashes

The router should apply one consistent application-wide rule for trailing slashes.

Application authors should not need to register both:

```text
/users
/users/
```

unless the framework is explicitly configured to distinguish them.

The exact normalization policy belongs to router configuration, not individual route handlers.

# Query strings

Query strings are not part of route matching.

For:

```text
/users?page=2&active=true
```

the route path is:

```text
/users
```

Query-string values are accessed through `Request` and are documented in `request.md`.

# Route middleware order

Middleware should execute in deterministic order.

For:

```gnr
Route::get('/admin', AdminController::index)
    .middleware([
        AuthMiddleware,
        VerifiedMiddleware,
        AdminMiddleware
    ]);
```

the request passes through the middleware in declared order before reaching the controller action.

Response unwinding follows the middleware contract defined in `middleware.md`.

# Global middleware

Application-wide middleware may be registered by the application/bootstrap layer.

Global middleware is not repeated on every route.

Routing documentation only defines route-level and group-level attachment. Global middleware registration belongs in the application/bootstrap documentation.

# Route model binding and ownership

Nested resources should not automatically imply ownership unless scoped binding is requested or a route contract explicitly defines it.

For example:

```text
/users/{user}/posts/{post}
```

must not silently assume `post` belongs to `user` unless:

```gnr
.scopeBindings()
```

or an equivalent explicit nested-resource rule is active.

This keeps lookup semantics predictable.

# What does not belong in routing

Routing declarations should not contain business workflows.

Avoid:

```gnr
Route::post('/orders', () => {
    // large payment workflow
    // inventory orchestration
    // mail sending
    // ERP synchronization
});
```

Prefer:

```gnr
Route::post('/orders', OrderController::store);
```

Routes describe HTTP dispatch.

Controllers and services perform application work.

# Complete web routing example

```gnr
Route::get('/', HomeController::index)
    .name('home');

Route::prefix('/users')
    .name('users.')
    .group(() => {
        Route::get('/', UserController::index)
            .name('index');

        Route::get('/create', UserController::create)
            .middleware(AuthMiddleware)
            .name('create');

        Route::post('/', UserController::store)
            .middleware(AuthMiddleware)
            .name('store');

        Route::get('/{user}', UserController::show)
            .name('show');

        Route::get('/{user}/edit', UserController::edit)
            .middleware(AuthMiddleware)
            .name('edit');

        Route::put('/{user}', UserController::update)
            .middleware(AuthMiddleware)
            .name('update');

        Route::delete('/{user}', UserController::destroy)
            .middleware(AuthMiddleware)
            .name('destroy');
    });

Route::fallback(ErrorController::notFound);
```

# Complete API routing example

```gnr
Route::prefix('/api')
    .middleware(ApiMiddleware)
    .group(() => {
        Route::apiResource('/users', UserController);

        Route::get(
            '/users/{user}/posts/{post}',
            PostController::show
        )
            .scopeBindings()
            .name('users.posts.show');
    });
```

# Routing and controllers

Routes reference public controller actions.

```gnr
Route::get('/users/{user}', UserController::show);
```

```gnr
controller UserController {
    public show(User user) {
        return json(user);
    }
}
```

The controller does not need to declare `Response`; controller actions already have an implicit response contract.

# Routing and middleware

Routes attach middleware:

```gnr
Route::get('/dashboard', DashboardController::index)
    .middleware(AuthMiddleware);
```

The middleware implementation itself belongs in `middleware.md`.

# Routing and models

Routes may request model binding:

```gnr
Route::get('/users/{user}', UserController::show);
```

Models define database mapping and relationships.

The ORM performs model lookup.

The router coordinates binding.

These responsibilities remain separate.

# Routing grammar

The exact expression grammar is defined by the general Gungnir language specification, but routing should resolve structurally as framework route declarations.

Conceptually:

```text
routeDeclaration
  := Route '::' routeOperation '(' arguments ')' routeModifier*

routeOperation
  := get
   | post
   | put
   | patch
   | delete
   | options
   | head
   | match
   | any
   | resource
   | apiResource
   | redirect
   | view
   | fallback

routeModifier
  := name
   | middleware
   | where
   | whereNumber
   | whereUuid
   | whereAlpha
   | whereAlphaNumeric
   | scopeBindings
   | only
   | except
```

Group builders include:

```text
prefix
middleware
name
domain
group
```

The parser should not depend on raw source scanning to discover route metadata.

# Route AST

Routes should ultimately be represented structurally.

A normal route:

```text
RouteDeclaration
  method
  path
  handler
  name?
  middleware[]
  constraints[]
  scopedBindings
```

A controller handler:

```text
ControllerRouteHandler
  controller
  action
```

An inline handler:

```text
InlineRouteHandler
  parameters[]
  body
  responseContract = Response
```

A route group:

```text
RouteGroup
  prefix?
  namePrefix?
  domain?
  middleware[]
  routes[]
```

A resource route should be represented as a resource declaration or expanded deterministically into validated route declarations before final routing/code-generation lowering.

# Semantic validation

The routing semantic pass should validate at least:

- HTTP method names are valid;
- route paths are valid;
- route parameter names are unique within a route;
- optional parameters appear in valid positions;
- constraint names refer to actual route parameters;
- controller types resolve;
- controller actions resolve;
- referenced controller actions are public;
- route action parameters are compatible with path parameters;
- model-bound parameters resolve to known models;
- custom binding keys are structurally valid;
- middleware references resolve;
- route names are unique;
- resource actions resolve where required;
- `only()` and `except()` contain valid resource action names;
- duplicate or structurally ambiguous routes are diagnosed where deterministically detectable;
- route groups preserve inherited prefix, middleware, name, and domain metadata;
- fallback definitions do not conflict;
- inline handlers satisfy the response contract.

# Compiler contract

A route such as:

```gnr
Route::get('/users/{user}', UserController::show)
    .middleware(AuthMiddleware)
    .whereNumber('user')
    .name('users.show');
```

should conceptually pass through:

```text
source
  -> lexer
  -> parser
  -> RouteDeclaration AST
  -> controller/action resolution
  -> middleware resolution
  -> path/parameter analysis
  -> route-model binding analysis
  -> semantic validation
  -> validated route metadata
  -> route lowering
  -> immutable/native dispatch representation
  -> C++23 generation/runtime registration
```

Before native generation, the validated route should already know:

```text
method             GET
path               /users/{user}

parameter          user
constraint         number

controller         UserController
action             show
action public      true

binding type       User
binding key        primaryKey

middleware         AuthMiddleware
route name         users.show
```

The transpiler must not rediscover route paths, handlers, middleware, names, constraints, or bindings by scanning raw source text after parsing.

# Generated C++ boundary

Application code:

```gnr
Route::get('/users/{user}', UserController::show)
    .name('users.show');
```

may lower to native router registration and controller-member dispatch code.

The generated C++ representation is not part of the application-facing routing syntax.

Users should not need to write controller member pointers, container-resolution plumbing, coroutine adapters, route-binding registries, or native router templates manually.

# Naming convention

Public Gungnir routing APIs use camelCase where multiple words are required:

```text
drop-in single words:
get
post
put
patch
delete
options
head
name
middleware
prefix
domain
group
resource

camelCase:
apiResource
whereNumber
whereUuid
whereAlpha
whereAlphaNumeric
scopeBindings
```

The native C++ implementation may use different internal naming.

# Design rule

The routing contract is intentionally focused:

```text
route = HTTP method/path + handler + routing metadata
```

Controllers define public HTTP actions.

Middleware wraps request execution.

Models describe persistence mapping.

The ORM resolves persisted records.

Routes only connect the incoming HTTP request to those framework concepts.
