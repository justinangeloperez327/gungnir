# SQL Server Adapter

> **Future design specification.** This document describes intended long-term architecture beyond the published `1.0` contract. Examples and requirements may exceed the current implementation. See the [current implementation guide](../sqlserver.md) before using an API.


## Release alignment

This document describes possible future architecture beyond 1.0. Current implementation guides and executable tests define the published contract.

Gungnir provides an optional SQL Server adapter through ODBC.

This document defines the backend/runtime contract. Application-facing code uses the common ORM, migration, and database APIs.

# Runtime dependency

The adapter uses the platform ODBC manager and requires a compatible Microsoft SQL Server ODBC driver at runtime.

Current deployments should use a supported modern Microsoft ODBC driver, with Driver 18 as the expected baseline where configured by the repository.

# Registration

The adapter registers explicitly with the database driver registry during application bootstrap.

Configured SQL Server connection aliases may then be used by ORM, migrations, transactions, validation rules, and health checks.

# Parameterization

The adapter uses prepared ODBC statements and bound parameters.

Runtime values must remain separate from SQL text.

Native SQL Server raw statements typically use question-mark placeholders:

~~~text
SELECT ... WHERE email = ?
~~~

ORM-generated queries must remain parameterized.

# Encryption

Modern SQL Server ODBC drivers default toward encrypted connections.

Production deployments should validate server certificates.

TrustServerCertificate-style development options should be explicit and must not become secure-production defaults.

# Value mapping

Common mappings include:

~~~text
BIT              -> bool
integer families -> integer types
floating values  -> float/double
text-like values -> string
NULL             -> null/optional
~~~

# DECIMAL / NUMERIC

The Gungnir language defines decimal as an exact application-level decimal concept.

If the current ODBC adapter maps SQL Server DECIMAL/NUMERIC through binary Double, that is a known adapter limitation.

Applications requiring exact financial/decimal semantics must not rely on a lossy mapping.

The target adapter should map DECIMAL/NUMERIC to Gungnir's canonical decimal runtime representation.

# Transactions

The adapter should support begin, commit, rollback, and savepoints/capability reporting through the common database runtime.

Backend behavior should be exposed through capabilities rather than application ODBC code.

# Cancellation

Where ODBC/native statement cancellation is supported, request/application cancellation should interrupt in-flight execution according to the database runtime contract.

# Health

A bounded live query may be used for readiness.

Health checks should not mutate application data.

# Pooling

Connection pooling is owned by the common database manager/pool.

The adapter owns physical ODBC connection/session behavior.

# Async behavior

A blocking ODBC operation is not made non-blocking merely because it is called from an async controller.

Blocking calls must use an offload strategy or a future genuinely asynchronous execution path.

# Configuration

Connection configuration may include:

~~~text
server
port
database
username
password
driver
encryption options
timeouts
additional reviewed ODBC options
~~~

Secrets remain runtime configuration.

# Integration testing

Live tests should verify parameter binding, transactions, value mapping, cancellation where supported, migration compatibility, and TLS configuration expectations.

# Design rule

~~~text
Gungnir application API stays backend-neutral
ODBC remains inside the adapter
bound parameters are mandatory
encryption and decimal limitations are explicit
~~~

