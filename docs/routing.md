# Routing

Routes connect HTTP requests to controllers and middleware.

Application routes are normally defined in `routes/web.gnr` and API routes can be organized separately.

## Basic routes

```gnr
Route::get("/", HomeController::index);
Route::post("/users", UserController::store);
Route::put("/users/{user}", UserController::update);
Route::patch("/users/{user}", UserController::update);
Route::delete("/users/{user}", UserController::destroy);
```

## Route parameters

Dynamic segments use braces:

```gnr
Route::get("/projects/{project}", ProjectController::show);
```

Parameters are passed to the target action using the route's typed handler contract.

## Named routes

Routes can be assigned stable names for URL generation and redirects.

```gnr
Route::get("/dashboard", DashboardController::index)
    .name("dashboard");
```

## Constraints

Route parameters can be constrained:

```gnr
Route::get("/users/{id}", UserController::show)
    .whereNumber("id");
```

UUID and explicit pattern constraints can be used for identifiers with stricter formats.

## Middleware

```gnr
Route::get("/account", AccountController::show)
    .middleware("auth");
```

Middleware aliases and groups are registered by the application.

## Route groups

Groups share common prefixes, middleware, and naming conventions:

```gnr
Route::prefix("/admin")
    .middleware("auth")
    .group(() => {
        Route::get("/users", AdminUserController::index);
        Route::get("/projects", AdminProjectController::index);
    });
```

## Model binding

Typed model parameters can resolve route identifiers into model instances. Missing bound models use the framework's not-found response path.

## Fallback routes

Applications can register a fallback handler for requests that do not match another route.

## URL generation

Named routes provide stable URL generation independent of the controller implementation.
