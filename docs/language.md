# Gungnir Language Frontend

Gungnir application code is allowed to be more expressive than native C++.
C++ remains the compilation target and runtime implementation language.

The language frontend is intentionally separate from the runtime so syntax
sugar has no runtime cost.

## Source files

Gungnir source files use the `.gnr` extension.

Compile a source file with:

```bash
gungnirc app/controllers/user_controller.gnr -o .gungnir/user_controller.cpp
```

Use `--check` to validate the Gungnir syntax without writing generated C++.

## Inferred bindings

Immutable bindings:

```gungnir
const users = User::all();
```

lower to:

```cpp
const auto users = User::all();
```

Mutable first assignment:

```gungnir
users = User::all();
users = User::where("active", true).get();
```

lowers to:

```cpp
auto users = User::all();
users = User::where("active", true).get();
```

The parser tracks lexical scopes. A visible binding is reassigned instead of
being redeclared.

## Framework classes

Gungnir hides CRTP and framework namespace mechanics.

```gungnir
class User : Model
{
}
```

lowers to:

```cpp
class User : public gungnir::Model<User>
{
}
```

The same convention currently applies to `Controller` and `Migration`.

## Diagnostics

Generated code starts with a C++ `#line` directive by default. Compiler
diagnostics therefore point back to the original `.gnr` file instead of the
generated cache file.

## Architecture

The frontend pipeline is:

```text
.gnr source
    -> Lexer
    -> Parser / lexical scope analysis
    -> Gungnir AST
    -> lowering
    -> generated C++
    -> Clang / GCC / MSVC
    -> native binary
```

This is not a macro preprocessor. Language-specific constructs are parsed
outside strings and comments and lowered through explicit AST nodes.

## Design rules

- Prefer expressive application syntax over exposing C++ mechanics.
- Keep generated code ordinary, inspectable C++.
- Keep the compiler frontend out of the runtime hot path.
- Preserve native C++ as an interoperability target.
- Add syntax only when its semantics can be made predictable.
- Do not hide meaningful asynchronous suspension or ownership behavior until
  the language frontend can model them safely.

Future language work should build on this frontend for framework-aware model
metadata generation, dependency injection lowering, async/await lowering,
route declarations, migration syntax, richer diagnostics, formatting, and
language-server support.


## Model language

Gungnir models expose ORM behavior automatically. Application source does not
need to spell CRTP, Field templates, PrimaryKey templates, Fillable metadata,
or generated attribute tuples.

```gungnir
class User : Model
{
    string name;
    string email;
    string? nickname;
    bool active = true;

    posts()
    {
        return hasMany<Post>();
    }
}
```

The frontend generates the native C++ model plumbing, including:

- `gungnir::Model<User>` inheritance
- conventional `users` table name
- incrementing integer `id` primary key when no primary key is declared
- `Field<T>` wrappers and nullable `std::optional<T>` fields
- fillable metadata for application fields
- `created_at` and `updated_at` model attributes by default, maintained automatically on save
- relationship state and generated model metadata

Supported scalar field keywords are `string`, `int`/`integer`, `int64`,
`uint64`, `bool`/`boolean`, `float`, and `double`.

Configuration is convention-first:

```gungnir
class AuditUser : Model
{
    table = "legacy_users";
    connection = "reporting";
    timestamps = false;
    softDeletes = true;

    string name;
}
```

`softDeletes = true` generates the soft-delete model marker and
`deleted_at` attribute plumbing. Framework-managed timestamp and soft-delete
columns are not mass assignable.

### Relationships

Relationship methods use expressive model syntax while the frontend generates
the backing relation state required by the ORM eager loader.

```gungnir
posts()
{
    return hasMany<Post>();
}

profile()
{
    return hasOne<Profile>();
}

user()
{
    return belongsTo<User>();
}

roles()
{
    return belongsToMany<Role>();
}
```

Conventional foreign keys, pivot names, and local keys are inferred. Explicit
string arguments override the inferred values. `hasOneThrough` and
`hasManyThrough` use the same lowering path and support explicit key
arguments.

### Eloquent-style method names

Gungnir source accepts familiar expressive names and lowers them to the native
runtime API. Examples include:

```gungnir
const user = User::findOrFail(id);

const users = User::whereIn("id", ids)
    .orderBy("name")
    .withTrashed()
    .get();

user.delete();
```

The generated C++ uses `find_or_fail`, `where_in`, `order_by`,
`with_deleted`, and `remove`. The aliases exist only in the language
frontend; the runtime remains ordinary C++.


## Controllers and IoC

Gungnir controllers are public application-facing classes by convention. The
language frontend inserts the native C++ access and construction plumbing.

```gungnir
class UserController : Controller
{
    Response index()
    {
        const users = User::all();

        return response("users");
    }
}
```

The generated class derives from `gungnir::Controller` and exposes its
actions publicly without requiring C++ access-specifier boilerplate.

### Injection

A controller can declare a dependency with `inject`:

```gungnir
inject Logger logger;
```

The frontend generates a container-aware constructor and retains ownership of
the resolved dependency. Application code can continue to use normal dot
syntax; Gungnir lowers the access to the generated native representation.

Controllers themselves do not need to be registered when they can be
constructed automatically. The container now supports construction with
`Container&`, allowing generated controller constructors to resolve their
declared dependencies.

### Controller routes

Routes can point directly to controller actions:

```gungnir
Route::get("/users", UserController::index);
Route::get("/users/{id}", UserController::show);
Route::post("/users", UserController::store);
Route::delete("/users/{id}", UserController::destroy);
```

The frontend generates typed native member-function pointers and controller
resolution through the application container. `delete` is lowered to the
native runtime's non-keyword route operation.

Controller actions may currently accept no arguments or `Request&`, and may
return `Response` or `Task<Response>`.

### Route parameters

Parameterized paths are matched by segment:

```gungnir
Route::get("/users/{id}", UserController::show);
```

The matched request exposes the value through:

```gungnir
request.parameter("id");
```

Route parameters are cleared and repopulated for every dispatch, so request
state does not leak between route matches.


## Async and await

Gungnir exposes language-level `async` and `await` while keeping C++
coroutine mechanics in generated code.

```gungnir
class UserController : Controller
{
    async Response index()
    {
        const result = await fetchResponse();

        return result;
    }
}
```

The frontend lowers this to a native coroutine using
`gungnir::Task<Response>`, `co_await`, and `co_return`. Application code
does not need to name the coroutine task type or C++ coroutine keywords.

`await` is only valid inside an `async` function. The frontend reports a
Gungnir diagnostic before C++ compilation when it appears elsewhere.

Async is semantic, not cosmetic. The frontend does not automatically wrap
synchronous ORM/database operations in tasks. A synchronous operation remains
synchronous until the database/ORM runtime provides a genuinely asynchronous
implementation.

### Cleaner request parameters

Controller source can use:

```gungnir
Response show(Request request)
{
    return text(request.parameter("id"));
}
```

or:

```gungnir
async Response show(Request request)
{
    return text(request.parameter("id"));
}
```

The frontend generates the native `gungnir::Request&` parameter and qualifies
framework response types. The reference marker is runtime plumbing and is not
part of normal Gungnir application syntax.

### Generated-source verification

The test suite includes a real `.gnr` async controller fixture. CI runs
`gungnirc`, compiles the generated C++, links it against the Gungnir runtime,
and executes the resulting test binary. This verifies the full path from
Gungnir source through transpilation to native controller dispatch.


## Views and response rendering

Gungnir controllers can return HTML views directly:

```gungnir
Response index()
{
    const users = User::all();

    return view("users/index", {
        "users": users,
        "title": "Users"
    });
}
```

The object-style view data syntax is lowered to the native typed view-data
container. Models and ORM collections are accepted directly; application code
does not need `toView()`, `toMap()`, or another conversion layer.

The runtime also exposes:

```gungnir
Response::view("users/index", {
    "users": users
});
```

Views default to the application's `views` directory. The root can be
configured through the application runtime.

### Template syntax

View files are ordinary HTML files with lightweight expressions:

```html
<h1>{{ title }}</h1>

<ul>
{{#each users}}
    <li>{{ name }}</li>
{{/each}}
</ul>
```

`{{ value }}` is HTML-escaped by default. Raw output is explicit with
`{{{ value }}}`. Dot paths are supported for nested objects.

Models are exposed to views through their generated attribute metadata and
collections become arrays automatically. Loaded application data therefore
keeps the same shape from ORM query to controller to view.

### Safety

View names are resolved underneath the configured view root and path traversal
outside that root is rejected. Normal interpolation is escaped by default to
reduce accidental HTML injection.


## HTTP runtime

Gungnir now owns the HTTP listener behind the application object. Normal
application code does not construct or manage a server class.

```gungnir
Application app;

Route::get("/", HomeController::index);

app.listen(8000);
```

The default bind address is `127.0.0.1`. To accept external connections:

```gungnir
app.listen(8000, "0.0.0.0");
```

The current native backend provides HTTP/1.0 and HTTP/1.1 request parsing,
case-insensitive headers, request bodies through `Content-Length`, fixed
worker-thread dispatch, controller coroutine execution, response
serialization, and clean application-level stop control.

Transport mechanics remain framework plumbing. Application code only deals
with routes, requests, responses, controllers, models, and views. The server
currently closes each connection after one response; persistent connections
and event-driven socket I/O belong to the next transport/runtime layer rather
than being simulated as asynchronous behavior.


## Request input

The request object owns common HTTP input plumbing. Query strings,
URL-encoded form bodies, JSON bodies, cookies, headers, and route parameters
remain distinct internally but have a compact controller-facing API.

\`\`\`gungnir
Response store(Request request)
{
    const name = request.input("name");

    if (!request.has("email")) {
        return response("Email is required", 422);
    }

    const values = request.only(["name", "email"]);

    return response(name);
}
\`\`\`

Body input takes precedence over query input. JSON input remains typed through
\`request.json()\` when nested values are needed. \`all()\`, \`only(...)\`,
and \`except(...)\` provide flat scalar request input without exposing HTTP
parser mechanics.

JSON responses serialize scalar values, maps, ranges, models, and ORM
collections directly:

\`\`\`gungnir
return json(user);
\`\`\`

## Middleware

Middleware participates in the request lifecycle before controller dispatch.
Global middleware is registered on the application and route middleware is
attached to an individual route.

\`\`\`gungnir
class AuthMiddleware : Middleware
{
    async Response handle(Request request, Next next)
    {
        if (request.header("authorization").empty()) {
            return text("Unauthorized", 401);
        }

        return await next(request);
    }
}

Route::get("/dashboard", DashboardController::index)
    .middleware(AuthMiddleware);
\`\`\`

Middleware instances are resolved through the application container, so
constructor injection works without application code constructing middleware.
Global middleware runs before route middleware. Middleware can short-circuit
the request by returning a response without invoking \`next\`.

The continuation is asynchronous by design. Gungnir does not disguise a
possibly asynchronous downstream controller as a synchronous call, so
middleware that continues the pipeline uses \`async\` and \`await\`.


## Validation

Controllers can validate request input without constructing validator objects.

```gungnir
Response store(Request request)
{
    const data = request.validate({
        "name": "required|string|min:2|max:80",
        "email": "required|email",
        "age": "nullable|integer|min:18"
    });

    const user = User::create(data);

    return json(user, 201);
}
```

The Gungnir frontend lowers the object-style rule declaration to the typed
native validation runtime. Successful validation returns only fields declared
in the rule set.

The core rule set currently includes `required`, `present`, `sometimes`,
`nullable`, `string`, `integer`, `numeric`, `boolean`, `email`, `accepted`,
`min`, `max`, `length`, `in`, `same`, and `confirmed`.

Database-backed rules such as `unique` and `exists` are intentionally not
implemented by issuing ad-hoc SQL from the HTTP layer. They should be added
after validation can reuse a shared database query abstraction across SQL and
MongoDB backends.

## Centralized exceptions

Exceptions raised by middleware and controllers now pass through one framework
exception handler before HTTP serialization.

Default mappings are:

- validation failures -> `422`
- authentication failures -> `401`
- authorization failures -> `403`
- model-not-found failures -> `404`
- other HTTP exceptions -> their explicit status
- unexpected exceptions -> `500`

When the request accepts JSON, the handler emits a JSON error object. Internal
exception messages are not exposed for unexpected server errors.

`first_or_fail()` and `find_or_fail()` now throw `ModelNotFoundError` rather
than a generic range exception. `ModelNotFoundError` still derives from
`std::out_of_range` for native C++ compatibility.


## Configuration, environment, and bootstrap

`Application::create()` is the convention-first entry point for a Gungnir
application. It establishes the application base path, loads `.env`, seeds
framework configuration, registers configuration objects in the IoC container,
and resolves the view root before routes begin handling requests.

```gungnir
app = Application::create();

Route::get("/", HomeController::index);

app.run();
```

The language frontend qualifies `Application` automatically, so normal Gungnir
source does not need the native `gungnir::` namespace.

The default `.env` keys are:

- `APP_NAME`
- `APP_ENV`
- `APP_DEBUG`
- `APP_HOST`
- `APP_PORT`
- `VIEW_PATH`
- `DB_CONNECTION`
- `DB_HOST`
- `DB_PORT`
- `DB_DATABASE`
- `DB_USERNAME`
- `DB_PASSWORD`

Process environment variables take precedence over values loaded from `.env`.
Quoted strings, comments, `export KEY=value`, integers, and boolean values are
supported.

Configuration is accessible through the application:

```gungnir
const name = app.config().string("app.name");
const port = app.config().integer("server.port");
const debug = app.config().boolean("app.debug");
```

`app.run()` reads `server.host` and `server.port` from configuration and then
starts the existing HTTP runtime. Existing explicit code such as
`app.listen(8000, "127.0.0.1")` remains supported.

Database environment values are loaded into configuration but do not create a
database connection automatically yet. Gungnir currently exposes database
driver abstractions without bundled native client drivers; pretending otherwise
would make bootstrap appear more complete than the runtime actually is.


## Gungnir CLI

The `gungnir` command is the framework-facing tool. `gungnirc` remains the
lower-level language compiler used by the framework and advanced tooling.

Create an application:

```text
gungnir new my-app
cd my-app
gungnir run
```

A new project follows framework conventions without exposing CMake or generated
C++ as normal application code:

```text
my-app/
├── app/
│   ├── controllers/
│   ├── middleware/
│   └── models/
├── config/
├── database/
│   └── migrations/
├── routes/
│   └── web.gnr
├── views/
├── .env
├── .env.example
└── .gungnir-project
```

Generators:

```text
gungnir make:model User
gungnir make:controller UserController
gungnir make:middleware AuthMiddleware
gungnir make:migration create_users_table
```

`gungnir build` scans application model, middleware, controller, and route
sources, transpiles them through the Gungnir frontend, and assembles the hidden
native translation unit under `.gungnir/`. It then builds the application
against the installed Gungnir CMake package. `gungnir run` builds and starts
the application from the project root. Use `--release` for release builds.

The current assembler intentionally uses one generated application translation
unit. This keeps route/controller visibility predictable without exposing C++
header mechanics. A future module/import layer can split compilation for larger
applications without changing the application directory conventions.

Migration files are generated but are not linked into the web application
binary. A dedicated `gungnir migrate` path belongs to the database-driver and
migration CLI work.

Gungnir now installs CMake package metadata and exported runtime, ORM, and
language targets. Generated applications use `find_package(Gungnir)` internally
instead of repository-relative library paths.


## Database bootstrap and migration commands

Database environment configuration is now converted into a typed
`database::Settings` object during application boot. A backend adapter registers
a configured driver factory once, and the application can then create the
default pooled connection from `.env` without controllers or models knowing
about native client libraries.

Supported backend identifiers at the framework contract level are PostgreSQL,
MySQL/MariaDB, SQL Server, and MongoDB. Their default ports are 5432, 3306,
1433, and 27017 respectively.

Concrete network/client adapters are deliberately separate from this loop.
If `DB_CONNECTION` names a backend whose adapter has not been registered,
Gungnir raises `DriverUnavailableError` instead of silently using a mock or
shell command.

The application-level adapter hook is:

```cpp
app.database_driver(database::Backend::postgresql, factory);
```

`factory` receives the resolved host, port, database, username, password,
connection name, and pool size.

The CLI now assembles migration files into a separate hidden native migration
binary and exposes:

```text
gungnir migrate
gungnir migrate:rollback
gungnir migrate:reset
gungnir migrate:status
gungnir migrate:plan
```

`migrate:plan` requires only `DB_CONNECTION`; it compiles every migration plan
using the selected backend compiler and prints the statements without opening
a database connection. This makes schema/backend verification usable before
a concrete database adapter is installed.

The execution commands use the existing migration repository and transactional
runner. They require a real registered driver and fail explicitly if one is
not available. Backend-specific adapters can now be added independently without
changing the migration CLI contract.
