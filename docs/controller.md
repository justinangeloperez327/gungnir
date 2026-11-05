# Controllers

Controllers organize HTTP request handling into typed application actions.

## Defining a controller

```gnr
import app.models.User;

controller UserController {
    index() {
        return json(User::all());
    }
}
```

Controller actions use `Response` as their logical result, so the explicit result type can be omitted when the controller contract makes it unambiguous.

## Requests

Actions can receive the current request:

```gnr
controller UserController {
    store(Request request) {
        const data = request.validate({
            "name": "required|string",
            "email": "required|email"
        });

        return json(User::create(data), 201);
    }
}
```

## Dependency injection

Services can be injected into controllers and resolved from the application container.

```gnr
controller ReportController {
    inject ReportService reports;

    index() {
        return json(reports.summary());
    }
}
```

## Async actions

```gnr
controller ReportController {
    async index() {
        const report = await reports.generate();
        return json(report);
    }
}
```

Async actions use the same logical `Response` contract while executing on Gungnir's coroutine runtime.

## Route parameters and model binding

Import the model's project-relative module in the controller file. For example,
`app/models/Project.gnr` is imported with `import app.models.Project;`.
Gungnir uses `import`, and the module path is case-sensitive. An alias refers to
the module: `import app.models.Project as Models;` exposes `Models::Project`.

Route segments are matched to action parameter names. Model parameters use the
model's primary key, connection, and soft-delete rules:

```gnr
// app/controllers/ProjectController.gnr
import app.models.Project;

controller ProjectController {
    show(Project project) {
        return json(project);
    }

    update(Project project, Request request) {
        request.validate({ "name": "required|string" });
        project.name = request.input("name");
        project.save();
        return json(project);
    }

    destroy(Project project) {
        project.remove();
        return noContent();
    }
}
```

```gnr
// routes/web.gnr
import app.controllers.ProjectController;

Route::get("/projects/{project}", ProjectController::show);
Route::patch("/projects/{project}", ProjectController::update);
Route::delete("/projects/{project}", ProjectController::destroy);
```

The `{project}` segment matches the `project` parameter. The bound model is
required: missing records, invalid keys, and soft-deleted records return 404
before the action runs. No null check or second `findOrFail()` is needed.

Public `Response` actions own their model parameters by value and may change,
save, or remove those models directly. Other parameters remain immutable;
`const` copies and arrow captures also remain immutable. Use `remove()` for
model deletion.

## Responses

Controllers can return text, JSON, views, HTML, redirects, downloads, streams, or no-content responses through the response APIs.

See [Responses](response.md).
