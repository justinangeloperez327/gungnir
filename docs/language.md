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
