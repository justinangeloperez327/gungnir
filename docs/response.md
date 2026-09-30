# Responses

> **Status: experimental, pre-1.0.** This page describes the current implementation. Native C++ APIs and `.gnr` syntax are identified separately. Proposed contracts are in the [design specification](design/response.md).

## Current behavior

`Response` contains status, body, headers, cookies and optional streaming/WebSocket state. Native factories include `text`, `json`, `view`, `html`, `redirect`, `no_content`, `not_found`, `download`, `stream` and `websocket`.

`status`, `body`, `header`, `cookie` and `without_cookie` modify a response. `download` receives body contents and a filename; it is not a filesystem-path reader. Controller helpers expose a subset of these factories.

## Example

```gnr
controller HealthController {
    Response index() {
        return text("ok").header("Cache-Control", "no-store");
    }
}
```

## Limits and planned work

Use `Response::no_content()` or the controller `no_content()` helper for 204. Do not document `noContent()` as an implemented alias. The controller `response` helper accepts a string body; `response(null, 204)` is not its documented signature.

## Implementation references

- [include/gungnir/http/response.hpp](../include/gungnir/http/response.hpp)
- [include/gungnir/controller/controller.hpp](../include/gungnir/controller/controller.hpp)

See the [documentation index](README.md), [getting started](getting-started.md), and [target design](design/response.md).
