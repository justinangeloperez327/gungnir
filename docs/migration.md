# Migrations

Migrations version an application's database structure.

## Creating a migration

```sh
gungnir make:migration CreateUsers
```

## Migration declaration

```gnr
migration CreateUsers {
    up() {
        Table::create("users", (table) => {
            table.id();
            table.string("name");
            table.string("email").unique();
            table.timestamps();
        });
    }

    down() {
        Table::dropIfExists("users");
    }
}
```

`up` applies the migration and `down` reverses it.

## Columns

The migration API provides common column types including strings, text, integer families, booleans, decimals, floating-point values, JSON, UUIDs, dates/times, timestamps, binary data, foreign IDs, and soft-delete timestamps.

## Modifiers

Columns can be configured with modifiers such as nullable values, defaults, uniqueness, indexes, primary keys, unsigned numeric behavior, and foreign-key references.

## Indexes and foreign keys

Migrations can create, remove, and rename indexes and define foreign-key constraints with delete/update behavior.

## Altering tables

Use `Table::alter` to change an existing table.

## Running migrations

The Gungnir CLI applies pending migrations in order and records completed migrations. Rollback operations execute the corresponding `down` methods.

## Portability

The migration layer translates supported operations to the selected database backend. Database-specific features should be isolated when an application must remain portable across engines.
