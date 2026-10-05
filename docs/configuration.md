# Configuration

`Application::create()` loads the application's environment and `.env` file.
Standard application, server, and database settings map to the existing
configuration repository. Deployment environment values take precedence over
the file. Keep deployment secrets outside committed source files.

Set typed application values in `bootstrap::configure` before requests or workers:

```cpp
app.config().set("app.name", "Project portal")
    .set("projects.page_size", gungnir::Int64{25})
    .set("projects.enabled", true)
    .set("projects.ratio", 0.5);
```

Read settings through injected Config:

```gnr
controller ConfigurationController {
    inject Config config;
    index() {
        return json({"name": config.string("app.name", "Application"),
            "pageSize": config.integer("projects.page_size", 25),
            "enabled": config.boolean("projects.enabled", false),
            "ratio": config.number("projects.ratio", 1.0)});
    }
}
```

| Method | Result | Missing-key default |
| --- | --- | --- |
| `has(key)` | `bool` | `false` |
| `get(key)` | `Json?` | Absent optional |
| `string(key, fallback)` | `string` | Empty string |
| `integer(key, fallback)` | `int` | `0` |
| `boolean(key, fallback)` | `bool` | `false` |
| `number(key, fallback)` | `double` | `0` |

Keys are strings; defaults match the result type. Typed reads use native
repository conversions and reject incompatible stored values. Integer conversion
rejects nonfinite and out-of-range numbers; numeric reads require finite values.
Defaults apply to absent keys; `string` also uses its default for stored null.
`get` preserves the stored scalar type.

Use `has` to distinguish stored null from an absent setting:

```gnr
function bool hasOptionalSetting(Config config) {
    return config.has("optional.setting");
}
```

Config retains its repository owner and provides read-only application access.
Configure shared repositories before concurrent work; use a scoped repository
for request-specific values. Serialize only selected public settings.

See [Dependency Injection](dependency-injection.md) and [Production](production.md).
