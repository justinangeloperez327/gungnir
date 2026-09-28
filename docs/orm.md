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

Parent-scoped relationship queries are available through `orm::relation_query(parent, parent.relation)`. Joined relationship queries qualify their columns and preserve empty-parent semantics so an uninitialized key cannot produce an unbounded query.

Many-to-many relations also expose parent-aware `orm::attach`, `orm::detach`, and transactional `orm::sync` helpers. These execute against the pivot table, participate in query observation, and invalidate loaded relationship state after successful writes.

## Query observation

`orm::listen()` can observe executed ORM statements without receiving binding values. Events include the connection, backend, statement, and binding count. This provides an observability hook without copying potentially sensitive bound data.

## Backend boundary

Relational backends compile SQL using backend-specific quoting and placeholders. MongoDB uses its document query representation and rejects relational-only concepts such as joins and row locks.

The ORM does not imply that a concrete production database adapter is installed. Adapter availability remains the responsibility of the database layer.
