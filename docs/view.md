# Views

> **Status: experimental, pre-1.0.** This page describes the current implementation. Native C++ APIs and `.gnr` syntax are identified separately. Proposed contracts are in the [design specification](design/view.md).

## Current behavior

The default root is `views/`; names without an extension resolve to `.html`. Rendering supports escaped `{{ value }}`, explicit raw `{{{ value }}}`, dotted lookup, `this`, nested `each`, `if`, `unless` and `else` blocks. Rendered values are never reparsed as template source. Native model values respect `hidden` and `visible` serialization metadata.

Logical names are resolved under the configured root. Absolute paths, traversal and root escapes are rejected. Model/range values can be converted to structured view values.

## Example

```html
<h1>{{ title }}</h1>
<ul>
{{#each users}}
    <li>{{ name }}</li>
{{/each}}
</ul>
```

## Limits and planned work

Partials use `{{> 'partial' key=value}}`. Layouts use `#layout` with named `#section` blocks, and layout templates consume sections with `#yield` (optional fallback body). `#component` passes named props and rendered `slot` content. Each loops expose `loop.index` (zero-based), `first`, `last`, and `count`. Register native helpers with `Engine::helper`; helper calls such as `{{ upper(name) }}` remain HTML-escaped. Raw slots use the explicit triple-brace form. Expressions support lookup, literals and registered helpers, rather than arbitrary C++. Rendering is bounded by nesting and output limits. External ModelLike classes that only expose `attributes()` must select their own public fields.

## Implementation references

- [include/gungnir/view/engine.hpp](../include/gungnir/view/engine.hpp)
- [include/gungnir/view/value.hpp](../include/gungnir/view/value.hpp)
- [src/view/engine.cpp](../src/view/engine.cpp)
- [src/language/view_lowering.cpp](../src/language/view_lowering.cpp)

See the [documentation index](README.md), [getting started](getting-started.md), and [target design](design/view.md).
