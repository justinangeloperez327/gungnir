# Database

Gungnir provides a unified database layer for application connections, queries, transactions, migrations, ORM persistence, and supported SQL and document databases.

## Connections

Applications configure named database connections through environment and configuration values. A default connection is used unless a model or operation selects another connection.

The [application SDK](sdk-packages.md) includes SQLite. Other database adapters
require a custom source build with their native dependencies. A core SDK can run
an HTTP application with `DB_CONNECTION` empty.

## Supported databases

Gungnir provides adapters for:

- SQLite
- PostgreSQL
- MySQL and MariaDB
- Microsoft SQL Server
- MongoDB

Database-specific documentation explains configuration and capabilities for each adapter.

## Connection pools

Database connections are managed through pools so requests and background work can lease connections without opening a new network connection for every operation.

## Transactions

Use transactions when a group of operations must commit atomically:

```gnr
database.transaction(() => {
    const project = Project::create(data);
    AuditEntry::create({
        "project_id": project.id,
        "action": "created"
    });
});
```

The callback runs synchronously with no parameters. Its return value is returned
by `database.transaction`; an exception rolls back the scope before propagating.
ORM operations inside the callback use the same leased connection. Nested
transactions use savepoints on SQLite, PostgreSQL, MySQL and SQL Server.

The current MongoDB adapter supports document persistence but does not implement
transaction sessions or savepoints. It rejects transaction work before invoking
the callback, including when the server itself has a replica-set topology.

## Queries

The query layer uses bound parameters for application values. Identifiers that must be dynamic are validated separately rather than being treated as bound values.

## ORM

Models use the database layer for hydration, persistence, eager loading, pagination, and relationships.

See [ORM](orm.md).

## Migrations

Database structure is managed through migrations.

See [Migrations](migration.md).

## Multiple connections

Models can select a named connection through model metadata, allowing an application to separate operational, reporting, or service-specific data stores.

## MongoDB

MongoDB uses the same application database manager but retains document-database semantics where SQL concepts do not apply.

## Application acceptance

The database acceptance suite builds an ordinary `.gnr` application through the
installed CLI and SDK. It exercises migrations, bound queries, CRUD, nullable and
hidden values, Unicode, pagination, route model binding, eager and inverse
relationships, persistence across a server restart, and migration rollback.
Eager loading uses two queries for multiple parents with a one-slot pool.

SQLite runs in the primary validation job. PostgreSQL, MySQL, SQL Server and
MongoDB have live service jobs on pull requests and `main`. SQL jobs also check
nested savepoint rollback and after-commit callbacks through the native API.
MongoDB checks explicitly cover its transaction and foreign-key limitations.

See the adapter guides for native dependencies and deployment configuration.
