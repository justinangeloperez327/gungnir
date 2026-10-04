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

`all()` and `get()` return `Collection<Model>`. `query()` returns `Query<Model>`.

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

Use a comparison string for other operators and a list for membership:

```gnr
const users = User::where("id", ">=", 1).whereIn("id", [1, 2, 3]).get();
```

Comparisons are `=`, `==`, `!=`, `<>`, `<`, `<=`, `>` and `>=`, plus `like`.
`orWhere` combines another predicate; `whereNotIn` excludes list members. An
empty `whereIn` list matches no rows. Filter values are scalar or optional scalar
attributes; column names are validated by the database query compiler.

## Ordering and limits

```gnr
const users = User::where("active", true)
    .orderBy("created_at", "desc")
    .limit(20)
    .get();
```

## Creating records

```gnr
let user = User::create({
    "name": "Freya",
    "email": "freya@example.com"
});
```

Mass assignment respects the model's `fillable` contract.

## Updating records

Use a mutable `let` binding for persistence changes. Models track changes to
persisted attributes and synchronize dirty state after successful persistence.

```gnr
user.name = "Freya Njord";
user.save();
```

Query-based updates are available for bulk operations.

```gnr
const changed = User::where("id", id).update({"name": "Freya"});
```

Instance `save`, `update`, `remove`, `forceRemove`, `restore`, `touch` and `refresh`
return `bool`. Query `update`, `remove`, `forceRemove` and `restore` return an
affected-row count. `dirty()` inspects any changed field; `isDirty("name")`
checks one field. `exists()` reports whether a model was persisted.

## Deleting records

```gnr
user.remove();
```

Models configured with `softDeletes = true` retain deleted rows and can use the ORM's soft-delete query and restore operations.

```gnr
const deleted = User::onlyDeleted().get();
const restored = User::withDeleted().where("id", id).restore();
```

Ordinary queries exclude soft-deleted rows. `withDeleted` includes them,
`onlyDeleted` restricts results to them, and `forceRemove` permanently removes
matching rows.

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

```gnr
const page = User::orderBy("id").paginate(1, 20);
const users = page.data;
const total = page.total;
const more = page.hasMore();
```

The type is `Page<Model>`. Its fields are `data`, `currentPage`, `perPage`, `total`
and `lastPage`; `empty`, `hasMore` and `hasPrevious` inspect the page. Page numbers
and sizes must be positive; defaults are page 1 and 15 records per page.

## Transactions

Use the database transaction API when multiple persistence operations must commit or roll back together. See [Database](database.md).

## Relationships

Relationship queries and mutations use the same ORM infrastructure. See [Relationships](relationships.md).
