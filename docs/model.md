# Models

Models represent application data and provide the entry point to Gungnir's ORM.

A model automatically participates in querying, hydration, persistence, dirty tracking, serialization, relationships, eager loading, and model collections.

## Defining a model

```gnr
model User {
    table = "users";

    fillable = [
        "name",
        "email"
    ];

    hidden = [
        "password"
    ];

    casts = {
        "active": "bool",
        "settings": "json"
    };

    timestamps = true;
}
```

Gungnir uses model metadata to define persistence and serialization behavior. Database tables and columns are created with [migrations](migration.md), not by model declarations.

## Table and connection

By convention, Gungnir derives a plural snake-case table name from the model name. Override it when necessary:

```gnr
model AuditEntry {
    table = "audit_log";
    connection = "reporting";
}
```

## Default table naming

Without a `table` declaration, Gungnir uses plural snake-case names,
including acronym boundaries and common plural endings:

| Model | Default table |
| --- | --- |
| `Product` | `products` |
| `APIKey` | `api_keys` |
| `Company` | `companies` |
| `Buzz` | `buzzes` |

Use `table = "existing_table_name";` to retain a previously deployed
physical table name. In particular, after upgrading an application that used
an implicit table name with consecutive uppercase letters or a final `z`,
confirm the actual schema name before regenerating the application.

## Primary keys

Models use `id` as their conventional primary key. A different key can be declared explicitly:

```gnr
model User {
    primaryKey = "uuid";
    incrementing = false;

    casts = {
        "uuid": "string"
    };
}
```

Incrementing keys use integer-compatible primary-key types.

## Mass assignment

`fillable` defines attributes that may be assigned through mass-assignment operations:

```gnr
model Project {
    fillable = [
        "name",
        "status"
    ];
}
```

## Serialization

Use `hidden` to exclude sensitive attributes and `visible` when a model should expose an explicit allow-list. Hidden attributes take precedence.

```gnr
model User {
    hidden = [
        "password",
        "remember_token"
    ];
}
```

## Casts

Casts define the application type of persisted attributes. When an
attribute is explicitly declared with a type, its `casts` entry must agree with
that type. For example, `bool active` can use `"active": "bool"` but
not `"active": "integer"`. Metadata-only models may declare cast-backed
attributes without separate field declarations.

Duplicate entries in `fillable`, `hidden`, `visible`, and `casts` are
compile-time errors (`GNR2308`).

## Generated metadata and member names

A model cannot declare a field, relationship, or method with the name of a
generated C++ ORM metadata member. Gungnir reports these conflicts before
native code generation (`GNR2308`). The `table` name is always reserved;
other names are reserved when the corresponding metadata is present:
`connection`, `fillable`, `hidden`, `visible`, `casts`,
`primaryKey`, `incrementing`, and `timestamps`. The
`soft_deletes` member is reserved when `softDeletes = true`.

For example, `model Item { string table; }` is invalid because every model
already has generated `table` metadata. Use a different Gungnir member name.

```gnr
model Project {
    casts = {
        "active": "bool",
        "budget": "decimal",
        "settings": "json",
        "starts_at": "datetime"
    };
}
```

## Timestamps and soft deletes

```gnr
model Project {
    timestamps = true;
    softDeletes = true;
}
```

Timestamp-enabled models expose `created_at` and `updated_at`. Soft-delete models expose `deleted_at` and use soft-delete-aware ORM operations.

## Persistence and dirty tracking

Hydrated models retain their original persisted values. Attribute changes make the model dirty until a successful persistence operation synchronizes its original state.

Inside a model method, guard a nullable field before applying compound assignments or postfix `++` and `--`. These updates participate in dirty tracking:

```gnr
model Counter {
    int? count;

    int increment() {
        if (count == null) { return 0; }
        count += 1;
        return count;
    }
}
```

## Relationships

Models declare relationships to other models and can eager-load them to avoid N+1 query patterns. See [Relationships](relationships.md).

## Querying

Models provide the ORM query entry point for retrieval, filtering, ordering, pagination, creation, updates, deletion, and eager loading. See [ORM](orm.md).
