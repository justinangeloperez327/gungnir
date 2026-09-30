# Gungnir

**Gungnir is an expressive web framework built in C++23.**

Gungnir is designed to provide a Laravel/Adonis-style application-development experience while retaining the performance, deployment model, and interoperability of native C++.

Application code is written in Gungnir's .gnr language:

~~~gnr
model User {
    table = 'users';

    fillable = [
        'name',
        'email'
    ];

    posts() {
        return hasMany('posts');
    }
}
~~~

~~~gnr
controller UserController {
    inject UserService users;

    public async show(int id) {
        const user = await users.find(id);

        if (user == null) {
            return response(null, 404);
        }

        return json(user);
    }
}
~~~

~~~gnr
Route::get('/users/{user}', UserController::show)
    .middleware(AuthMiddleware)
    .name('users.show');
~~~

Gungnir compiles application source into ordinary, inspectable C++23.

---

## Goals

Gungnir is built around a small set of principles:

- expressive application syntax;
- convention over boilerplate;
- strongly typed application code;
- first-class models, controllers, middleware, migrations, policies, events, listeners, notifications, and mail;
- Eloquent-style ORM behavior;
- explicit async/await without exposing C++ coroutine plumbing;
- native performance and C++ interoperability;
- generated C++ that remains understandable;
- clear compiler phase boundaries;
- backend/runtime capabilities that do not leak into normal application code.

Gungnir is not intended to reproduce the entire C++ language inside .gnr.

The language is deliberately smaller and focused on web and application development.

---

## Status

Gungnir is under active development and is **pre-1.0**.

The source language, compiler architecture, runtime APIs, and generated-code ABI may still change while the framework moves toward a coherent stable contract.

The canonical documentation defines the target language and framework behavior before compiler/runtime implementation is considered complete.

Some runtime subsystems are already substantial, while parts of the language frontend and validated lowering pipeline are still being migrated from earlier compatibility/source-rewrite implementations.

Do not assume every documented target-language feature is already fully implemented by the current compiler.

See [docs/stability.md](docs/stability.md) for the compatibility policy.

---

## Compiler Architecture

The target compiler architecture is:

~~~text
.gnr source
    ↓
Lexer
    ↓
Tokens
    ↓
Parser
    ↓
Syntax AST
    ↓
Module / Symbol Resolution
    ↓
Semantic + Type Analysis
    ↓
Control-Flow / Framework Validation
    ↓
Validated AST
    ↓
Framework Lowering
    ↓
C++23 IR
    ↓
C++23 Emitter
    ↓
Native C++ Compiler
    ↓
Application
~~~

The compiler follows one important rule:

> **Parse once, resolve once, validate once, then lower deterministic compiler structures.**

Supported Gungnir syntax should not be rediscovered later through raw-source scanning or string matching.

Key compiler specifications:

- [Grammar](docs/grammar.md)
- [Syntax AST](docs/ast.md)
- [Semantics](docs/semantics.md)
- [Validated AST](docs/validated-ast.md)
- [Transpiler](docs/transpiler.md)

---

## Language

Gungnir application code uses application-oriented types and control flow without exposing native pointer, reference, allocator, template, or coroutine syntax.

### Types

~~~gnr
const string name = 'Gungnir';
const int limit = 25;
const bool active = true;

const User? user = User::find(id);
const List<string> roles = ['admin', 'editor'];
~~~

Both single-quoted and double-quoted literals are Gungnir strings.

Optional types use:

~~~text
T?
~~~

Examples:

~~~text
User?
string?
int?
~~~

See [docs/language-types.md](docs/language-types.md).

### Functions

~~~gnr
function string fullName(
    string first,
    string last
) {
    return first + ' ' + last;
}
~~~

Async functions expose the logical return type rather than native coroutine wrappers:

~~~gnr
async function User loadUser(int id) {
    return await users.find(id);
}
~~~

Generated C++ may use coroutine-backed runtime types such as Task<User>, but those are compiler/runtime implementation details.

See:

- [Functions](docs/functions.md)
- [Async and Await](docs/async.md)
- [Expressions](docs/expressions.md)
- [Statements](docs/statements.md)

---

## Framework Declarations

Gungnir treats common application concepts as first-class language declarations.

### Model

~~~gnr
model User {
    table = 'users';
    primaryKey = 'id';

    fillable = [
        'name',
        'email'
    ];

    timestamps = true;

    posts() {
        return hasMany('posts');
    }
}
~~~

Models define persistence metadata and relationships.

Database schema belongs to migrations.

See [docs/model.md](docs/model.md).

### Migration

~~~gnr
migration CreateUsersTable {
    up() {
        Table::create('users', (table) => {
            table.id();
            table.string('name');
            table.string('email').unique();
            table.timestamps();
        });
    }

    down() {
        Table::dropIfExists('users');
    }
}
~~~

See [docs/migration.md](docs/migration.md).

### Controller

~~~gnr
controller UserController {
    inject UserService users;

    public index() {
        return view('users/index', {
            'users': User::orderBy('name').get()
        });
    }

    public async show(int id) {
        const user = await users.find(id);

        if (user == null) {
            return response(null, 404);
        }

        return json(user);
    }
}
~~~

Controller actions have an implicit Response contract.

See [docs/controller.md](docs/controller.md).

### Middleware

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

See [docs/middleware.md](docs/middleware.md).

### Policy

~~~gnr
policy PostPolicy {
    public update(User user, Post post) {
        return user.id == post.user_id;
    }
}
~~~

See [docs/policy.md](docs/policy.md).

### Events and listeners

~~~gnr
event UserRegistered {
    User user;
}
~~~

~~~gnr
listener SendWelcomeNotification {
    public handle(UserRegistered event) {
        Notification::send(
            event.user,
            WelcomeNotification()
        );
    }
}
~~~

See:

- [Events](docs/event.md)
- [Listeners](docs/listener.md)

### Notifications and mail

~~~gnr
notification WelcomeNotification {
    public via(User user) {
        return ['mail'];
    }

    public mail(User user) {
        return WelcomeMail(
            user: user
        );
    }
}
~~~

~~~gnr
mail WelcomeMail {
    User user;

    public subject() {
        return 'Welcome to Gungnir';
    }

    public content() {
        return view('mail/welcome', {
            'user': user
        });
    }
}
~~~

See:

- [Notifications](docs/notification.md)
- [Mail](docs/mail.md)

---

## Routing

Routes use explicit HTTP methods and controller actions:

~~~gnr
Route::get('/users', UserController::index);

Route::get(
    '/users/{user}',
    UserController::show
)
    .middleware(AuthMiddleware)
    .name('users.show');
~~~

The routing contract includes route parameters, named routes, middleware, constraints, groups, resources, model binding, and scoped bindings.

See [docs/routing.md](docs/routing.md).

---

## ORM

Gungnir's ORM is model-centric:

~~~gnr
const users = User::where('active', true)
    .with('profile')
    .orderBy('name')
    .paginate(25);
~~~

A query and a materialized collection are semantically different:

~~~text
Query<User>
    ↓ get()
Collection<User>
~~~

The ORM contract covers querying, aggregates, pagination, CRUD, soft deletes, eager loading, relationships, many-to-many operations, transactions, locks, serialization, model hydration, and lifecycle behavior.

See:

- [ORM](docs/orm.md)
- [Collections](docs/collection.md)
- [Relationships](docs/relationships.md)

---

## Request, Response, and Validation

### Request

~~~gnr
const page = request.integer('page');
const token = request.bearerToken();
~~~

See [docs/request.md](docs/request.md).

### Response

~~~gnr
return json(user);

return view('users/show', {
    'user': user
});

return response(null, 204);
~~~

See [docs/response.md](docs/response.md).

### Validation

~~~gnr
const data = request.validate({
    'name': 'required|string',
    'email': 'required|email'
});
~~~

Validation rules are intended to be normalized into structured compiler/runtime metadata rather than reparsed during lowering.

See [docs/validation.md](docs/validation.md).

---

## Views

Server-rendered views use HTML templates under the configured view root.

~~~html
<h1>{{ title }}</h1>

{{#each users}}
    <p>{{ name }}</p>
{{/each}}
~~~

Double braces escape output by default:

~~~html
{{ value }}
~~~

Raw output is explicit:

~~~html
{{{ trustedHtml }}}
~~~

See [docs/view.md](docs/view.md).

---

## Authentication and Authorization

Authentication resolves request identity:

~~~gnr
Auth::check();
Auth::user();
Auth::attempt(credentials);
Auth::logout();
~~~

Authorization is policy-driven:

~~~gnr
authorize('update', post);
~~~

See:

- [Authentication](docs/authentication.md)
- [Policies](docs/policy.md)
- [Sessions](docs/session.md)
- [Security](docs/security.md)

---

## Database Backends

The database runtime is separated from ORM/application language semantics.

Backend adapters include or target:

- SQLite
- PostgreSQL
- MySQL / MariaDB-compatible clients
- SQL Server
- MongoDB

Backend capability differences remain explicit. MongoDB, for example, remains document-native rather than pretending to provide relational semantics.

See:

- [Database Runtime](docs/database.md)
- [PostgreSQL](docs/postgresql.md)
- [MySQL](docs/mysql.md)
- [SQL Server](docs/sqlserver.md)
- [MongoDB](docs/mongodb.md)

---

## Runtime

Gungnir includes runtime foundations for:

- HTTP serving;
- async execution;
- request cancellation;
- dependency injection;
- sessions;
- cache;
- queues;
- scheduling;
- storage;
- logging;
- tracing and metrics;
- application lifecycle;
- graceful shutdown;
- health and readiness.

See:

- [Application Lifecycle](docs/application-lifecycle.md)
- [Async Runtime](docs/async-runtime.md)
- [Dependency Injection](docs/dependency-injection.md)
- [HTTP Runtime](docs/http-runtime.md)
- [Cache](docs/cache.md)
- [Queues and Jobs](docs/queues.md)
- [Scheduler](docs/scheduler.md)
- [Storage](docs/storage.md)
- [Logging and Observability](docs/logging-observability.md)
- [Errors](docs/errors.md)
- [Production and Deployment](docs/production.md)

---

## Modules

Gungnir modules are static source-language modules.

File:

~~~text
app/models/user.gnr
~~~

maps conventionally to:

~~~text
app.models.user
~~~

Imports are explicit:

~~~gnr
import app.models.user;
import app.services.billing as Billing;
~~~

Modules are not C++ headers, C++20 modules, or runtime package loaders.

See [docs/modules.md](docs/modules.md).

---

## Generated C++

Generated C++23 is a build artifact, but it is intentionally inspectable.

The target transpiler architecture is:

~~~text
ValidatedProject
    ↓
Framework Lowering
    ↓
C++ IR
    ↓
C++ Emitter
~~~

Generated code may use C++23 coroutines, RAII, templates, optional/native containers, generated namespaces, and framework runtime types.

Those are not application-facing Gungnir syntax.

See [docs/transpiler.md](docs/transpiler.md).

---

## CLI

The CLI is project-aware and should generate only canonical Gungnir source.

Core workflow:

~~~text
gungnir new <name>
gungnir build
gungnir run
gungnir dev
~~~

Compiler tooling should expose checking, formatting, AST/semantic inspection, Validated AST inspection, and generated-C++ inspection as implementation matures.

Generators should only be enabled for constructs supported by the language and compiler.

See [docs/cli-codegen.md](docs/cli-codegen.md).

---

## Testing

Gungnir testing should exercise the real framework path.

The strategy includes:

- HTTP/router tests;
- dependency overrides;
- database isolation;
- queue, mail, and storage test adapters;
- lexer/parser tests;
- Syntax AST tests;
- semantic tests;
- Validated AST tests;
- generated C++ compile tests;
- backend integration tests.

See [docs/testing.md](docs/testing.md).

---

## Build

Gungnir requires:

- C++23
- CMake 3.25 or newer

Typical native build:

~~~sh
cmake -S . -B build
cmake --build build
~~~

Optional adapters and repository build flags depend on selected runtime features.

Refer to adapter documentation and CMake configuration for currently available options.

---

## Documentation

Documentation is organized into three layers.

### Language and Compiler

- [Language Types](docs/language-types.md)
- [Expressions](docs/expressions.md)
- [Statements](docs/statements.md)
- [Functions](docs/functions.md)
- [Async](docs/async.md)
- [Modules](docs/modules.md)
- [Grammar](docs/grammar.md)
- [Syntax AST](docs/ast.md)
- [Semantics](docs/semantics.md)
- [Validated AST](docs/validated-ast.md)
- [Transpiler](docs/transpiler.md)

### Framework

- [Models](docs/model.md)
- [ORM](docs/orm.md)
- [Collections](docs/collection.md)
- [Migrations](docs/migration.md)
- [Controllers](docs/controller.md)
- [Routing](docs/routing.md)
- [Middleware](docs/middleware.md)
- [Request](docs/request.md)
- [Response](docs/response.md)
- [Validation](docs/validation.md)
- [Authentication](docs/authentication.md)
- [Policies](docs/policy.md)
- [Events](docs/event.md)
- [Listeners](docs/listener.md)
- [Notifications](docs/notification.md)
- [Mail](docs/mail.md)
- [Views](docs/view.md)

### Runtime and Infrastructure

- [Application Lifecycle](docs/application-lifecycle.md)
- [Async Runtime](docs/async-runtime.md)
- [Dependency Injection](docs/dependency-injection.md)
- [Database](docs/database.md)
- [HTTP Runtime](docs/http-runtime.md)
- [Sessions](docs/session.md)
- [Cache](docs/cache.md)
- [Queues](docs/queues.md)
- [Scheduler](docs/scheduler.md)
- [Storage](docs/storage.md)
- [Logging and Observability](docs/logging-observability.md)
- [Security](docs/security.md)
- [Errors](docs/errors.md)
- [Extensions](docs/extensions.md)
- [Production](docs/production.md)
- [Testing](docs/testing.md)
- [Stability](docs/stability.md)

---

## Project Direction

Gungnir is moving away from a compatibility/source-edit transpiler toward a compiler with explicit frontend and semantic phases.

Current priority:

1. canonical language contracts;
2. complete lexer/parser coverage;
3. structural Syntax AST;
4. module and symbol resolution;
5. semantic and type analysis;
6. Validated AST;
7. structural framework lowering;
8. C++23 IR and emitter;
9. removal of remaining source-rewrite compatibility passes;
10. full compiler/runtime conformance tests.

The objective is not merely to generate C++ that compiles.

The objective is to make Gungnir a coherent, predictable application framework where the compiler understands the application before generating native code.

---

## Stability

Gungnir is currently pre-1.0.

Pin the exact version or commit used by an application until a stable compatibility policy is declared.

See [docs/stability.md](docs/stability.md).
