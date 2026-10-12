# Views and Spear

Spear is Gungnir's server-rendered template language. Templates use ordinary
HTML, escaped expressions and Blade-like directives. Controllers continue to
return views through `view()`.

## Render a view

```gnr
return view('users/index', {title: 'Users', users: users});
```

The data argument must be an object. `view(name)` supplies empty data, and
`view(name, data, status)` sets the HTTP status. Dynamic Json data is checked at
render time. The response content type is `text/html; charset=utf-8`.

The default root is `views/`; `VIEW_PATH` or `app.view_root(...)` selects another
root. Logical names use slash-separated paths. `users/index` first resolves to
`views/users/index.spear`, then falls back to `views/users/index.html`. An
explicit extension selects that exact file. New applications contain
`views/welcome.spear`.

## Escaped and raw output

```html
<h1>{{ title }}</h1>
{!! trustedHtml !!}
```

Double braces escape `&`, `<`, `>`, double quotes and single quotes. Raw output
is explicit and should receive trusted HTML. HTML escaping does not validate
URLs or encode JavaScript/CSS; supply appropriate values for those contexts.
Inline section values and yield defaults are also escaped.

Models and model lists honor `hidden` and `visible` during conversion to view
data. Passing a model to a template does not expose its hidden password field.
Signed and unsigned 64-bit values retain their range. Number formatting is
independent of the process locale.

## Expressions

```html
{{ user.name }}
{{ users[0].name }}
{{ labels['primary'] }}
{{ missing ?? 'Default' }}
{{ enabled ? 'On' : 'Off' }}
{{ upper(title) }}
```

Expressions support string, integer, floating-point, boolean and null literals;
arrays and objects; property/index access; registered helper calls; unary `!`
and `-`; `==`, `!=`, `<`, `<=`, `>` and `>=`; `&&`, `||`, `??` and conditional
`? :`. Parentheses control grouping. Conditions, coalescing and ternaries short
circuit. Arithmetic beyond unary negation belongs in controllers or helpers.

Missing variables, properties and indexes evaluate to null and interpolate as
empty text. Null, false, numeric zero, empty strings, arrays and objects are
false; other values are true. Equality does not coerce strings or booleans to
numbers. Numeric equality compares numeric values, including mixed signed and
unsigned integers without loss. Arrays and objects compare by contents.
`===` and `!==` are aliases for the same equality rules.

An optional `$` prefix is accepted for variables, and `user->name` is an alias
for `user.name`. These conveniences do not enable PHP or arbitrary native
calls. Only helpers registered on this application's view engine are callable.

## Conditions

```html
@if(users && enabled)
    <p>Users available</p>
@elseif(loading)
    <p>Loading</p>
@else
    <p>No users</p>
@endif

@unless(disabled)
    <button>Continue</button>
@endunless
```

Every branch is parsed and checked for syntax even when its condition is false.

## Loops and empty states

```html
<ul>
@forelse(users as user)
    <li>{{ loop.iteration }}: {{ user.name }}</li>
@empty
    <li>No users</li>
@endforelse
</ul>

@foreach(users as index => user)
    <p>{{ index }}: {{ user.name }}</p>
@endforeach
```

Loops accept arrays, objects or null. Object keys iterate in lexical order.
Loop variables are scoped to each iteration; they do not overwrite the caller's
values. Loop variable names cannot be `loop`, `this`, `true`, `false` or `null`.
`this` refers to the current item.

| Loop field | Meaning |
| --- | --- |
| `loop.index` | Zero-based index |
| `loop.iteration` | One-based position |
| `loop.remaining` | Items after this one |
| `loop.count` | Number of items |
| `loop.first`, `loop.last` | First/last iteration |
| `loop.depth` | One-based loop nesting depth |
| `loop.parent` | Outer loop metadata, or null |

Nested loops restore outer metadata after completion.

## Includes

```html
@include('partials/user', {user: user, label: 'Member'})
```

In `views/partials/user.spear`:

```html
<p>{{ label }}: {{ user.name }}</p>
```

Includes inherit the caller's data. The optional second argument must be an
object; its entries override values only within that include. Include output
is already rendered and is not escaped again. Included templates and their
layout chains cannot change the caller's sections.

## Layouts and sections

For `views/layouts/app.spear`:

```html
<html>
<head><title>@yield('title', 'Application')</title></head>
<body><main>@yield('content')</main></body>
</html>
```

For `views/users/index.spear`:

```html
@extends('layouts/app')
@section('title', title)
@section('content')
    <h1>{{ title }}</h1>
    @forelse(users as user)
        @include('partials/user', {label: 'Member'})
    @empty
        <p>No users</p>
    @endforelse
@endsection
```

An extended template places its output in sections and extends one parent.
`@extends` and `@section` belong at template scope. Sections support a block
body or an inline value. A parent may itself extend another layout; child
sections override parent defaults. Duplicate section definitions within one
template are errors. Missing yields render their optional escaped default or
empty text. Layout/include cycles fail with a template error.

## Comments and literal template syntax

```html
{{-- This comment is omitted. --}}
@@if
@{{ clientExpression }}
@verbatim
    <p>{{ clientExpression }}</p>
@endverbatim
```

`@@` emits a literal `@`. `@{{ ... }}` emits literal double-brace text.
`@verbatim` preserves its body without interpreting expressions or directives.
Ordinary email addresses and CSS at-rules remain text. Unknown directive calls
such as `@incldue(...)` report an error.

## Helpers

Register presentation helpers in `bootstrap/app.hpp`:

```cpp
app.views().helper("shout", [](const std::vector<gungnir::view::Value>& values) {
    return gungnir::view::Value{values.empty() ? "" : values.front().string() + "!"};
});
```

Helper results remain escaped in `{{ shout(title) }}`. Application services,
queries and authorization decisions belong in the controller or middleware;
pass their presentation data explicitly to the template.

## Reload and cache

Parsed templates are immutable and cached per application; request data, loop
variables, section values and rendered HTML are local to each render.

Development/testing uses content-based reload. Changed templates, includes and
layouts are reparsed on their next use, including edits with unchanged size or
timestamp. `gungnir dev` does not rebuild or restart for template-only edits.
Source, bootstrap and configuration changes retain the normal rebuild workflow.

`APP_ENV=production` selects an immutable cache. Deploy templates with the
application and restart processes after changes. Native configuration can
explicitly select a policy or clear the cache:

```cpp
app.views().cache_mode(gungnir::view::CacheMode::reload);
app.views().cache_mode(gungnir::view::CacheMode::immutable);
app.views().clear_cache();
```

Set an explicit policy after loading environment/view-root configuration.
Changing the root or cache mode clears the cache. The cache holds at most 256
files and 32 MiB of estimated source/AST storage. Larger parsed templates are
rendered without retention; eviction may cause a subsequent parse.

## Diagnostics and limits

Spear errors identify the template, one-based line and byte column. Syntax is
validated before a parsed template enters the cache. A failed development parse
reports the error until the file is corrected; it does not silently serve an
older template. `view::SyntaxError` exposes `template_name()`, `line()` and
`column()` to native tooling. Runtime helper/type/composition errors also carry
a source location.

Absolute paths, traversal, embedded nulls and symlink escapes are rejected,
including on cache hits. Sources are limited to 2 MiB, parsed syntax to 100,000
nodes, syntax/composition nesting to 64, rendered fragment work to 8 MiB and
render operations to one million. These limits apply across both grammars and
composed templates. A render snapshots its root and shares one bounded session
across includes/layouts.

## Existing HTML templates

Existing `.html` templates retain the legacy brace grammar and native
`render_text()` behavior:

```html
{{ title }}
{{{ trustedHtml }}}
{{#if enabled}}Yes{{else}}No{{/if}}
{{#unless disabled}}Enabled{{/unless}}
{{#each users}}{{ loop.index }}: {{ name }}{{else}}No users{{/each}}
{{> 'partials/user.html' user=this }}
```

Legacy layouts, sections, yields, components, slots and registered helpers
remain available. Use explicit `.html` names during gradual migration when a
`.spear` file has the same logical name. Spear can include existing `.html`
partials, and legacy composition can select `.spear` templates. Rename a file
only after converting its control-flow and raw-output syntax.

Native inline Spear rendering is explicit:

```cpp
auto html = app.views().render_spear_text("@if(enabled){{ title }}@endif",
    {{"enabled", true}, {"title", "Spear"}});
```

A runnable [Spear application example](../examples/spear/README.md) is installed
under `share/gungnir/examples/spear`. Installed application acceptance verifies
HTTP responses, model hiding, exact numbers, concurrency, diagnostics,
compatibility, development reload and production restart behavior.
