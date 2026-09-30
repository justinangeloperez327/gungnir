# Gungnir ORM

Gungnir ORM is the application-facing model query and persistence layer for `.gnr` applications.

The ORM follows Laravel/Eloquent-style conventions where those conventions fit Gungnir cleanly. Application code uses expressive camelCase APIs such as `orderBy`, `findOrFail`, `whereIn`, `withTrashed`, and `firstOrCreate`.

Gungnir remains a compiled C++23 framework. The familiar application syntax is parsed, validated, and lowered to the native runtime; application developers should not need to write C++ ORM plumbing directly.

This document defines the intended ORM language contract. Compiler and runtime support may be implemented progressively, but public `.gnr` syntax should converge on this contract.

## ORM conventions

Gungnir ORM follows these rules:

- Application-facing `.gnr` APIs use camelCase.
- Native C++ runtime APIs may use snake_case internally.
- Queries are model-centric and chainable.
- Query builder methods do not execute the query until a terminal operation is called.
- Values are passed as database bindings rather than interpolated into SQL.
- Model metadata is generated from Gungnir model declarations.
- Relationship names use the conventions defined in [model.md](model.md).
- Multiple-model results use `Collection<T>`.
- Framework conventions should remove repetitive configuration when behavior can be inferred safely.
- Backend-specific capabilities must not be presented as portable when they are not.

## Quick example

~~~gnr
const users = User::with([
        'profile',
        'roles',
        'posts.comments'
    ])
    .where('active', true)
    .orderBy('name')
    .paginate(25);
~~~

Conceptually:

~~~text
User
  -> with(...)
  -> Query<User>
  -> where(...)
  -> Query<User>
  -> orderBy(...)
  -> Query<User>
  -> paginate(...)
  -> paginator of User
~~~

## Starting a query

Use `query()` when a query needs to be built explicitly:

~~~gnr
const query = User::query();
~~~

Most operations can also begin directly from the model:

~~~gnr
User::where('active', true);
User::orderBy('name');
User::with('posts');
~~~

## Retrieving all records

~~~gnr
const users = User::all();
~~~

The result is:

~~~text
Collection<User>
~~~

## Retrieving query results

Use `get()` to execute a query that returns multiple models:

~~~gnr
const users = User::where('active', true)
    .orderBy('name')
    .get();
~~~

## Finding by primary key

~~~gnr
const user = User::find(id);
~~~

If no matching record exists, `find()` returns the ORM's empty/optional model result rather than raising a model-not-found error.

## Find or fail

Use `findOrFail()` when absence is exceptional:

~~~gnr
const user = User::findOrFail(id);
~~~

A missing model produces a model-not-found error that the framework exception layer may translate into an HTTP 404 response.

## Retrieving the first result

~~~gnr
const user = User::where('active', true)
    .first();
~~~

## First or fail

~~~gnr
const user = User::where('email', email)
    .firstOrFail();
~~~

## Basic where clauses

~~~gnr
const users = User::where('active', true).get();
~~~

Comparison operators may be supplied explicitly:

~~~gnr
const adults = User::where('age', '>=', 18).get();
~~~

Multiple conditions may be chained:

~~~gnr
const users = User::where('active', true)
    .where('verified', true)
    .get();
~~~

## Or conditions

~~~gnr
const users = User::where('role', 'admin')
    .orWhere('role', 'manager')
    .get();
~~~

## whereIn

~~~gnr
const users = User::whereIn('id', ids).get();
~~~

## whereNotIn

~~~gnr
const users = User::whereNotIn('status', [
    'blocked',
    'deleted'
]).get();
~~~

## Null conditions

~~~gnr
const users = User::whereNull('deleted_at').get();
~~~

~~~gnr
const users = User::whereNotNull('email_verified_at').get();
~~~

## Range conditions

~~~gnr
const orders = Order::whereBetween('total', 100, 500).get();
~~~

~~~gnr
const orders = Order::whereNotBetween('total', 100, 500).get();
~~~

## Date conditions

~~~gnr
const users = User::whereDate('created_at', '2026-09-30').get();

const users = User::whereYear('created_at', 2026).get();

const users = User::whereMonth('created_at', 9).get();
~~~

## Text matching

~~~gnr
const users = User::whereLike('name', '%Justin%').get();
~~~

Text matching semantics may vary by database backend and collation.

## Ordering

Use `orderBy()` as the canonical Gungnir ORM API:

~~~gnr
const users = User::orderBy('name').get();
~~~

Descending order:

~~~gnr
const users = User::orderBy('created_at', 'desc').get();
~~~

Multiple order expressions may be chained:

~~~gnr
const users = User::orderBy('status')
    .orderBy('name')
    .get();
~~~

### Latest

~~~gnr
const users = User::latest().get();
~~~

A column may be specified when required:

~~~gnr
const users = User::latest('updated_at').get();
~~~

### Oldest

~~~gnr
const users = User::oldest().get();
~~~

## Limit and offset

~~~gnr
const users = User::limit(25).get();
~~~

~~~gnr
const users = User::offset(25)
    .limit(25)
    .get();
~~~

## Selecting columns

~~~gnr
const users = User::select([
    'id',
    'name',
    'email'
]).get();
~~~

Additional selected columns may be appended:

~~~gnr
const users = User::select('id')
    .addSelect('name')
    .get();
~~~

## Distinct values

~~~gnr
const countries = User::select('country')
    .distinct()
    .get();
~~~

## Aggregates

Count records:

~~~gnr
const total = User::count();
~~~

Count a filtered query:

~~~gnr
const active = User::where('active', true).count();
~~~

Other aggregates:

~~~gnr
const total = Order::sum('total');
const average = Order::avg('total');
const minimum = Order::min('total');
const maximum = Order::max('total');
~~~

Gungnir uses `avg()` as the canonical API rather than introducing a separate `average()` name.

## Existence checks

~~~gnr
const exists = User::where('email', email).exists();
~~~

~~~gnr
const missing = User::where('email', email).doesntExist();
~~~

## Plucking values

Retrieve one column:

~~~gnr
const names = User::pluck('name');
~~~

Retrieve keyed values:

~~~gnr
const users = User::pluck('name', 'id');
~~~

## Pagination

Standard pagination:

~~~gnr
const users = User::orderBy('name')
    .paginate(25);
~~~

Simple pagination:

~~~gnr
const users = User::orderBy('name')
    .simplePaginate(25);
~~~

Cursor pagination:

~~~gnr
const users = User::orderBy('id')
    .cursorPaginate(25);
~~~

Pagination should preserve the query constraints that precede it.

## Processing large result sets

### chunk

~~~gnr
User::orderBy('id')
    .chunk(500, (users) => {
        // process this chunk
    });
~~~

### chunkById

When records may be updated while processing, prefer ID-based chunking:

~~~gnr
User::chunkById(500, (users) => {
    // process this chunk
});
~~~

### lazy

~~~gnr
for (const user of User::lazy()) {
    // process
}
~~~

### lazyById

~~~gnr
for (const user of User::lazyById()) {
    // process
}
~~~

### cursor

~~~gnr
for (const user of User::cursor()) {
    // process one model at a time
}
~~~

The exact buffering and database-cursor strategy is backend/runtime dependent, but the public API should preserve predictable memory behavior.

# Creating and persisting models

## Create

Create and persist a model with object-style data:

~~~gnr
const user = User::create({
    'name': 'Justin',
    'email': 'justin@example.com'
});
~~~

Framework-managed attributes such as timestamps should not need to be supplied manually.

## Instantiate and save

A model may also be created in memory and saved later:

~~~gnr
const user = User();

user.name = 'Justin';
user.email = 'justin@example.com';

user.save();
~~~

## Save changes

~~~gnr
const user = User::findOrFail(id);

user.name = 'Justin Perez';
user.save();
~~~

## Update a model

~~~gnr
user.update({
    'name': 'Justin Perez',
    'active': true
});
~~~

## Mass update

~~~gnr
User::where('active', false)
    .update({
        'status': 'inactive'
    });
~~~

Mass updates operate on the query rather than hydrating every matching model. Lifecycle behavior must therefore be documented explicitly where model events are involved.

## firstOrCreate

~~~gnr
const user = User::firstOrCreate(
    {
        'email': email
    },
    {
        'name': name
    }
);
~~~

The first object describes the attributes used to find an existing record. The second contains values used when a record must be created.

## firstOrNew

~~~gnr
const user = User::firstOrNew(
    {
        'email': email
    },
    {
        'name': name
    }
);
~~~

Unlike `firstOrCreate()`, a newly constructed model is not persisted until `save()` is called.

## updateOrCreate

~~~gnr
const user = User::updateOrCreate(
    {
        'email': email
    },
    {
        'name': name,
        'active': true
    }
);
~~~

## Upsert

~~~gnr
User::upsert(
    users,
    uniqueBy: ['email'],
    update: ['name', 'active']
);
~~~

Upsert behavior depends on the target backend's conflict/uniqueness capabilities and must retain consistent Gungnir semantics where supported.

# Deleting models

## Delete an instance

~~~gnr
const user = User::findOrFail(id);

user.delete();
~~~

## Destroy by identifier

~~~gnr
User::destroy(id);
~~~

Multiple identifiers may be supplied:

~~~gnr
User::destroy(ids);
~~~

## Mass delete

~~~gnr
User::where('status', 'expired')
    .delete();
~~~

# Soft deletes

Enable soft deletion on the model:

~~~gnr
model User {
    softDeletes = true;

    string name;
    string email;
}
~~~

Normal deletion then records the deletion state rather than immediately removing the database row:

~~~gnr
user.delete();
~~~

## Check whether a model is trashed

~~~gnr
if (user.trashed()) {
    // ...
}
~~~

## Include trashed records

~~~gnr
const users = User::withTrashed().get();
~~~

## Retrieve only trashed records

~~~gnr
const users = User::onlyTrashed().get();
~~~

## Restore

~~~gnr
user.restore();
~~~

Queries may restore matching soft-deleted records where supported:

~~~gnr
User::withTrashed()
    .where('status', 'archived')
    .restore();
~~~

## Force delete

~~~gnr
user.forceDelete();
~~~

`forceDelete()` permanently removes the record.

# Eager loading

Relationship definitions live in [model.md](model.md). ORM queries use those relationships for loading and filtering.

## Load one relationship

~~~gnr
const users = User::with('posts').get();
~~~

## Load multiple relationships

~~~gnr
const users = User::with([
    'profile',
    'posts',
    'roles'
]).get();
~~~

## Nested eager loading

~~~gnr
const users = User::with('posts.comments').get();
~~~

## Constrained eager loading

~~~gnr
const users = User::with('posts', (query) => {
    query.where('published', true)
        .orderBy('created_at', 'desc');
}).get();
~~~

## Load relationships after retrieval

~~~gnr
const user = User::findOrFail(id);

user.load('posts');
~~~

## Load only missing relationships

~~~gnr
user.loadMissing('posts');
~~~

## N+1 queries

Prefer eager loading when iterating over models and accessing related data.

Avoid repeatedly causing relationship queries inside a loop:

~~~gnr
const users = User::all();

for (const user of users) {
    const posts = user.posts;
}
~~~

Prefer:

~~~gnr
const users = User::with('posts').get();

for (const user of users) {
    const posts = user.posts;
}
~~~

The ORM should batch relationship loading where possible instead of issuing one database query per parent record.

# Relationship queries

A relationship method also acts as a query entry point.

Given:

~~~gnr
model User {
    posts() {
        return hasMany('posts');
    }
}
~~~

query the relationship:

~~~gnr
const posts = user.posts()
    .where('published', true)
    .orderBy('created_at', 'desc')
    .get();
~~~

## Creating through a relationship

~~~gnr
const post = user.posts().create({
    'title': 'Gungnir ORM',
    'published': true
});
~~~

The parent key should be applied automatically according to the relationship definition.

## Saving through a relationship

~~~gnr
user.posts().save(post);
~~~

# Relationship existence queries

## has

~~~gnr
const users = User::has('posts').get();
~~~

## doesntHave

~~~gnr
const users = User::doesntHave('posts').get();
~~~

## whereHas

~~~gnr
const users = User::whereHas('posts', (query) => {
    query.where('published', true);
}).get();
~~~

## whereDoesntHave

~~~gnr
const users = User::whereDoesntHave('posts', (query) => {
    query.where('published', true);
}).get();
~~~

# Relationship aggregates

## withCount

~~~gnr
const users = User::withCount('posts').get();
~~~

Each returned model may expose:

~~~gnr
user.posts_count
~~~

## withSum

~~~gnr
const users = User::withSum('orders', 'total').get();
~~~

## withAvg

~~~gnr
const users = User::withAvg('orders', 'total').get();
~~~

## withMin

~~~gnr
const users = User::withMin('orders', 'total').get();
~~~

## withMax

~~~gnr
const users = User::withMax('orders', 'total').get();
~~~

# Many-to-many operations

Given a relationship such as:

~~~gnr
roles() {
    return belongsToMany('roles');
}
~~~

## attach

~~~gnr
user.roles().attach(roleId);
~~~

Attach with pivot data:

~~~gnr
user.roles().attach(roleId, {
    'assigned_by': admin.id
});
~~~

## detach

~~~gnr
user.roles().detach(roleId);
~~~

## sync

~~~gnr
user.roles().sync(roleIds);
~~~

## syncWithoutDetaching

~~~gnr
user.roles().syncWithoutDetaching(roleIds);
~~~

## toggle

~~~gnr
user.roles().toggle(roleIds);
~~~

Many-to-many writes should participate in transaction handling where required and invalidate stale loaded relationship state after successful mutation.

# Timestamps

Models use conventional managed timestamps unless disabled:

~~~text
created_at
updated_at
~~~

Disable them on a model only when required:

~~~gnr
model AuditEntry {
    timestamps = false;

    string message;
}
~~~

# Model state

## Dirty tracking

~~~gnr
user.name = 'New Name';

if (user.isDirty()) {
    // model contains unsaved changes
}
~~~

Check a specific attribute:

~~~gnr
if (user.isDirty('name')) {
    // ...
}
~~~

Check whether a model is clean:

~~~gnr
if (user.isClean()) {
    // ...
}
~~~

Check whether a value changed during the most recent persistence operation:

~~~gnr
if (user.wasChanged('email')) {
    // ...
}
~~~

## Original values

~~~gnr
const originalEmail = user.getOriginal('email');
~~~

## Fresh

`fresh()` retrieves a new model instance from the database:

~~~gnr
const freshUser = user.fresh();
~~~

## Refresh

`refresh()` reloads the existing instance:

~~~gnr
user.refresh();
~~~

Loaded relationships may also be refreshed according to runtime support.

# Serialization

Models should serialize naturally when passed to response helpers:

~~~gnr
return json(user);
~~~

Explicit conversion remains available:

~~~gnr
const data = user.toArray();
const jsonText = user.toJson();
~~~

Loaded relationships should participate in serialization.

## Hidden attributes

Models may hide sensitive attributes from serialization:

~~~gnr
model User {
    hidden = [
        'password',
        'remember_token'
    ];

    string name;
    string email;
    string password;
}
~~~

## Visible attributes

A model may instead explicitly define its visible serialization surface:

~~~gnr
model User {
    visible = [
        'id',
        'name',
        'email'
    ];

    string name;
    string email;
}
~~~

# Attribute casts

Models may declare application-level casts:

~~~gnr
model User {
    casts = {
        'active': 'bool',
        'metadata': 'json',
        'created_at': 'datetime'
    };

    bool active;
}
~~~

Casting syntax is part of the intended model/ORM contract. Exact runtime type mappings must remain explicit and portable across supported backends.

# Query scopes

Reusable query constraints should follow familiar ORM conventions while using Gungnir syntax.

## Local scope

~~~gnr
model User {
    scope active(query) {
        return query.where('active', true);
    }
}
~~~

Usage:

~~~gnr
const users = User::active()
    .orderBy('name')
    .get();
~~~

## Dynamic scope

~~~gnr
model User {
    scope ofType(query, type) {
        return query.where('type', type);
    }
}
~~~

Usage:

~~~gnr
const admins = User::ofType('admin').get();
~~~

Global scopes may be added after local scope semantics and compiler validation are stable. Global behavior must remain visible and removable so queries are not modified unpredictably.

# Conditional queries

Use `when()` when a query condition is conditional:

~~~gnr
const users = User::query()
    .when(activeOnly, (query) => {
        query.where('active', true);
    })
    .orderBy('name')
    .get();
~~~

# Transactions

Transactions belong to the database/runtime layer but should expose a concise application API:

~~~gnr
Database::transaction(() => {
    const order = Order::create(orderData);

    Payment::create({
        'order_id': order.id,
        'amount': order.total
    });
});
~~~

If the callback fails, the transaction should roll back according to the selected database backend.

Asynchronous transaction semantics must not be exposed until connection ownership and coroutine suspension are safe across the full runtime.

# Pessimistic locking

Relational backends may support row locking.

## lockForUpdate

~~~gnr
const order = Order::where('id', id)
    .lockForUpdate()
    .firstOrFail();
~~~

## sharedLock

~~~gnr
const order = Order::where('id', id)
    .sharedLock()
    .firstOrFail();
~~~

Locking support is backend-specific and should produce a clear diagnostic or runtime capability error where unsupported.

# Joins

Advanced queries may use joins on relational backends:

~~~gnr
const users = User::join(
        'profiles',
        'users.id',
        '=',
        'profiles.user_id'
    )
    .get();
~~~

## Left joins

~~~gnr
const users = User::leftJoin(
        'profiles',
        'users.id',
        '=',
        'profiles.user_id'
    )
    .get();
~~~

Joins are relational concepts and should not be silently emulated by document backends.

# Grouping and having

~~~gnr
const orders = Order::groupBy('status').get();
~~~

~~~gnr
const orders = Order::groupBy('customer_id')
    .having('total', '>', 5)
    .get();
~~~

# Raw expressions

Raw database expressions are advanced escape hatches and should never be the default query style.

~~~gnr
const totals = Order::selectRaw('count(*) as total').get();
~~~

Possible raw APIs include:

~~~text
selectRaw()
whereRaw()
havingRaw()
orderByRaw()
~~~

Raw expressions bypass portions of Gungnir's normal structural guarantees. Applications must not concatenate untrusted input into raw expressions.

# Query execution

Query-building operations should remain lazy.

Examples of builder operations:

~~~text
query()
select()
addSelect()
distinct()
where()
orWhere()
whereIn()
whereNotIn()
whereNull()
whereNotNull()
whereBetween()
whereHas()
orderBy()
latest()
oldest()
with()
limit()
offset()
groupBy()
having()
~~~

Examples of terminal operations:

~~~text
all()
get()
first()
firstOrFail()
find()
findOrFail()
count()
sum()
avg()
min()
max()
exists()
doesntExist()
pluck()
paginate()
simplePaginate()
cursorPaginate()
create()
update()
delete()
restore()
~~~

The compiler/runtime should make query execution points predictable so developers can understand when database I/O occurs.

# Collections

Queries returning multiple models use Gungnir collections:

~~~gnr
const users = User::where('active', true).get();
~~~

Conceptual type:

~~~text
Collection<User>
~~~

Common collection operations may include:

~~~gnr
users.count();
users.first();
users.isEmpty();
users.map(...);
users.filter(...);
~~~

The complete collection API belongs in a separate `collection.md` specification.

# Hydration

Database results are hydrated using generated model metadata:

~~~text
database row
    -> generated model metadata
    -> model attributes
    -> User instance
~~~

Hydrated models are treated as persisted and clean. Dirty tracking should represent application changes made after hydration rather than the act of loading values from the database.

# Parameter binding

Normal ORM values must be passed separately from generated SQL or backend query structure.

Conceptually:

~~~text
User::where('email', email)

        -> query structure
        -> bound value: email
~~~

Values must not be interpolated directly into SQL.

Identifiers such as column names, table names, raw fragments, ordering identifiers, and similar structural query input require separate validation because database parameter binding generally applies to values, not SQL identifiers.

# Query observation

The runtime may expose ORM query observation for diagnostics and observability.

Observers may receive metadata such as:

~~~text
connection
backend
statement
binding count
duration
~~~

Sensitive bound values should not be copied into logs by default.

The current native runtime hook may use an internal API such as `orm::listen()`; that native name is not required to become the final `.gnr` application API.

# Model lifecycle events

The ORM should integrate model persistence with Gungnir's event system.

Planned lifecycle concepts include:

~~~text
creating
created
updating
updated
saving
saved
deleting
deleted
restoring
restored
~~~

Bulk update/delete behavior must be documented explicitly because operations that execute directly against a query may not hydrate individual model instances.

# Backend portability

Gungnir ORM should provide a common model/query experience across supported database backends where the operation has equivalent semantics.

Relational targets include:

~~~text
SQLite
PostgreSQL
MySQL
SQL Server
~~~

Backend-specific SQL quoting, placeholder syntax, generated identifiers, locking syntax, returning clauses, pagination strategy, and similar implementation details belong to the database adapter.

# MongoDB boundary

MongoDB may share model-oriented concepts such as:

~~~text
find
where
create
update
delete
collections of models
relationships that have meaningful document semantics
~~~

Gungnir must not pretend that relational-only behavior exists where MongoDB cannot provide equivalent semantics.

Examples include:

~~~text
SQL joins
relational pivot tables
row locks
relational foreign-key enforcement
~~~

Backend limitations should be surfaced clearly instead of being hidden behind misleading ORM syntax.

# Naming convention

Gungnir's public ORM API uses camelCase.

Examples:

| Gungnir application API | Possible native C++ implementation |
| --- | --- |
| `orderBy` | `order_by` |
| `whereIn` | `where_in` |
| `whereNotIn` | `where_not_in` |
| `findOrFail` | `find_or_fail` |
| `firstOrFail` | `first_or_fail` |
| `firstOrCreate` | `first_or_create` |
| `updateOrCreate` | `update_or_create` |
| `withTrashed` | native equivalent |
| `onlyTrashed` | native equivalent |
| `forceDelete` | `force_delete` |
| `lockForUpdate` | `lock_for_update` |
| `simplePaginate` | native equivalent |
| `cursorPaginate` | native equivalent |

Native names are implementation details. Documentation and normal `.gnr` application code should use the Gungnir names.

# ORM compiler contract

ORM syntax must eventually be represented structurally by the compiler.

For example:

~~~gnr
const users = User::where('active', true)
    .orderBy('name')
    .get();
~~~

should conceptually pass through:

~~~text
source
  -> lexer
  -> parser
  -> expression/call AST
  -> symbol resolution
  -> model and ORM semantic analysis
  -> typed/validated AST
  -> ORM lowering
  -> C++23 generation
~~~

The validated representation should understand the query types:

~~~text
User
  -> ModelSymbol<User>

User::where(...)
  -> Query<User>

.orderBy(...)
  -> Query<User>

.get()
  -> Collection<User>
~~~

The C++23 backend may then lower the validated operation to the native runtime:

~~~cpp
auto users = User::where("active", true)
    .order_by("name")
    .get();
~~~

The transpiler should not implement ORM syntax by scanning raw source text for method names and replacing strings.

# Complete controller example

~~~gnr
controller UserController {
    Response index() {
        const users = User::with([
                'profile',
                'roles',
                'posts.comments'
            ])
            .where('active', true)
            .orderBy('name')
            .paginate(25);

        return view('users/index', {
            'users': users
        });
    }

    Response show(int id) {
        const user = User::with('posts')
            .findOrFail(id);

        return json(user);
    }

    Response store(Request request) {
        const data = request.validate({
            'name': 'required|string',
            'email': 'required|email'
        });

        const user = User::create(data);

        return json(user, 201);
    }

    Response update(Request request, int id) {
        const user = User::findOrFail(id);

        user.update(request.only([
            'name',
            'email'
        ]));

        return json(user);
    }

    Response destroy(int id) {
        const user = User::findOrFail(id);

        user.delete();

        return response(null, 204);
    }
}
~~~

# Design principles

The Gungnir ORM should remain:

- familiar to developers coming from Laravel/Eloquent;
- convention-first;
- expressive without becoming dynamically ambiguous;
- statically analyzable by the Gungnir compiler;
- safe by default with parameterized values;
- explicit about database I/O and backend limitations;
- efficient enough to prevent common N+1 and unnecessary hydration patterns;
- compatible with generated, inspectable C++23;
- independent of C++ template syntax in normal application code.

When Laravel already has a widely understood ORM method name and that behavior maps cleanly to Gungnir, prefer the familiar name instead of inventing a new one.
