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
