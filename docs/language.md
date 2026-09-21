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
