# Gungnir ORM

Gungnir ORM provides the model query layer above the database execution contracts.

## Querying

```cpp
auto users = User::query()
    .where("active", true)
    .order_by("name")
    .limit(25)
    .get();
```

Models also expose concise entry points such as `User::where(...)`, `User::find(...)`, `User::all()`, and `User::paginate(...)`.

## Parameterization

Query plans compile values into database bindings. Values are not interpolated into SQL. The database layer receives the compiled statement and bindings separately.

## Hydration

Rows are hydrated through generated model metadata. Persisted models are marked clean after hydration so dirty tracking represents application changes rather than database state.

## Eager loading

`with()` records relationship loading on the query plan. Relationship loading batches keys instead of issuing one query per parent model. Nested eager loading is supported through dotted relation paths.

Relationship definitions and advanced relationship behavior are developed further in Group 14.

## Query observation

`orm::listen()` can observe executed ORM statements without receiving binding values. Events include the connection, backend, statement, and binding count. This provides an observability hook without copying potentially sensitive bound data.

## Backend boundary

Relational backends compile SQL using backend-specific quoting and placeholders. MongoDB uses its document query representation and rejects relational-only concepts such as joins and row locks.

The ORM does not imply that a concrete production database adapter is installed. Adapter availability remains the responsibility of the database layer.
