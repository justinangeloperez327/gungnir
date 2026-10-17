# Getting Started

Gungnir is a C++23 web framework whose application logic is written in
structured `.gnr` files. The `gungnir` CLI creates, builds and runs native
applications; `gungnirc` validates the language without generating a binary.

## 1. Choose an SDK

You need a C++23-compatible compiler, **CMake 3.25+**, and Gungnir's CLI.
On Windows, install Visual Studio C++ Build Tools and use a developer
terminal that can locate the compiler. Open a new terminal after setup
to refresh `PATH`.

- **Core SDK:** Install the v1.0.1 core package from
  [GitHub Releases](https://github.com/justinangeloperez327/gungnir/releases).
  It includes HTTP, Spear views and typed delivery APIs, without bundled SQLite
  or password hashing.
- **Application SDK:** Install the v1.0.1 application package for bundled SQLite
  and password hashing. See [SDK Packages](sdk-packages.md).
- **Other adapters:** Build Gungnir with the corresponding adapter and native
  dependencies. Selecting a database in `.env` does not enable an unbuilt
  adapter.

Verify the installed commands:

```sh
gungnir --version
gungnirc --version
```

## 2. Create a project

```sh
gungnir new hello
cd hello
```

The generated project contains application declarations, routes, configuration,
editable native bootstrap, and build metadata. Current source also generates
`views/welcome.spear`; the published core release predates the Spear grammar.
See [Views and Spear](view.md) for current-source templates.

For a minimal controller, use the same shape as the
[verified Hello example](../examples/hello/README.md):

```gnr
controller HomeController {
    Response index() {
        return text("Hello from Gungnir");
    }
}
```

Register the action in `routes/web.gnr`:

```gnr
Route::get("/", HomeController::index);
```

The example README identifies which generated files to replace.

## 3. Build and run

```sh
gungnir build
gungnir run
```

With the default generated environment, visit
`http://127.0.0.1:8000/`. Set `APP_PORT` in `.env` to select another port;
**8000 is the default**, not 3000 or 7000.

For automatic rebuilds/restarts during development:

```sh
gungnir dev
```

The watcher tracks source, native bootstrap and configuration changes.
On current source, template-only edits reload on the next request in
development/testing mode instead of rebuilding the binary. A source-driven
process restart loses process-local memory state.

## 4. Check your source

```sh
gungnirc app/controllers/HomeController.gnr --check
```

`--check` performs structured-language semantic validation without native
compilation. `gungnir build` additionally compiles routes and modules,
emits native code and builds the executable. The Hello controller is compiled
by the documentation CI workflow.

## 5. Generate application components

```sh
gungnir make:model User
gungnir make:controller UserController
gungnir make:migration CreateUsers
gungnir make:middleware Authenticate
gungnir make:request StoreUser
gungnir make:policy UserPolicy app.models.User::User
gungnir make:event UserRegistered
gungnir make:listener SendWelcomeEmail app.events.UserRegistered::UserRegistered
gungnir make:notification WelcomeNotification app.models.User::User
gungnir make:mail WelcomeMail
gungnir make:job ProcessImport
```

Declaration filenames retain PascalCase, such as `UserController.gnr`;
migrations use snake_case filenames. Generators refuse to overwrite
existing files. See [CLI and Code Generation](cli-codegen.md) for
arguments and generated declarations.

## 6. Add a database or background processing

To use SQLite, install/build an SDK that includes the SQLite adapter,
create the parent directory for the database, and set in `.env`:

```dotenv
DB_CONNECTION=sqlite
DB_DATABASE=storage/app.sqlite
```

Then run migrations:

```sh
gungnir migrate
gungnir migrate:status
```

The published core SDK does not contain the SQLite backend. Consult
[SDK Packages](sdk-packages.md), [Database](database.md),
[Models](model.md), [ORM](orm.md) and [Migrations](migration.md).

When your application defines queued jobs or schedules, start the
separate processes explicitly:

```sh
gungnir queue:work
gungnir schedule:work
```

Memory stores are process-local. For independent servers and workers,
configure shared services; see [Queues](queues.md),
[Sessions](session.md) and [Production](production.md).

## Troubleshooting

| Symptom | Check |
| --- | --- |
| `gungnir` is not recognized | Install the CLI and add the SDK `bin` directory to `PATH`; reopen the terminal |
| CMake or the compiler cannot be found | Install CMake and a C++23 toolchain; use a Visual Studio developer terminal on Windows |
| SQLite/password capability error | Select an SDK with those compiled features; changing `.env` or generated CMake cannot add them |
| Can't reach the local HTTP application | Confirm the listener address and `APP_HOST`/`APP_PORT` |
| A queued job is not consumed | Run a worker and check both processes use the same configured queue |
| A production view change is not visible | Restart the process or explicitly invalidate the immutable view cache |

Next: [Language](language.md), [Routing](routing.md),
[Validation](validation.md), [Views](view.md),
[Testing](testing.md) and [Production](production.md).
