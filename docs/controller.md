# Controllers

> **Status: Development.** This guide describes the current implementation and documented limits. The 1.0 compatibility contract is not frozen yet.

The structured profile (`gungnirc --strict`) supports typed functions and framework actions, structured callbacks, and validated C++ emission. See [compiler profiles](compiler-profiles.md) for usage and current limits. The compatibility profile retains the native syntax described below.

## Current behavior

Use a typed action in the current language frontend. A synchronous action returns `Response`; async actions use an explicit logical result type. The lowerer generates the native controller base and injected dependency plumbing.

Controller helpers include `response`, `text`, `json`, `view`, `html`, `download`, `no_content`, and `redirect`. Request arguments and route arguments must match the currently generated handler signatures.

## Example

```gnr
controller HomeController {
    Response index() {
        return text("Hello from Gungnir");
    }
}
```

## Limits and planned work

`public index()` uses the implicit Response contract in the structured profile. Compatibility mode continues to require typed actions. Route binding is a separate contract; successful parsing alone does not establish that a handler can be invoked.

## Implementation references

- [include/gungnir/controller/controller.hpp](../include/gungnir/controller/controller.hpp)
- [src/language/controller_lowering.cpp](../src/language/controller_lowering.cpp)
- [src/language/parser.cpp](../src/language/parser.cpp)

See the [documentation index](README.md), [getting started](getting-started.md), and [target design](design/controller.md).
