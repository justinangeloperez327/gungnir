# Routing

> **Status: experimental, pre-1.0.** This page describes the current implementation. Native C++ APIs and `.gnr` syntax are identified separately. Proposed contracts are in the [design specification](design/routing.md).

## Current behavior

The `.gnr` lowerer recognizes controller action references and generates native handler adapters. The native router provides HTTP method registrations, middleware, named routes, parameter constraints, URL generation, fallback handlers and prefixed groups.

Native `RouteRegistration` methods include `middleware`, `name`, `where`, `where_number`, and `where_uuid`. Native groups are obtained from `Router::group(prefix)`; this is separate from the proposed fluent group language.

## Example

```gnr
Route::get("/", HomeController::index);
```

## Limits and planned work

Use the current compiler-supported route shape before adding fluent modifiers. Do not assume support for every `Route::prefix(...).group(() => {...})`, arrow handler, resource route or implicit model binding in the design contract. Native names and `.gnr` lowering aliases are not interchangeable.

## Implementation references

- [include/gungnir/routing/route.hpp](../include/gungnir/routing/route.hpp)
- [include/gungnir/routing/router.hpp](../include/gungnir/routing/router.hpp)
- [src/language/controller_lowering.cpp](../src/language/controller_lowering.cpp)

See the [documentation index](README.md), [getting started](getting-started.md), and [target design](design/routing.md).
