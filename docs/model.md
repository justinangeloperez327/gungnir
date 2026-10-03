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

Casts define the application type of persisted attributes:

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

## Relationships

Models declare relationships to other models and can eager-load them to avoid N+1 query patterns. See [Relationships](relationships.md).

## Querying

Models provide the ORM query entry point for retrieval, filtering, ordering, pagination, creation, updates, deletion, and eager loading. See [ORM](orm.md).
