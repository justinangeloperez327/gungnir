# ORM

Gungnir's ORM provides typed model querying, hydration, persistence, pagination, soft deletes, eager loading, relationships, and collections.

## Querying models

Use a model as the entry point to a query:

```gnr
const users = User::all();

const active = User::where("active", true)
    .orderBy("name")
    .get();
```

Queries remain composable until a terminal operation such as `get`, `first`, `find`, or a persistence operation executes them.

## Finding records

```gnr
const user = User::find(id);
const required = User::findOrFail(id);
const first = User::where("active", true).first();
const requiredFirst = User::where("active", true).firstOrFail();
```

Methods that may not find a row return an optional result. `OrFail` variants raise the framework's not-found error.

## Filtering

```gnr
const users = User::where("status", "active")
    .where("verified", true)
    .get();
```

Gungnir uses bound database parameters for values rather than interpolating application input into SQL.

## Ordering and limits

```gnr
const users = User::where("active", true)
    .orderBy("created_at", "desc")
    .limit(20)
    .get();
```

## Creating records

```gnr
const user = User::create({
    "name": "Freya",
    "email": "freya@example.com"
});
```

Mass assignment respects the model's `fillable` contract.

## Updating records

Models track changes to persisted attributes and synchronize dirty state after successful persistence.

```gnr
user.name = "Freya Njord";
user.save();
```

Query-based updates are available for bulk operations.

## Deleting records

```gnr
user.remove();
```

Models configured with `softDeletes = true` retain deleted rows and can use the ORM's soft-delete query and restore operations.

## Eager loading

Load related models with the parent query:

```gnr
const users = User::with("posts").get();
```

Eager loading batches parent keys so relationship traversal does not degrade into one query per parent.

## Collections

Multi-record results return typed model collections. Collections provide iteration and application-side collection operations. See [Collections](collection.md).

## Pagination

Paginated queries return a page containing the selected models and pagination metadata.

## Transactions

Use the database transaction API when multiple persistence operations must commit or roll back together. See [Database](database.md).

## Relationships

Relationship queries and mutations use the same ORM infrastructure. See [Relationships](relationships.md).
