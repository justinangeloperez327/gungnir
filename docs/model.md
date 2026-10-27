# Models

Models represent application data and provide the entry point to Gungnir's ORM.

Single and double quotes both create Gungnir strings. Model metadata uses
assignments without PHP's `$` prefix.

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

Declare a string key with `string uuid;` or a matching string cast and
`incrementing = false`. The declared field type supplies the key type; a second
`keyType` setting is unnecessary. Once a model has been persisted, its primary
key cannot be changed. Replicate the model to create a different record.

Use `perPage = 25;` to set the model's default pagination size. An explicit
`paginate(page, perPage)` call overrides it.

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

An alternative to `fillable` is a `guarded` blocklist:

```gnr
model Project {
    string name;
    bool approved = false;
    guarded = ['id', 'approved'];
}
```

A nonempty `fillable` list is authoritative and its listed attributes are
allowed even when `guarded` contains `'*'`. With an empty or absent `fillable`
list, a declared `guarded` list allows known attributes except those it blocks.
`guarded = ['*'];` blocks everything, while `guarded = [];` explicitly allows
all known attributes. Without either contract, mass assignment is denied.
Unknown attributes are never created by mass assignment. `fill()` and `forceFill()` also work on mutable Gungnir model instances.
`forceFill()` bypasses the allowlist but still enforces attribute names and value types.

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

Supported cast names are `bool`/`boolean`, `int`/`integer`/`int64`, `uint64`,
`double`/`float`/`real`, `decimal`, `string`, `json`/`array`/`object`, `date`, and
`datetime`. Decimal casts use exact `Decimal` values. JSON casts retain JSON
values; date and datetime casts use string-backed values. Explicit numeric and
boolean casts accept checked database text representations. Ordinary typed
fields without a cast retain strict conversion rules. SQLite NUMERIC affinity
may store decimal data as floating point; an exact decimal cast cannot restore
precision already lost by the database. Use text storage or a database with an
exact decimal column type when that distinction matters.

## Generated metadata and member names

A model cannot declare a field, relationship, or method with a reserved native
configuration name. Gungnir reports these conflicts before native code generation
(`GNR2308`). These include `table`, `connection`, `fillable`, `guarded`, `hidden`,
`visible`, `casts`, `timestamps`, `perPage`, `createdAt`, `updatedAt`, `dateFormat`,
`default_with`, `default_with_count`, `touches`, and `soft_deletes`. `primaryKey`
and `incrementing` are additionally reserved when their metadata is declared.

Names required by the inherited ORM API, including `save`, `query`, `fill`,
`attributes`, and `primary_key_name`, are also reserved. Keep database column
names distinct from these model operations.

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

Timestamps are opt-in. Customize their columns and storage format explicitly:

```gnr
model Project {
    timestamps = true;
    createdAt = 'created_on';
    updatedAt = 'modified_on';
    dateFormat = 'Y-m-d H:i:s.u';
    perPage = 25;
}
```

Timestamp values use UTC. Supported format tokens are `Y`, `y`, `m`, `d`, `H`,
`i`, `s`, `u` (microseconds), `v` (milliseconds), and `U` (Unix seconds).
Escape literal letters with a backslash. Timestamp column names must be
distinct from the primary key, each other, and `deleted_at`.

## Default values

Use typed field initializers or an `attributes` object for constant defaults:

```gnr
model Project {
    string status = 'draft';
    attributes = {'active': true, 'priority': 1};
    casts = {'active': 'bool', 'priority': 'integer'};
    fillable = ['status', 'active', 'priority'];
}
```

The two forms cannot supply competing defaults for the same attribute. Defaults
initialize the ordinary typed fields, participate in inserts, and are
overwritten by hydrated database values. Persistent fields support scalar,
decimal, JSON, and optional versions of those types. Store structured lists and
objects in a JSON field with an explicit JSON cast.

## Appended accessors

List public, synchronous, zero-argument methods in `appends` to include their
computed values in JSON output:

```gnr
model User {
    string name;
    appends = ['label'];
    string label() { return name + '!'; }
}
```

Appended accessors have const receivers. The compiler rejects direct field
assignments and persistence calls inside them; keep helper functions free of
side effects too. Calls to other model methods require those methods to be
appended accessors as well. They must return a supported scalar, optional scalar, decimal,
or JSON value. `hidden` and `visible` also apply to appended names; `hidden`
takes precedence. Computed values are not written to the database.

## Automatic relationships and counts

```gnr
model User {
    with = ['posts'];
    withCount = ['posts'];
    posts() { return hasMany<Post>('user_id'); }
}
model Post { int user_id; }
```

`with` supplies default eager loads. `withCount` adds a batched count query per
relationship without fetching all related records for that count. Counts appear
as `<relationship>_count` in JSON and as read-only properties in Gungnir, such as
`user.posts_count`. Counts respect `hidden` and `visible` and do not become
persistent model attributes. A count name cannot collide with a persistent
attribute. Cyclic automatic eager loading is rejected at runtime by a bounded
depth guard.

Use `touches` to update a timestamped parent when a child changes:

```gnr
model User { timestamps = true; }
model Post {
    fillable = ['user_id', 'title'];
    touches = ['author'];
    author() { return belongsTo<User>('user_id'); }
}
```

Touched relationships must be `belongsTo` relationships whose parent enables
timestamps. Parent touching follows the parent's column names and date format.
Instance saves, deletes, and restores touch the parent. Bulk query mutations
do not invoke per-instance hooks. Cycles are suppressed. Use an explicit database transaction if child and parent
writes must commit together; a failed parent write is reported to the caller.

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
