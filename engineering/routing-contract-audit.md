# Canonical routing contract audit

Baseline: merged main `5dce2f9524c91e9e318788ae4d75af8bbf3bc5b8`
(PR #179). Its tree `6669cf2b1a0f207a81e24c081ba9976ce2c673be` matches
the verified PR head. All seven preceding workflows passed, including the
GCC/Clang/MSVC generated-service fixture and compiler-output comparison.

## Proven contract gap

The public routing guide described fluent names, constraints, middleware and
groups, but canonical parsing rejected module-scope `Route` declarations. CLI
assembly excluded `routes/` from ordinary project compilation and translated a
small legacy-parser subset into handwritten native route calls. Typed model
arguments, grouped declarations and named URL helpers had no canonical
application path. Native routers, middleware registries, exception handling,
controller normalization and ORM lookups already existed and are retained.

## Canonical declarations and structural IR

Route declarations now have explicit syntax, resolved controller/action symbols,
parameter roles, middleware and constraints. Validation flattens nested prefixes,
name prefixes and middleware in declaration order. All seven HTTP methods,
fallbacks, typed and aliased middleware, names, numeric/UUID/regex constraints,
and explicit controller imports use the ordinary project compiler.

Invalid action visibility/results/parameters, duplicate names or URI parameters,
nonliteral paths, malformed constraints and unsupported modifiers fail before
emission. Validation-only and emission modes produce identical diagnostics and
source spans. `Route` is a static API; known nonobject URL parameter containers
and nonscalar values are rejected before C++ compilation. Dynamic JSON objects
retain runtime validation.

The C++ IR has typed route and binding nodes referenced by route-registration
statements. Its verifier checks IDs, controller/action presence, binding roles,
method/path shape, middleware representation and fallback options. The emitter
serializes those nodes into module registration functions using the existing
native router. CLI assembly calls these functions and validates the complete
project before writing any generated output. It no longer reparses routes with
the compatibility frontend or a separate semantic index.

## Controller binding and ownership

Action arguments bind by URI parameter name. `Request` can occupy any one action
parameter position. Scalar conversion requires a complete, in-range value and
rejects nonfinite floating-point values; invalid values produce 404 before the
action runs. Model binding reads native primary-key metadata and delegates one
bound lookup to `find_or_fail`, preserving custom string keys, named connections,
hidden serialization and the default soft-delete scope.

A function coroutine owns the resolved controller, binding names and argument
tuple across awaited actions. The registration lambda is not a coroutine and
does not lend its closure to a suspended call. Existing direct native route and
binding APIs remain available.

Complete projects also exposed a pre-existing source-composition defect:
relationship argument expression IDs were not remapped when combining syntax
arenas. That remapping now accompanies fields, metadata and route arguments.
A multi-module model-binding regression and the installed CLI application cover
explicit relationship keys after earlier modules have contributed expressions.

## Named URL fidelity

Native URL generation walks the original template once and percent-encodes
parameter bytes using the unreserved set from
[RFC 3986](https://www.rfc-editor.org/rfc/rfc3986). Substituted braces cannot become
new placeholders. Dynamic matching splits segments first and decodes once;
encoded slashes remain inside one parameter and literal plus signs remain plus
signs. Constraints run on the decoded value. Empty URL values, control bytes and
malformed incoming percent escapes are rejected.

## Acceptance evidence

The generated routing fixture executes 17 ordinary routes and one fallback. It
covers all HTTP methods, scalar ordering/range failures, typed/aliased nested
middleware and global ordering, constraints, request-context URL helpers and
percent-encoded round trips. SQLite execution checks asynchronous model actions,
missing/soft-deleted rows, custom string keys on a named connection, hidden model
fields, single-query binding and a quoted SQL-like key passed as a bound value.

Local GCC 13 Debug, SQLite and password hashing enabled, TLS disabled:
**96 CTest checks pass**, including the installed CLI's nested routing, database
model binding, URL helpers, fallback, invalid-route output preservation,
migrations, incremental builds and development restart. **Eight installed SDK
consumers pass**: native API, ORM contract, SQLite ORM, HTTP, request context,
authentication, cache/storage and routing. **All ten public routing examples
pass strict project validation**. Public-header isolation, documentation and
development consistency, workflow YAML and whitespace checks pass.

CI executes generated routing and the installed routing consumer. Compiler
conformance executes the fixture with GCC, Clang and MSVC and compares its emitted
C++, validated AST and structural IR alongside the preceding fixtures. Published
remote outcomes are recorded in the pull request after verification.

## Remaining product requirements

This evidence closes the documented canonical routing and primary-key model
binding path. It does not establish the complete application-service acceptance
gate or live equivalence across every database backend. Resource authorization,
validation/uploads, events/jobs/scheduling, mail/notifications and the wider
backend/security/performance requirements still need their own integration
evidence. The baseline product matrix remains the record of those requirements.
