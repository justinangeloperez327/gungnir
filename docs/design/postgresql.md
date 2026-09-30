# PostgreSQL Adapter

> **Design specification.** This document preserves the intended contract. Examples and requirements below may exceed the current implementation. See the [current implementation guide](../postgresql.md) before using an API.

Gungnir provides an optional PostgreSQL adapter backed by libpq.

This document defines the backend/runtime contract. Application-facing queries continue to use the ORM and migration APIs.

# Enablement

The adapter is optional and may be enabled through the Gungnir build configuration.

Applications using it link the PostgreSQL adapter/runtime target required by the build system.

# Registration

The adapter registers explicitly with the database driver registry during application bootstrap.

After registration, a PostgreSQL-configured connection can be used by:

- ORM;
- migrations;
- transactions;
- validation database rules;
- health checks.

Application models/controllers do not use libpq handles.

# Parameterization

PostgreSQL execution uses bound parameters.

Runtime values are passed separately from SQL text.

Raw PostgreSQL execution may use native PostgreSQL placeholder numbering such as:

~~~text
$1
$2
...
~~~

ORM-generated queries must remain parameterized.

# Value mapping

The adapter maps PostgreSQL values into Gungnir runtime values.

Common mappings include:

~~~text
BOOLEAN          -> bool
integer families -> integer types
floating values  -> float/double
text-like values -> string
NULL             -> null/optional
~~~

# DECIMAL / NUMERIC

The Gungnir language defines decimal as an exact application-level decimal concept.

If the current PostgreSQL adapter/runtime still maps NUMERIC/DECIMAL through binary floating point, that is a backend implementation limitation and must not be presented as exact decimal support.

Until exact decimal mapping is implemented, applications requiring exact monetary/arbitrary-precision PostgreSQL NUMERIC semantics should use an explicitly supported exact representation or avoid relying on lossy conversion.

The adapter should eventually map NUMERIC/DECIMAL to the canonical Gungnir decimal runtime type.

# Transactions

The adapter should support begin, commit, rollback, and savepoints according to the database runtime contract.

Transaction behavior must be represented through Gungnir runtime APIs rather than exposing libpq transaction commands to application source.

# Cancellation

Where native libpq cancellation is supported, request/application cancellation should interrupt in-flight operations according to the database runtime contract.

Cancellation must remain distinguishable from normal query failure.

# Health

A live health check may use a bounded operation such as a simple SELECT.

Health checks should not mutate application data.

# Connection configuration

Configuration may include:

~~~text
host
port
database
username
password
TLS settings
connect timeout
application name/options
~~~

Credentials belong in runtime secrets/configuration.

# TLS

Production PostgreSQL deployments should use appropriate certificate validation and TLS policy.

Development flags that weaken verification must not become production defaults.

# Pooling

The common database manager/pool owns connection acquisition and lifetime.

The adapter implements physical PostgreSQL connections.

# Async behavior

A PostgreSQL operation is considered truly async only when the runtime path avoids blocking the request executor.

Native cancellation support alone does not make a blocking libpq execution model coroutine-native.

If blocking calls are used, they require the runtime's blocking/offload strategy.

# Integration testing

Live adapter tests should be opt-in and verify:

- connection;
- parameterized execution;
- value mapping;
- transaction behavior;
- cancellation where supported;
- migration/ORM compatibility.

# Design rule

~~~text
Gungnir ORM defines application semantics
PostgreSQL adapter maps them to libpq/PostgreSQL
bound parameters remain mandatory
backend precision and async limits stay explicit
~~~

