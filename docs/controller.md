# Controllers

Controllers organize HTTP request handling into typed application actions.

## Defining a controller

```gnr
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

Route parameters are matched to typed action parameters. Model parameters can use route model binding:

```gnr
controller ProjectController {
    show(Project project) {
        return json(project);
    }
}
```

## Responses

Controllers can return text, JSON, views, HTML, redirects, downloads, streams, or no-content responses through the response APIs.

See [Responses](response.md).
