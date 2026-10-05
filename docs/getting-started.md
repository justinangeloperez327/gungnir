# Getting Started

Gungnir is an expressive C++23 web framework with a dedicated `.gnr` application language and a native runtime.

## Requirements

A Gungnir development environment requires a supported C++23 compiler, CMake, and the Gungnir CLI. Optional database, Redis, SMTP, TLS, HTTP/2, and storage adapters may require their native dependencies.

## Install

Install a packaged Gungnir distribution for your platform or build and install the framework from source.

For SQLite and password authentication, select the [application SDK](sdk-packages.md).
The published v1.0.0 packages contain the core SDK; the application profile is
available from current source and is included in subsequent release packaging.

After installation, verify the tools:

```sh
gungnir --version
gungnirc --version
```

## Create an application

```sh
gungnir new hello
cd hello
```

A project contains application declarations, routes, views, configuration, bootstrap code, tests, and build metadata.

## Create a controller

```gnr
controller HomeController {
    index() {
        return text("Hello from Gungnir");
    }
}
```

## Register a route

```gnr
Route::get("/", HomeController::index);
```

## Build and run

```sh
gungnir build
gungnir run
```

The development server can rebuild and restart the application while source files change:

```sh
gungnir dev
```

## Validate source

```sh
gungnirc app/controllers/HomeController.gnr --check
```

The check command performs Gungnir parsing and semantic/framework validation without producing a native executable.

## Generate application files

Use the CLI to generate framework declarations:

```sh
gungnir make:model User
gungnir make:controller UserController
gungnir make:migration CreateUsers
gungnir make:middleware Authenticate
gungnir make:policy UserPolicy app.models.User::User
gungnir make:event UserRegistered
gungnir make:listener SendWelcomeEmail app.events.UserRegistered::UserRegistered
gungnir make:notification WelcomeNotification app.models.User::User
gungnir make:mail WelcomeMail
gungnir make:job ProcessImport
```

Generated declarations follow Gungnir naming and project-layout conventions.

## Next steps

Continue with [The Gungnir Language](language.md), [Routing](routing.md), [Controllers](controller.md), [Models](model.md), [ORM](orm.md), [Migrations](migration.md), and [Testing](testing.md).
