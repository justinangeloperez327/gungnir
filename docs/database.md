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

Nested transaction behavior follows the selected database driver's transaction/savepoint capabilities.

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
