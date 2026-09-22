# Migrations and Database Development

Gungnir migrations use table and column terminology directly.

```cpp
class CreateUsersTable : public Migration {
public:
    void up() override {
        Table::create("users", [](Column& column) {
            column.id();
            column.string("name");
            column.string("email").unique();
            column.timestamps();
        });
    }

    void down() override {
        Table::drop_if_exists("users");
    }
};
```

## Schema operations

The migration DSL supports table creation, alteration, rename and drop operations; common scalar, text, JSON, UUID and temporal columns; indexes; composite indexes; foreign keys; referential actions; timestamps; soft deletes; column rename/drop; and index/foreign-key removal.

Plans are compiled for the active database backend. Backend-specific SQL belongs in the compiler rather than application migrations.

## Registry

`migration::Registry` keeps migration identity explicit and rejects duplicate or empty names before execution.

```cpp
migration::Registry migrations;
migrations.add("2026_09_22_000001_create_users", create_users);
runner.migrate(migrations.all());
```

Migration names are persistent database identities. Once deployed, rename them only as a deliberate migration-history operation.

## Runner

The runner provides:

- migrate pending migrations
- rollback the latest batch
- reset all applied batches
- migration status

Each migration is executed with its repository record in the same transaction contract where the backend supports transactions.

## Backend limitations

DDL transaction guarantees vary by database engine. Gungnir does not claim atomic schema changes when the underlying driver/database cannot provide them.

MongoDB schema evolution has different semantics from relational DDL. The migration compiler must preserve that distinction rather than manufacturing SQL-like guarantees for document collections.
