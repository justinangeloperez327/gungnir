# Routing

Routes connect HTTP requests to public controller actions. Define them at module
scope in `routes/web.gnr`; other files under `routes/` can organize API routes.
The application compiler validates these files with the rest of the project and
registers them on the application's router.

## Basic routes

```gnr
Route::get("/", HomeController::index);
Route::post("/users", UserController::store);
Route::put("/users/{user}", UserController::update);
Route::patch("/users/{user}", UserController::update);
Route::delete("/users/{user}", UserController::destroy);
Route::options("/users", UserController::options);
Route::head("/", HomeController::index);
```

Paths start with `/`. Dynamic segments occupy a whole segment, such as `{user}`;
parameter names must be unique within the path. Query strings and fragments are
not part of a route declaration. Register specific routes before broader routes
that could match the same request.

## Route parameters

Action parameters bind by name, independently of their order in the URI.
An action can also accept one `Request` at any position:

```gnr
controller ItemController {
    show(bool enabled, Request request, int id) {
        return json({ id: id, enabled: enabled, path: request.path() });
    }
}
Route::get("/items/{id}/{enabled}", ItemController::show);
```

Bound scalar types are `string`, `int`, `uint64`, `double`, `decimal` and `bool`.
Numbers must parse completely and fit the declared type; floating-point values
must be finite. Booleans accept `true`, `false`, `1` and `0`. Invalid values return
404 before the action runs. Bound parameters are required and do not have default
values. Unused URI parameters remain available through `request.parameter()`.

Dynamic segments are percent-decoded once after the URI is split into segments.
An encoded slash remains part of one parameter. A literal `+` remains a plus;
malformed escapes and control characters do not match.

## Named routes

Names are unique across the project and provide stable URL generation:

```gnr
Route::get("/dashboard", DashboardController::index).name("dashboard");
```

## Constraints

Constraints match the complete decoded parameter:

```gnr
Route::get("/users/{id}", UserController::show).whereNumber("id");
Route::get("/codes/{code}", UserController::showCode).whereUuid("code");
Route::get("/labels/{code}", UserController::showCode).where("code", "[a-z]+");
```

`whereNumber` accepts decimal digits. `whereUuid` accepts UUID versions 1 through
5. `where` uses an ECMAScript regular expression. Constraints refer to parameters
present in the URI. A failed constraint continues route matching.

## Middleware

Use a registered alias or a declared middleware type:

```gnr
Route::get("/account", AccountController::show).middleware("auth");
Route::get("/audit", AccountController::show).middleware(AuditMiddleware);
```

Register aliases in `bootstrap/app.hpp`, for example
`app.middleware_alias<AuthMiddleware>("auth")`. Generated middleware types are
resolved through the application container. The typed form
`.middleware<AuditMiddleware>()` is also available. Global middleware wraps route
middleware; group and action middleware run in declaration order, then unwind in
reverse order after the action.

## Route groups

Groups share prefixes, middleware and name prefixes. They can be nested:

```gnr
Route::prefix("/admin").name("admin.").middleware("auth").group(() => {
    Route::get("/users", AdminUserController::index).name("users");
    Route::prefix("/reports").name("reports.").group(() => {
        Route::get("/", ReportController::index).name("index");
    });
});
```

These routes are named `admin.users` and `admin.reports.index`. A child path `/`
uses the group prefix itself, so the report route is `/admin/reports`. Group
callbacks contain route declarations and take no parameters. Groups start with
`prefix`, `name` or `middleware`; `group` is the final call.

## Controller imports

A controller's unqualified name can be used when it is unique in the project.
Use an explicit module import when several modules declare the same name:

```gnr
import app.controllers.HomeController as Home;
Route::get("/", Home::HomeController::index);
```

## Model binding

A model parameter loads a row using the model's primary key:

```gnr
import app.models.Project;
controller ProjectController {
    async show(Project project) {
        return json(project);
    }
}
Route::get("/projects/{project}", ProjectController::show).name("projects.show");
```

The `{project}` segment binds to the action's `project` parameter. Binding uses
the model's configured connection and primary-key type, including custom string
keys. It performs one lookup using the ORM's bound query parameters and default
soft-delete scope. Missing rows, soft-deleted rows and invalid keys return 404
without calling the action. The bound model stays owned across an awaited
action. Configure the database in the application as described in
[database connections](database.md).

## Fallback routes

One fallback can handle requests that do not match another route:

```gnr
controller NotFoundController {
    index(Request request) { return text("Page not found", 404); }
}
Route::fallback(NotFoundController::index);
```

Fallback declarations belong at module scope. They use global middleware and do
not take route names, constraints or route middleware.

## URL generation

Use `Route::url` and `Route::has` while handling a request:

```gnr
controller NavigationController {
    show(int project) { return json({ project: project }); }
    index() {
        if (Route::has("projects.show")) {
            return redirect(Route::url("projects.show", { project: 42 }));
        }
        return text("No project route", 404);
    }
}
Route::get("/projects/{project}", NavigationController::show).name("projects.show");
Route::get("/", NavigationController::index).name("home");
```

Routes without parameters need only the name. Parameter values are strings,
numbers or booleans; they are converted to strings and percent-encoded as one URI
segment. Unknown names, missing parameters, empty values and control characters
raise a configuration error. Braces, slashes and percent signs in values are
encoded as data and cannot introduce another placeholder. `Route` is a static
API; it cannot be injected or stored as a value.
