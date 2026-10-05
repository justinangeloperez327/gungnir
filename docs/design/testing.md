# Testing

> **Future design specification.** This document describes intended long-term architecture beyond the published `1.0` contract. Examples and requirements may exceed the current implementation. See the [current implementation guide](../testing.md) before using an API.


## Release alignment

This document describes possible future architecture beyond 1.0. Current implementation guides and executable tests define the published contract.

Gungnir testing should exercise real framework behavior with minimal fake infrastructure.

The testing layer is not a second implementation of routing, middleware, validation, authentication, or responses.

# HTTP tests

HTTP test helpers dispatch Request values through the real router and middleware pipeline without requiring a network socket.

Convenience operations may include:

~~~text
GET
POST
PUT
PATCH
DELETE
generic send
~~~

The same controller and middleware code used in production should execute.

# Response assertions

Testing helpers may assert:

- status;
- headers;
- body text;
- JSON shape/value;
- redirect location;
- cookies;
- validation errors.

Assertions should produce focused source-level failure messages.

# Application test lifecycle

Each test application should boot and shut down deterministically.

Tests must not depend on process-global state left by previous test cases.

# Dependency injection

Tests may override container bindings with fakes or in-memory adapters.

Overrides should be scoped to the test application/container.

# Database isolation

Database tests require explicit isolation such as:

- transaction rollback where safe;
- test database/schema;
- deterministic reset/migration strategy.

Do not assume all backends support the same DDL transaction behavior.

# Cache/session isolation

Use isolated stores or namespaces.

Shared developer Redis/session keys should not leak between tests.

# Queue testing

Queue tests may use MemoryDriver to inspect dispatched jobs without requiring a production broker.

Tests should still validate stable job names and serialized payloads.

# Scheduler testing

Use an injected/fake clock.

Do not sleep real wall-clock time merely to test scheduling logic.

# Mail/notification testing

Use test transports/channels that capture structured messages.

Do not send real external email/SMS/push during unit tests.

# Storage testing

Use isolated temporary roots or memory/test disks.

Path-containment/security behavior should also have adapter-specific integration tests.

# Compiler tests

The language/compiler requires tests for:

~~~text
lexer
parser
Syntax AST
semantics
Validated AST
lowering
C++ emitter
native compile
runtime behavior
~~~

A parser success test alone does not prove generated code works.

# Generated-code tests

Representative Gungnir fixtures should be transpiled and compiled under supported C++ compilers.

This catches runtime/header/codegen drift.

# Adapter integration tests

Database, Redis, SMTP, and other external adapters should have opt-in live integration tests in addition to unit tests.

# No hidden reset

The framework should not silently wipe external resources between tests.

Destructive cleanup must be explicit and test-environment scoped.

# Design rule

~~~text
test the real framework path
replace external infrastructure deliberately
isolate state explicitly
keep compiler and generated-code tests first-class
~~~

