# Views

> **Status: experimental, pre-1.0.** This page describes the current implementation. Native C++ APIs and `.gnr` syntax are identified separately. Proposed contracts are in the [design specification](design/view.md).

## Current behavior

The default root is `views/`; names without an extension resolve to `.html`. Rendering supports escaped `{{ value }}`, explicit raw `{{{ value }}}`, dotted lookup, `this`, each blocks and nested each blocks.

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

Conditionals, else/unless, partials, layouts, sections, yields, components, loop metadata and safe helper calls are target syntax. The current ModelLike conversion iterates `attributes()`; do not promise hidden-field filtering. Pass an explicitly selected view object when sensitive attributes are present.

## Implementation references

- [include/gungnir/view/engine.hpp](../include/gungnir/view/engine.hpp)
- [include/gungnir/view/value.hpp](../include/gungnir/view/value.hpp)
- [src/view/engine.cpp](../src/view/engine.cpp)
- [src/language/view_lowering.cpp](../src/language/view_lowering.cpp)

See the [documentation index](README.md), [getting started](getting-started.md), and [target design](design/view.md).
