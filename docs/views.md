# Views

Gungnir views provide server-rendered HTML with escaped interpolation by default.

## Rendering

`Engine::render()` resolves templates beneath the configured view root. Absolute paths and parent traversal are rejected. `Response::view()` uses the application's active view engine.

## Data

`view::Data` accepts initializer-list entries and supports incremental composition with `with()` and `merge()`.

```cpp
view::Data data;
data.with("title", "Users")
    .with("users", users);
```

Models and collections are converted to view values through the existing model attribute contract.

## Escaping

`{{ value }}` HTML-escapes output. Triple braces are intentionally raw and should only be used for trusted HTML. User-controlled content must use escaped interpolation.

## Loops

`{{#each users}}...{{/each}}` iterates arrays and makes each item the current context. Nested object paths remain available.

## Errors

View failures use view-specific error types so applications can distinguish missing templates and template syntax failures from unrelated runtime failures.

## Architecture

The current engine is deliberately small. Layouts, includes, components and template compilation should be added through explicit syntax and parsing rather than by growing fragile string replacement rules indefinitely.
