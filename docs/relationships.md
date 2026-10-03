# ORM Relationships

Relationships connect typed models. Declare a public, synchronous, parameterless
method that returns one of the six relationship helpers.

```gnr
model User {
    profile() { return hasOne<Profile>(); }
    posts() { return hasMany<Post>(); }
    roles() { return belongsToMany<Role>(); }
    firstComment() { return hasOneThrough<Comment, Post>(); }
    comments() { return hasManyThrough<Comment, Post>(); }
}

model Post {
    author() { return belongsTo<User>("user_id"); }
    comments() { return hasMany<Comment>(); }
}
```

Through helpers take the related model first and the intermediate model second.
Related types can be imported models, including module aliases. Self relationships
use the same syntax.

## Keys

Keys default to the models' declared primary keys. Foreign keys combine the
singular snake-case model name and its key, such as `user_id` or `tenant_uuid`.
`belongsTo` instead combines the relationship name and the related key: an
`author` relationship defaults to `author_id`. The default pivot table joins the
two singular model names alphabetically, such as `role_user`.

Override keys with positional or named string literals:

```gnr
model Tenant {
    primaryKey = "uuid";
    incrementing = false;
    casts = {"uuid": "string"};
    projects() { return hasMany<Project>(foreignKey: "owner_uuid"); }
}

model Project {
    owner() { return belongsTo<Tenant>("owner_uuid"); }
}
```

| Helper | Key arguments, in order |
| --- | --- |
| `hasOne`, `hasMany` | `foreignKey`, `localKey` |
| `belongsTo` | `foreignKey`, `ownerKey` |
| `belongsToMany` | `pivotTable`, `foreignPivotKey`, `relatedPivotKey`, `parentKey`, `relatedKey` |
| `hasOneThrough`, `hasManyThrough` | `firstKey`, `secondKey`, `localKey`, `secondLocalKey` |

Key names must be valid attribute or table identifiers. Foreign-key fields are
typed from their referenced keys; an explicitly declared field must agree.
Declare an optional field, such as `int? parent_id`, when a relationship permits
a null foreign key. Create the actual columns and pivot tables with migrations.
Include foreign keys in `fillable` when assigning them through `create` or `update`.

## Eager loading and loaded values

```gnr
const users = User::with(["posts.comments", "profile", "roles"]).get();
```

Eager loading batches parent keys. Nested paths load each requested edge in
batches; pivot and through relationships query their intermediate data as well.
Query counts depend on the requested paths rather than the number of parents.

Property access inspects the loaded relationship:

```gnr
const loaded = user.posts.loaded();
const posts = user.posts.get();
const count = user.posts.size();
const profile = user.profile.value();
```

Many-valued `get()` returns `List<Related>`. Single-valued `get()` returns a model
and throws when loaded empty; `value()` returns `Related?` instead. `loaded()`
distinguishes an unloaded relationship from a loaded empty one. Loaded-value
access and many-valued `empty()` throw `RelationNotLoaded` if no load has occurred.
Single-valued `empty()` is false until loaded. `unload()`
clears the loaded state. Traversing loaded values performs no database queries.

## Scoped queries

Calling the relationship method creates a query scoped to that parent:

```gnr
const posts = user.posts().where("title", "!=", "draft").orderBy("id", "desc").get();
```

The scoped query returns ordinary ORM results and supports filtering, retrieval,
pagination and mutations. It does not require or populate the property cache.
See [ORM](orm.md) for terminal operations and result types.

## Pivot mutations

```gnr
let user = User::with("roles").findOrFail(id);
user.roles.attach([role_id]);
user.roles.detach(role_id);
user.roles.detach();
```

`attach` and `detach` accept a related key or a list of related keys and return
the affected pivot count. Calling `detach()` removes all links for that parent.
Keys use the relationship's configured `relatedKey` type. Mutations require a
mutable model and invalidate its loaded relationship cache. Parent and related
records remain intact.
