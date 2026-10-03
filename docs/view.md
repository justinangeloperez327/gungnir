# Views

Gungnir views render HTML and other text responses from application data.

The default view root is `views/`. Logical names resolve to view templates under that root.

## Rendering a view

```gnr
return view("users/index", {
    "title": "Users",
    "users": users
});
```

## Escaping

Values rendered with double braces are HTML escaped:

```html
<h1>{{ title }}</h1>
```

Triple braces explicitly render raw content:

```html
{{{ trustedHtml }}}
```

Prefer escaped output for application and user-provided data.

## Conditions

Templates support `if`, `unless`, and `else` blocks.

## Loops

```html
<ul>
{{#each users}}
    <li>{{ name }}</li>
{{/each}}
</ul>
```

Loop metadata includes index, first/last state, and count.

## Partials

Reusable fragments can be included as partials and supplied named values.

## Layouts and sections

Layouts define shared page structure. Child views can provide named sections consumed by layout yields.

## Components and slots

Components receive named properties and rendered slot content, allowing reusable application UI without embedding arbitrary C++ in templates.

## Helpers

Applications can register view helpers and call them from template expressions. Helper output remains escaped unless raw rendering is explicitly requested.

## Model serialization

Models passed to views respect their public serialization contract, including `hidden` and `visible` metadata.

## Template safety

View names are resolved beneath the configured view root. Absolute paths, traversal, and root escapes are rejected. Rendering is bounded to protect against runaway nesting or output.
