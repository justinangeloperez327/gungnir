# Gungnir

**Gungnir is an expressive web framework built in C++23.**

Gungnir lets you create backend applications using a structured `.gnr`
language with familiar models, controllers, routes, middleware, validation,
authentication and database access. Applications compile into native C++23
executables.

> Project status: **Stable 1.0.2**. See the release scope and known limitations in the [installation guide](docs/release-v1.0.2.md).

## What an application looks like

Define a controller in `app/controllers/HomeController.gnr`:

```gnr
controller HomeController {
    Response index() {
        return text("Hello from Gungnir");
    }
}
```

Register an endpoint in `routes/web.gnr`:

```gnr
Route::get("/", HomeController::index);
```

Gungnir also provides [models and ORM](docs/model.md),
[migrations](docs/migration.md), [authentication](docs/authentication.md),
[validation](docs/validation.md), [queues](docs/queues.md),
[notifications](docs/notification.md), [mail](docs/mail.md) and
[server-rendered views](docs/view.md).

## Installation

You need a C++23 compiler and **CMake 3.25 or newer**.

Download the Windows installer or Windows/Linux SDK from
[GitHub Releases](https://github.com/justinangeloperez327/gungnir/releases).
Version 1.0.2 provides core and application SDKs. Choose the application SDK
for bundled SQLite persistence and password hashing; other external adapters
need an appropriately built SDK.
See [SDK Packages](docs/sdk-packages.md).

For a source build:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DGUNGNIR_BUILD_TOOLS=ON -DGUNGNIR_BUILD_TESTS=OFF
cmake --build build --parallel 2
cmake --install build
```

Check the installation:

```sh
gungnir --version
gungnirc --version
```

## Create and run an application

```sh
gungnir new hello
cd hello
gungnir build
gungnir run
```

Open **http://127.0.0.1:8000**. The default port can be changed with
`APP_PORT` in `.env`.

For development with automatic rebuilding:

```sh
gungnir dev
```

Validate Gungnir source without producing a native executable:

```sh
gungnirc app/controllers/HomeController.gnr --check
gungnirc --project . --verify-ir
```

## Generate application components

```sh
gungnir make:model User
gungnir make:controller UserController
gungnir make:migration CreateUsers
gungnir make:middleware Authenticate
gungnir make:request StoreUser
```

Generators preserve names like `UserController.gnr` for declarations.
Migrations use snake_case filenames. See [CLI Commands](docs/cli-codegen.md).

## Learn Gungnir

Read [Getting Started](docs/getting-started.md) for a complete walkthrough
or open the [Documentation Index](docs/README.md) to browse the framework:

- [Routes and Controllers](docs/routing.md)
- [Models and ORM](docs/orm.md)
- [Migrations](docs/migration.md)
- [Requests, Responses and Validation](docs/request.md)
- [Views and Spear](docs/view.md)
- [Authentication and Authorization](docs/authentication.md)
- [Background Jobs and Scheduling](docs/queues.md)
- [Configuration and Deployment](docs/production.md)

## Project information

[Contributing](CONTRIBUTING.md) · [Changelog](CHANGELOG.md) ·
[Security](SECURITY.md) · [Support](SUPPORT.md) · [License](LICENSE)

Gungnir uses the stable **1.0 language and native API contract**.
Generated application code should be rebuilt with the matching SDK.
