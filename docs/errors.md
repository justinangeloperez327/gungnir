# Errors

## Overview
Compiler diagnostics carry stable code, level, message, source location and a half-open source span. Structured compilation enriches diagnostics with the original source line and normalized end position before results leave the compiler. `gungnirc` uses the shared diagnostic renderer, showing source context and a span underline, and exits nonzero on errors. Diagnostic ordering is deterministic across multi-file compilation.

Generated C++ carries `#line` directives at callable and statement boundaries when line directives are enabled. Native compiler diagnostics are therefore mapped back to the closest originating `.gnr` statement instead of only the generated C++ file.

Native errors and exceptions cover HTTP, validation, ORM, database, view and storage failures. `ExceptionHandler` defines HTTP rendering at the router boundary.

## Scope
The structured compiler has an authoritative `ValidatedProject` gate for the supported language profile. Native compilation can still report backend/toolchain failures, but ordinary supported-language errors must be diagnosed before lowering. Multi-line diagnostic rendering currently emphasizes the first source line of the span; richer secondary labels and cross-backend native diagnostic normalization remain future work. Exception rendering must still be configured appropriately for production.



- [include/gungnir/language/diagnostic.hpp](../include/gungnir/language/diagnostic.hpp)
- [include/gungnir/language/diagnostic_renderer.hpp](../include/gungnir/language/diagnostic_renderer.hpp)
- [include/gungnir/http/exception_handler.hpp](../include/gungnir/http/exception_handler.hpp)
- [include/gungnir/errors/error.hpp](../include/gungnir/errors/error.hpp)

See the [documentation index](README.md), [getting started](getting-started.md), and [target design](design/errors.md).
