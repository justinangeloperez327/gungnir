# Dependency Injection

Declare injected services in controllers, middleware, policies, listeners, and jobs:

```gnr
controller SettingsController {
    inject Config config;
    inject Logger logger;
    index() {
        logger.info("Settings requested");
        return json({"name": config.string("app.name")});
    }
}
```

Configure adapters and factories in the editable `bootstrap/app.hpp`.
`ServicesProvider` supplies Cache, Storage, Events, Queue, Auth, Logger, and
Telemetry. Config reads the application's configuration repository. Register
dependencies before resolving their consumers.

## Lifetimes

| Binding | Lifetime |
| --- | --- |
| `bind<T>` | A new result for each resolution |
| `singleton<T>` / `instance<T>` | Shared within the application container |
| `scoped<T>` | Shared within one `ServiceScope` |

Generated HTTP factories preserve the request scope when constructing controllers
and middleware. Their dependencies resolve in that same scope; the next request
gets a new scope. Scoped resolution outside a scope fails.

A native factory can override Config for each request:

```cpp
app.scoped<gungnir::config::Service>([](gungnir::ServiceScope&) {
    auto values = std::make_shared<gungnir::config::Repository>();
    values->set("app.name", "Tenant application");
    return gungnir::config::Service{values};
});
```

Generated job handlers create a fresh scope before reconstructing injected
dependencies from validated payloads. The scope remains alive through an awaited
handler. Injected handles retain their owners across suspension; passing an
owning Config value also preserves its repository.

Policies and listeners register during boot. Their dependencies must be
application services because no request scope exists during registration.
Native consumers can explicitly resolve through a `ServiceScope`.

Config, Logger, Telemetry, and Span are typed handles supplied by injection or
factories. They cannot be constructed as empty language values, stored in model
fields or job payloads, or serialized into JSON.

See [Configuration](configuration.md), [Logging and Observability](logging-observability.md),
and [Application Lifecycle](application-lifecycle.md).
