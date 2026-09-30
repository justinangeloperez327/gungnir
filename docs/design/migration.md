# Migrations

> **Design specification.** This document preserves the intended contract. Examples and requirements below may exceed the current implementation. See the [current implementation guide](../migration.md) before using an API.

A Gungnir migration defines **database schema changes**.

Migrations are the schema authority for an application. They define:

- tables and collections;
- columns and column types;
- default values;
- nullable behavior;
- indexes;
- unique constraints;
- primary keys;
- foreign-key constraints;
- referential actions;
- timestamps;
- soft-delete columns;
- schema changes over time.

Models do not duplicate this information. Models only describe persistence metadata and relationships.

## Basic migration

A migration is declared with the `migration` keyword:

```gnr
migration CreateUsersTable {
    up() {
        Table::create('users', (table) => {
            table.id();
            table.string('name');
            table.string('email');
            table.string('password');
            table.timestamps();
        });
    }

    down() {
        Table::dropIfExists('users');
    }
}
```

The `up()` method applies the schema change.

The `down()` method reverses it.

## Migration responsibility

A migration answers questions such as:

```text
Which tables exist?
Which columns exist?
What type is each column?
Which columns are nullable?
What are the default values?
Which columns are indexed?
Which values must be unique?
Which columns are primary keys?
Which foreign keys exist?
What happens when referenced records are deleted or updated?
Which schema changes were applied over time?
```

Migrations do not contain application business logic.

# Table creation

Create a table with `Table::create()`:

```gnr
migration CreateUsersTable {
    up() {
        Table::create('users', (table) => {
            table.id();
            table.string('name');
            table.string('email');
            table.timestamps();
        });
    }

    down() {
        Table::dropIfExists('users');
    }
}
```

The callback receives a table builder used to declare columns, indexes, and constraints.

# Table modification

Modify an existing table with `Table::alter()`:

```gnr
migration AddPhoneToUsersTable {
    up() {
        Table::alter('users', (table) => {
            table.string('phone').nullable();
        });
    }

    down() {
        Table::alter('users', (table) => {
            table.dropColumn('phone');
        });
    }
}
```

# Rename a table

```gnr
migration RenameCustomersToClients {
    up() {
        Table::rename('customers', 'clients');
    }

    down() {
        Table::rename('clients', 'customers');
    }
}
```

# Drop a table

```gnr
Table::drop('users');
```

Prefer `dropIfExists()` when rollback or environment differences may make the table optional:

```gnr
Table::dropIfExists('users');
```

# Column types

Gungnir should provide a compact set of database-oriented column types.

## Primary key

```gnr
table.id();
```

This declares the conventional incrementing primary key:

```text
id
```

A named key may be declared when required:

```gnr
table.id('user_id');
```

## UUID

```gnr
table.uuid('uuid');
```

A UUID primary key may be declared explicitly:

```gnr
table.uuid('uuid').primary();
```

## String

```gnr
table.string('name');
```

Optional length:

```gnr
table.string('name', 150);
```

## Text

```gnr
table.text('description');
```

Long text:

```gnr
table.longText('content');
```

## Integer

```gnr
table.integer('quantity');
```

Other integer sizes may include:

```gnr
table.smallInteger('priority');
table.bigInteger('external_id');
table.unsignedInteger('count');
table.unsignedBigInteger('reference_id');
```

## Boolean

```gnr
table.boolean('active');
```

## Decimal

```gnr
table.decimal('price', 12, 2);
```

## Float and double

```gnr
table.float('score');
table.double('latitude');
```

## JSON

```gnr
table.json('settings');
```

## Date and time

```gnr
table.date('birth_date');
table.time('starts_at');
table.dateTime('published_at');
table.timestamp('verified_at');
```

## Binary

```gnr
table.binary('payload');
```

# Column modifiers

Column declarations may be refined with modifiers.

## Nullable

```gnr
table.string('phone').nullable();
```

## Default value

```gnr
table.boolean('active').default(true);
```

```gnr
table.string('status').default('pending');
```

## Unique

```gnr
table.string('email').unique();
```

## Primary

```gnr
table.uuid('uuid').primary();
```

## Index

```gnr
table.string('email').index();
```

## Unsigned

Where supported:

```gnr
table.integer('quantity').unsigned();
```

Backend-specific restrictions must be validated rather than silently ignored.

# Timestamps

Use `timestamps()` for the conventional timestamp columns:

```gnr
table.timestamps();
```

This represents:

```text
created_at
updated_at
```

The model may use:

```gnr
timestamps = true;
```

but the migration is responsible for creating the physical columns.

# Soft deletes

Use:

```gnr
table.softDeletes();
```

to create the conventional soft-delete column:

```text
deleted_at
```

The corresponding model may enable persistence behavior with:

```gnr
softDeletes = true;
```

The migration creates the column. The model tells the ORM to use it.

# Foreign keys

Foreign-key constraints belong in migrations.

A conventional foreign identifier:

```gnr
table.foreignId('user_id')
    .constrained('users');
```

This expresses:

```text
posts.user_id -> users.id
```

## Referential actions

Cascade on delete:

```gnr
table.foreignId('user_id')
    .constrained('users')
    .cascadeOnDelete();
```

Restrict deletion:

```gnr
table.foreignId('user_id')
    .constrained('users')
    .restrictOnDelete();
```

Set the foreign key to null:

```gnr
table.foreignId('user_id')
    .nullable()
    .constrained('users')
    .nullOnDelete();
```

Cascade on update:

```gnr
table.foreignId('user_id')
    .constrained('users')
    .cascadeOnUpdate();
```

Referential actions should map to native backend capabilities. Unsupported actions must not be silently emulated.

# Explicit foreign keys

For non-conventional schemas:

```gnr
table.foreign('author_id')
    .references('id')
    .on('users');
```

With actions:

```gnr
table.foreign('author_id')
    .references('id')
    .on('users')
    .cascadeOnDelete();
```

# Composite foreign keys

Where supported by the backend:

```gnr
table.foreign([
        'tenant_id',
        'user_id'
    ])
    .references([
        'tenant_id',
        'id'
    ])
    .on('users');
```

# Indexes

## Standard index

```gnr
table.index('email');
```

## Unique index

```gnr
table.unique('email');
```

## Composite index

```gnr
table.index([
    'tenant_id',
    'created_at'
]);
```

## Composite unique constraint

```gnr
table.unique([
    'tenant_id',
    'email'
]);
```

## Named index

When a specific database identifier is required:

```gnr
table.index(
    ['status', 'created_at'],
    name: 'orders_status_created_at_index'
);
```

Normally Gungnir should generate conventional index names automatically.

# Pivot tables

Many-to-many relationships are represented physically through migrations.

For:

```text
User <-> Role
```

a migration may define:

```gnr
migration CreateRoleUserTable {
    up() {
        Table::create('role_user', (table) => {
            table.foreignId('user_id')
                .constrained('users')
                .cascadeOnDelete();

            table.foreignId('role_id')
                .constrained('roles')
                .cascadeOnDelete();

            table.unique([
                'user_id',
                'role_id'
            ]);
        });
    }

    down() {
        Table::dropIfExists('role_user');
    }
}
```

The model relationship:

```gnr
roles() {
    return belongsToMany('roles');
}
```

describes how the ORM traverses the relationship.

The migration defines the physical pivot table and constraints.

# Polymorphic columns

Polymorphic relationships require physical identifier/type columns.

For example:

```gnr
table.unsignedBigInteger('commentable_id');
table.string('commentable_type');

table.index([
    'commentable_type',
    'commentable_id'
]);
```

Gungnir may later provide convenience helpers for polymorphic columns, but the resulting physical schema must remain explicit and inspectable.

# Rename a column

```gnr
Table::alter('users', (table) => {
    table.renameColumn('username', 'name');
});
```

The rollback should reverse the operation:

```gnr
Table::alter('users', (table) => {
    table.renameColumn('name', 'username');
});
```

# Drop columns

Drop one column:

```gnr
Table::alter('users', (table) => {
    table.dropColumn('phone');
});
```

Drop multiple columns:

```gnr
Table::alter('users', (table) => {
    table.dropColumns([
        'phone',
        'fax'
    ]);
});
```

# Drop indexes

```gnr
Table::alter('users', (table) => {
    table.dropIndex('users_email_index');
});
```

Drop a unique constraint/index:

```gnr
Table::alter('users', (table) => {
    table.dropUnique('users_email_unique');
});
```

# Drop foreign keys

```gnr
Table::alter('posts', (table) => {
    table.dropForeign('posts_user_id_foreign');
});
```

Where the compiler/runtime can infer the conventional constraint name safely, a column-based overload may also be supported:

```gnr
table.dropForeign(['user_id']);
```

# Complete users migration

```gnr
migration CreateUsersTable {
    up() {
        Table::create('users', (table) => {
            table.id();

            table.string('name');
            table.string('email').unique();
            table.string('password');

            table.boolean('active')
                .default(true);

            table.timestamp('verified_at')
                .nullable();

            table.json('settings')
                .nullable();

            table.timestamps();
            table.softDeletes();

            table.index('active');
        });
    }

    down() {
        Table::dropIfExists('users');
    }
}
```

The matching model contains only persistence metadata:

```gnr
model User {
    fillable = [
        'name',
        'email',
        'password'
    ];

    hidden = [
        'password'
    ];

    casts = {
        'active': 'bool',
        'settings': 'json',
        'verified_at': 'datetime'
    };

    softDeletes = true;

    posts() {
        return hasMany('posts');
    }
}
```

The migration owns the schema. The model does not repeat column declarations.

# Complete related-table example

```gnr
migration CreatePostsTable {
    up() {
        Table::create('posts', (table) => {
            table.id();

            table.foreignId('user_id')
                .constrained('users')
                .cascadeOnDelete();

            table.string('title');
            table.text('body');

            table.boolean('published')
                .default(false);

            table.timestamp('published_at')
                .nullable();

            table.timestamps();

            table.index([
                'user_id',
                'published'
            ]);
        });
    }

    down() {
        Table::dropIfExists('posts');
    }
}
```

The relationship remains in the model:

```gnr
model Post {
    fillable = [
        'title',
        'body',
        'published',
        'published_at'
    ];

    casts = {
        'published': 'bool',
        'published_at': 'datetime'
    };

    user() {
        return belongsTo('user');
    }
}
```

# Reversible migrations

Every migration should define both:

```text
up()
down()
```

unless the migration is intentionally irreversible and explicitly marked as such by a future migration contract.

The normal rule is:

```text
up()   -> apply the change
down() -> undo the change
```

Examples:

```text
create table  <-> drop table
add column    <-> drop column
rename A->B   <-> rename B->A
add index     <-> drop index
add FK        <-> drop FK
```

Rollback behavior should be understandable by reading the migration itself.

# Migration ordering and identity

Migration identity must be persistent and ordered.

A migration file or registry entry may use a timestamped identity such as:

```text
2026_09_30_000001_create_users_table
2026_09_30_000002_create_posts_table
```

The identity determines ordering and records which migrations have already been applied.

Once deployed, a migration's identity should not be casually renamed because it participates in migration history.

# Migration execution

The migration runner should support:

```text
migrate
rollback
reset
refresh
status
```

Conceptually:

```text
migrate   -> apply pending migrations
rollback  -> revert the latest applied batch
reset     -> revert all applied migrations
refresh   -> rollback and migrate again
status    -> show applied/pending migrations
```

Exact CLI commands are documented separately by the CLI documentation.

# Migration transactions

When the active database supports transactional DDL, Gungnir should execute a migration and its migration-history update within the appropriate transaction contract.

However, DDL transaction guarantees differ by database backend.

Gungnir must not claim atomic schema rollback when the selected database cannot provide it.

# Backend compilation

Migration syntax is database-independent where practical.

For example:

```gnr
table.string('email').unique();
```

is not raw SQL.

The migration compiler/runtime is responsible for translating the operation appropriately for:

```text
SQLite
PostgreSQL
MySQL
SQL Server
```

Backend-specific quoting, generated constraint names, type mapping, identity syntax, and DDL behavior belong below the application migration layer.

# MongoDB boundary

MongoDB schema evolution is not relational DDL.

Gungnir should not pretend that MongoDB has relational tables, SQL foreign keys, or row constraints.

MongoDB migration support may cover meaningful document/database changes such as:

- creating or dropping collections;
- creating or dropping indexes;
- renaming collections;
- data-shape migration where explicitly supported;
- validator/index configuration supported by the MongoDB adapter.

Relational-only operations must produce an explicit unsupported-capability result rather than being silently simulated.

# What is not allowed in a migration

A migration is a schema-evolution declaration.

It should not contain:

- HTTP handling;
- controller behavior;
- application-service orchestration;
- mail sending;
- authentication;
- authorization;
- unrelated business workflows;
- arbitrary network calls;
- long-running background jobs.

For example, this does not belong in a migration:

```gnr
migration CreateOrdersTable {
    up() {
        Table::create('orders', (table) => {
            table.id();
        });

        sendWelcomeEmails();
        callExternalService();
    }
}
```

Migrations should remain deterministic database-evolution operations.

# Migration and model separation

The intended separation is:

```text
Migration
  -> physical schema

Model
  -> persistence metadata
  -> relationships

ORM
  -> querying and persistence
```

For example:

```gnr
migration CreateUsersTable {
    up() {
        Table::create('users', (table) => {
            table.id();
            table.string('name');
            table.string('email').unique();
            table.string('password');
            table.boolean('active').default(true);
            table.timestamps();
        });
    }

    down() {
        Table::dropIfExists('users');
    }
}
```

and:

```gnr
model User {
    fillable = [
        'name',
        'email',
        'password'
    ];

    hidden = [
        'password'
    ];

    casts = {
        'active': 'bool'
    };
}
```

There is no duplicated model field declaration.

# Responsibility split

| Concern | Owner |
| --- | --- |
| Table/collection existence | Migration |
| Columns | Migration |
| Column types | Migration |
| Nullability | Migration |
| Database defaults | Migration |
| Primary-key constraint | Migration |
| Indexes | Migration |
| Unique constraints | Migration |
| Foreign-key constraints | Migration |
| Referential actions | Migration |
| Timestamp columns | Migration |
| Soft-delete column | Migration |
| Table mapping | Model |
| Connection mapping | Model |
| Primary-key mapping | Model |
| Mass assignment | Model |
| Serialization hiding | Model |
| Attribute casts | Model |
| Relationship declaration | Model |
| Queries and persistence | ORM |

# Migration grammar

A migration should eventually be represented approximately as:

```text
MigrationDeclaration
  name
  up
    SchemaOperation[]
  down
    SchemaOperation[]
```

Schema operations should be structured nodes such as:

```text
CreateTable
AlterTable
RenameTable
DropTable

AddColumn
RenameColumn
DropColumn

AddPrimaryKey
AddIndex
AddUnique
AddForeignKey

DropPrimaryKey
DropIndex
DropUnique
DropForeignKey
```

A column declaration should be represented structurally:

```text
ColumnDeclaration
  name
  type
  nullable
  defaultValue?
  primary
  unique
  indexed
  unsigned
  options
```

# Semantic validation

The migration semantic pass should validate at least:

- migration names are valid and unique;
- both `up()` and `down()` use valid migration operations;
- table names are valid;
- column names are valid;
- column types are supported;
- modifier combinations are valid;
- primary-key declarations are valid;
- referenced tables and columns are structurally valid where project information is available;
- index definitions contain valid columns;
- duplicate columns are rejected;
- duplicate constraint definitions are rejected;
- foreign-key actions are valid;
- backend-incompatible operations are reported clearly;
- arbitrary application behavior is rejected.

# Compiler contract

A migration such as:

```gnr
migration CreatePostsTable {
    up() {
        Table::create('posts', (table) => {
            table.id();

            table.foreignId('user_id')
                .constrained('users')
                .cascadeOnDelete();

            table.string('title');
            table.timestamps();
        });
    }

    down() {
        Table::dropIfExists('posts');
    }
}
```

should pass through:

```text
source
  -> lexer
  -> parser
  -> MigrationDeclaration AST
  -> SchemaOperation AST
  -> symbol/schema resolution
  -> semantic validation
  -> validated migration AST
  -> backend-neutral migration lowering
  -> database-specific execution/DDL generation
```

Before backend compilation, the validated representation should already know:

```text
migration        CreatePostsTable
operation        create table
table            posts

column           id
type             primary identifier

column           user_id
foreign table    users
foreign column   id
on delete        cascade

column           title
type             string

timestamps       enabled
```

The transpiler must not discover migration operations by scanning raw source strings.

# Naming convention

Public Gungnir migration APIs use camelCase:

```text
dropIfExists
dropColumn
dropColumns
renameColumn
foreignId
cascadeOnDelete
restrictOnDelete
nullOnDelete
cascadeOnUpdate
smallInteger
bigInteger
unsignedInteger
unsignedBigInteger
longText
dateTime
softDeletes
```

Native C++ runtime APIs may use different internal names. Those internal names are not part of the `.gnr` language contract.

# Design rule

The migration contract is intentionally strict:

```text
migration = database schema evolution
```

Models describe how application records map to that schema.

ORM code queries and persists those records.

Business logic belongs elsewhere.

