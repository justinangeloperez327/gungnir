# Requests

> **Status: experimental, pre-1.0.** This page describes the current implementation. Native C++ APIs and `.gnr` syntax are identified separately. Proposed contracts are in the [design specification](design/request.md).

## Current behavior

The native `Request` exposes method, target, path, body, body stream and cancellation; headers, query parameters, route parameters, cookies and parsed JSON; input selection; and attached service, session and authentication contexts.

| Input API | Current result |
| --- | --- |
| `input(name)` | `std::string` |
| `all`, `only`, `except` | String-to-string map |
| `query(name)`, `parameter(name)`, `header(name)` | String view |
| `json()` | Parsed `Json` |
| `validate(rules)` | Validated string map or exception |
| `check(rules)` | Result with values and errors |

Native multiword names include `expects_json`, `is_json`, `content_type`, `user_agent`, `client_ip` and `bearer_token`. `only`/`except` take initializer lists in C++.

## Limits and planned work

The broader typed input, default-value overloads, upload conveniences and camelCase methods in the design specification are not all public methods of the current Request class. Multipart and transport capabilities must not be confused with a complete Request upload API.

## Implementation references

- [include/gungnir/http/request.hpp](../include/gungnir/http/request.hpp)

See the [documentation index](README.md), [getting started](getting-started.md), and [target design](design/request.md).
