# Database application acceptance contract

Group 3 continues the accepted post-v1 order after the application SDK work in
#186/#187. Existing database adapters and transaction scopes are retained.

## Verified gaps and implementation

Generated projects linked/registered PostgreSQL, MySQL and SQL Server but omitted
MongoDB. MongoDB now participates in the same SDK target discovery and bootstrap
registration, without application-side repair.

The public `database.transaction(...)` example failed strict compilation with
`GNR2202`/`GNR2212`. Its canonical semantic binding now targets a small wrapper
around the existing `database::Transaction`. A synchronous zero-parameter
callback may return a typed value or void. Invalid callback values, parameters
and async functions fail before C++ emission. Nested work uses the runtime's
existing connection scope and savepoint ownership rules.

Live acceptance exposed missing generated primary keys on PostgreSQL and SQL
Server model inserts. Model saves now request the actual primary-key column via
`RETURNING`/`OUTPUT INSERTED`, aliased for the existing adapter result contract;
the original three-argument insert compiler remains available for query inserts.
MySQL `id()` now matches `foreignId()` unsigned key types, and savepoint commands
use the native direct protocol because the prepared protocol rejects them.
MongoDB nullable schema fields now allow explicit BSON null, including nullable
enum values. Compiler regression checks cover these schema and insert contracts.

## Acceptance boundaries

`tests/application_database.py` installs the SDK, creates a project with its CLI,
copies ordinary `.gnr` fixture sources and builds them. The handwritten bootstrap
only configures services and observes query/pool counters. It does not implement
ORM operations, migration work or transaction recovery.

The fixture verifies migrate/idempotence/status, prepared string bindings,
Unicode, nullable/hidden values, CRUD and dirty-field persistence, route binding
and 404s, pagination, unique/fillable constraints, eager/inverse relationships
with two queries and a one-slot pool, transaction capabilities, server restart,
rollback and a clean re-migration. Native SQL tests additionally verify inner
savepoint rollback with a continuing parent, parent rollback after nested commit,
after-commit promotion/discard and released pool leases.

SQLite runs in primary validation. PostgreSQL 16, MySQL 8.4, SQL Server 2022 and
MongoDB 8.0 have live service jobs on both pull requests and main. MongoDB uses C
driver 2.5.4 pinned to source commit `ad87ab88907a0105823469fb5d393ed717bed9ba`.
SQL Server installs the actual Microsoft ODBC Driver 18; unixODBC alone is not a
usable SQL Server backend.

MongoDB's adapter reports no transaction/savepoint support on any topology.
The acceptance explicitly checks rejection before writes and foreign-key
non-enforcement, instead of claiming SQL equivalence or replica-set transactions.
Native document JSON and exact decimal backend differences remain subject to
their existing adapter contracts; this gate does not claim complete dialect
parity or production durability/capacity certification.

The published version, release tag and historical release audit are not changed
by this group. Optional network adapters remain custom SDK dependencies.
Local checks and exact-commit CI evidence are recorded in the pull request.
