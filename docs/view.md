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

The data argument must be an object. `view(name)` uses empty data;
`view(name, data, status)` sets an explicit HTTP status. Dynamic Json data is
checked when rendered. Lists and scalars cannot be the root data object.

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

```html
{{#if users}}Users available{{else}}No users{{/if}}
{{#unless disabled}}Enabled{{/unless}}
```

## Loops

```html
<ul>
{{#each users}}
    <li>{{ loop.index }}: {{ name }}</li>
{{else}}
    <li>No users</li>
{{/each}}
</ul>
```

Loop metadata includes index, first/last state, and count.
Use `loop.index`, `loop.first`, `loop.last`, and `loop.count`; indexes start at zero.
`this` refers to the current value.

## Partials

Reusable fragments can be included as partials and supplied named values.

```html
{{> 'partials/user' user=this }}
```

In `views/partials/user.html`, use `{{ user.name }}`. Named values are evaluated
in the caller's current context.

## Layouts and sections

Layouts define shared page structure. Child views can provide named sections consumed by layout yields.

For `views/layouts/app.html`:

```html
<html><title>{{#yield 'title'}}Application{{/yield}}</title>
<main>{{#yield 'content'}}Empty{{/yield}}</main></html>
```

A child template can replace either section:

```html
{{#layout 'layouts/app'}}
{{#section 'title'}}Users{{/section}}
{{#section 'content'}}<h1>{{ title }}</h1>{{/section}}
{{/layout}}
```

## Components and slots

Components receive named properties and rendered slot content, allowing reusable application UI without embedding arbitrary C++ in templates.

```html
{{#component 'components/panel' kind='info'}}{{ title }}{{/component}}
```

For `views/components/panel.html`, the slot contains the already rendered child:

```html
<aside class="{{ kind }}">{{{ slot }}}</aside>
```

## Helpers

Applications can register view helpers and call them from template expressions. Helper output remains escaped unless raw rendering is explicitly requested.

Register helpers in `bootstrap/app.hpp`:

```cpp
app.views().helper("shout", [](const std::vector<gungnir::view::Value>& values) {
    return gungnir::view::Value{values.empty() ? "" : values.front().string() + "!"};
});
```

Call the helper with `{{ shout(title) }}`.

## Model serialization

Models passed to views respect their public serialization contract, including `hidden` and `visible` metadata.
These rules also apply to models inside lists, partials, and components. Hidden
password fields are unavailable to templates. Signed and unsigned 64-bit
integers retain their range; decimal rendering is independent of process locale.

## Template safety

View names are resolved beneath the configured view root. Absolute paths, traversal, and root escapes are rejected. Rendering is bounded to protect against runaway nesting or output.
