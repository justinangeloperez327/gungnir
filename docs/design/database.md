# Database Runtime

> **Gungnir 1.x design specification.** This document describes intended long-term architecture beyond the frozen `1.0.0-rc.1` contract. Examples and requirements may exceed the current implementation. See the [current implementation guide](../database.md) before using an API.


## 1.0 RC alignment

The shipped baseline is `1.0.0-rc.1`. The 1.0 contracts are frozen; this design document may describe additive 1.x evolution or future-major work, but current implementation guides and executable tests remain authoritative.

The database runtime is the execution layer beneath the ORM, migrations, transactions, and database-backed validation.

Application-facing query semantics are defined by orm.md and migration.md. This document defines the runtime and adapter boundary.

# Responsibilities

The database layer provides:

- named connections;
- driver registration;
- connection acquisition;
- parameterized execution;
- transactions;
- cancellation;
- health checks;
- pooling;
- backend capability reporting.

Application code should not need native client-library handles.

# Connections

Applications configure named connections.

A connection executes a statement with a separate bindings collection:

~~~text
statement
bindings[]
~~~

Runtime values must not be interpolated directly into SQL text.

# Drivers

The core database contract is implemented by backend adapters.

Supported adapter families include:

~~~text
SQLite
PostgreSQL
MySQL / MariaDB-compatible client
SQL Server
MongoDB
~~~

Concrete availability depends on build and runtime configuration.

# Driver registry

Drivers register through an explicit registry/application bootstrap path.

Runtime adapter discovery must not depend on unspecified global static initialization order.

# ORM boundary

The ORM lowers Gungnir query intent into backend/runtime database operations.

Example application code:

~~~gnr
const user = User::where('email', email)
    .first();
~~~

The database layer receives a parameterized query plan or statement. It does not parse Gungnir source.

# Parameterization

Bindings are always separate runtime values.

This is a security invariant.

Example conceptual SQL:

~~~text
SELECT ... WHERE email = ?
bindings = [email]
~~~

The exact placeholder syntax is backend-specific.

# Pooling

Connection pooling is owned by the database runtime/manager.

A pool should define:

- maximum size;
- acquisition timeout;
- idle policy;
- health/reconnect behavior;
- shutdown behavior.

Application models/controllers should not manually return native connections to pools.

# Transactions

Transactions preserve one logical transaction context and define commit, rollback, nested/savepoint behavior where supported, failure propagation, and backend capability limitations.

# Async transactions

A synchronous thread-affine transaction must not be carried across arbitrary await points.

Async transactions require coroutine-safe connection ownership, coroutine/request transaction context, and suspension-safe driver behavior.

Until that contract is implemented for a backend/runtime, async transaction use should be rejected rather than emulated unsafely.

# Cancellation

Database operations should observe cancellation when the adapter supports it.

Adapters must document whether cancellation is:

~~~text
native in-flight cancellation
cooperative boundary cancellation
unsupported
~~~

# Errors

Database failures should map into stable framework error categories such as:

~~~text
connection
timeout
cancelled
constraint
transaction
query
configuration
unsupported capability
~~~

Raw vendor error text may be logged but should not automatically be exposed to clients.

# Values

Backend values map into Gungnir semantic/runtime values.

Mappings must preserve language guarantees.

Exact decimal semantics must not be silently presented as guaranteed when an adapter currently maps DECIMAL/NUMERIC through binary floating point. Adapter limitations must remain explicit.

# Health

A driver may provide a bounded live health check suitable for readiness use.

Health checks should not mutate application data.

# Backend capabilities

Drivers should expose capabilities rather than forcing higher layers to guess from the driver name.

Examples:

~~~text
transactions
savepoints
DDL transactions
native cancellation
returning clauses
JSON/document operations
foreign keys
async execution
~~~

# MongoDB

MongoDB remains document-native.

The ORM/migration layers may normalize common model behavior, but the runtime must not pretend relational guarantees exist where MongoDB does not provide them.

# Security

Database credentials come from configuration/secrets.

Generated C++ must not embed environment credentials.

Query values remain bound parameters.

# Shutdown

Pools and connections participate in application shutdown.

New acquisition should stop during shutdown and outstanding owned work should drain or cancel according to lifecycle policy.

# Design rule

~~~text
ORM defines application query semantics
database runtime executes parameterized operations
driver adapts one backend
backend limitations stay explicit
~~~

