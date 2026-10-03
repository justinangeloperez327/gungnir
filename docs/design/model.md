# Models

> **Development design specification.** This document describes intended long-term architecture beyond the frozen `development` contract. Examples and requirements may exceed the current implementation. See the [current implementation guide](../model.md) before using an API.


## Development alignment

Gungnir is under active development. No 1.0 compatibility contract is frozen; this document may describe intended future architecture, but current implementation guides and executable tests remain authoritative.

A Gungnir model is a **database mapping declaration**.

Its responsibility is intentionally narrow:

- identify the database table or collection;
- identify the database connection when needed;
- identify the primary key and key behavior when non-standard;
- define which attributes are mass assignable;
- define which attributes are hidden during serialization;
- define which attributes require casting;
- define persistence options such as timestamps or soft deletes;
- declare relationships.

A model is **not** a business-logic class and is **not** the database schema definition.

Database columns, column types, indexes, constraints, and foreign keys belong to migrations.

## Basic model

The smallest model is:

```gnr
model User {
}
```

By convention, Gungnir infers:

```text
model       User
table       users
primaryKey  id
connection  default
timestamps  true
```

The ORM supplies querying and persistence behavior automatically.

## Model responsibility

A model answers only these questions:

```text
Which table or collection does this model represent?
Which database connection does it use?
What is its primary key?
Which attributes may be mass assigned?
Which attributes should be hidden when serialized?
Which attributes require type conversion?
Which persistence options are enabled?
Which relationships exist?
```

Business logic, HTTP behavior, validation workflows, authorization, events, and application services belong elsewhere.

# Table

Use `table` only when the conventional table name is not correct.

```gnr
model User {
    table = 'app_users';
}
```

Without an override:

```gnr
model User {
}
```

Gungnir conventionally maps:

```text
User -> users
Post -> posts
OrderItem -> order_items
```

# Connection

Use `connection` when the model should use a non-default database connection.

```gnr
model AuditLog {
    connection = 'reporting';
}
```

A model that omits `connection` uses the application's default database connection.

# Primary key

The conventional primary key is:

```text
id
```

Use `primaryKey` when the table uses a different key:

```gnr
model User {
    primaryKey = 'user_id';
}
```

Gungnir model configuration uses camelCase, so the canonical spelling is:

```text
primaryKey
```

not:

```text
primary_key
```

## Non-incrementing keys

For UUIDs or other manually assigned keys:

```gnr
model ApiClient {
    primaryKey = 'uuid';
    incrementing = false;
    keyType = 'string';
}
```

The default behavior is equivalent to:

```text
primaryKey = 'id'
incrementing = true
keyType = inferred/default integer key
```

# Fillable attributes

`fillable` defines which attributes may be supplied through mass-assignment operations such as `create()` and `update()`.

```gnr
model User {
    fillable = [
        'name',
        'email',
        'password'
    ];
}
```

This permits:

```gnr
const user = User::create({
    'name': 'Justin',
    'email': 'justin@example.com',
    'password': password
});
```

Attributes not allowed by the model's mass-assignment policy must not be silently assigned through object-style mass assignment.

For example, if `is_admin` is not fillable:

```gnr
User::create({
    'name': 'Justin',
    'is_admin': true
});
```

must not silently elevate the value through mass assignment.

`fillable` is persistence metadata. It does not define whether a database column exists; the migration remains the schema authority.

# Hidden attributes

`hidden` controls model serialization.

```gnr
model User {
    hidden = [
        'password',
        'remember_token'
    ];
}
```

Hidden attributes may still exist in the database and on the hydrated model. They are excluded from serialized output such as:

```gnr
return json(user);
```

Typical hidden attributes include:

```text
password
remember_token
security tokens
internal secrets
```

`hidden` affects representation, not database storage.

# Casts

`casts` defines attributes whose database representation should be converted to an application-level type.

```gnr
model User {
    casts = {
        'active': 'bool',
        'settings': 'json',
        'verified_at': 'datetime'
    };
}
```

Conceptually:

```text
database representation -> Gungnir representation

0 / 1                   -> bool
JSON value              -> object/map
timestamp               -> datetime
integer value           -> int
decimal value           -> decimal
```

Only attributes that require explicit conversion need to appear in `casts`.

Casting is model metadata because it describes how persisted values are interpreted after hydration and before persistence.

# Timestamps

Models use conventional timestamps by default:

```text
created_at
updated_at
```

Disable them when the mapped table does not use timestamps:

```gnr
model AuditEntry {
    timestamps = false;
}
```

`timestamps` describes persistence behavior. It does not declare the timestamp columns; migrations define the schema.

# Soft deletes

Enable soft deletion when the mapped table contains the appropriate soft-delete column:

```gnr
model User {
    softDeletes = true;
}
```

The ORM can then apply soft-delete behavior to operations such as:

```gnr
user.delete();

User::withTrashed().get();

user.restore();

user.forceDelete();
```

`softDeletes` describes persistence behavior. The migration remains responsible for creating the required database column.

# Relationships

Relationships are the only method-like declarations allowed inside a model.

A relationship declaration describes how the mapped database record relates to another model.

```gnr
model User {
    posts() {
        return hasMany('posts');
    }
}
```

Relationship bodies are intentionally restricted. They are declarations, not general-purpose model methods.

A relationship body should resolve to a supported relationship declaration.

# hasOne

Use `hasOne` when one model owns one related record.

```gnr
model User {
    profile() {
        return hasOne('profile');
    }
}
```

Conventionally:

```text
profiles.user_id -> users.id
```

# hasMany

Use `hasMany` when one model owns multiple related records.

```gnr
model User {
    posts() {
        return hasMany('posts');
    }
}
```

Conventionally:

```text
posts.user_id -> users.id
```

The relationship resolves to a collection of `Post` models.

# belongsTo

Use `belongsTo` when the current model references its parent.

```gnr
model Post {
    user() {
        return belongsTo('user');
    }
}
```

Conventionally:

```text
posts.user_id -> users.id
```

# belongsToMany

Use `belongsToMany` for a many-to-many relationship.

```gnr
model User {
    roles() {
        return belongsToMany('roles');
    }
}
```

Inverse side:

```gnr
model Role {
    users() {
        return belongsToMany('users');
    }
}
```

By convention, Gungnir may infer a pivot table such as:

```text
role_user

role_id
user_id
```

# hasOneThrough

Use `hasOneThrough` to access one related model through an intermediate model.

```text
Mechanic -> Car -> Owner
```

```gnr
model Mechanic {
    owner() {
        return hasOneThrough('owner', 'car');
    }
}
```

# hasManyThrough

Use `hasManyThrough` to access multiple related records through an intermediate model.

```text
Country -> Users -> Posts
```

```gnr
model Country {
    posts() {
        return hasManyThrough('posts', 'users');
    }
}
```

# morphOne

Use `morphOne` for one polymorphic related record.

```gnr
model User {
    image() {
        return morphOne('image', 'imageable');
    }
}

model Post {
    image() {
        return morphOne('image', 'imageable');
    }
}
```

# morphMany

Use `morphMany` for many polymorphic related records.

```gnr
model Post {
    comments() {
        return morphMany('comments', 'commentable');
    }
}

model Video {
    comments() {
        return morphMany('comments', 'commentable');
    }
}
```

# morphTo

Use `morphTo` on the inverse side of a polymorphic relationship.

```gnr
model Comment {
    commentable() {
        return morphTo('commentable');
    }
}
```

# morphToMany

Use `morphToMany` for the owning side of a polymorphic many-to-many relationship.

```gnr
model Post {
    tags() {
        return morphToMany('tags', 'taggable');
    }
}
```

# morphedByMany

Use `morphedByMany` for the inverse side of a polymorphic many-to-many relationship.

```gnr
model Tag {
    posts() {
        return morphedByMany('posts', 'taggable');
    }

    videos() {
        return morphedByMany('videos', 'taggable');
    }
}
```

# Relationship reference

| Relationship | Meaning | Typical result |
| --- | --- | --- |
| `hasOne()` | One owned child | Related model |
| `hasMany()` | Many owned children | `Collection<T>` |
| `belongsTo()` | Parent model | Related model |
| `belongsToMany()` | Many-to-many | `Collection<T>` |
| `hasOneThrough()` | One related record through another model | Related model |
| `hasManyThrough()` | Many related records through another model | `Collection<T>` |
| `morphOne()` | One polymorphic child | Related model |
| `morphMany()` | Many polymorphic children | `Collection<T>` |
| `morphTo()` | Polymorphic parent | Resolved model |
| `morphToMany()` | Polymorphic many-to-many | `Collection<T>` |
| `morphedByMany()` | Inverse polymorphic many-to-many | `Collection<T>` |

# Relationship modifiers

Relationship modifiers refine the database relationship declaration.

## latestOfMany

```gnr
latestOrder() {
    return hasOne('orders').latestOfMany();
}
```

## oldestOfMany

```gnr
oldestOrder() {
    return hasOne('orders').oldestOfMany();
}
```

## ofMany

```gnr
largestOrder() {
    return hasOne('orders').ofMany('price', 'max');
}
```

# Relationship key overrides

Normal relationships should use convention where possible.

Explicit key configuration is available for non-standard schemas.

```gnr
posts() {
    return hasMany(
        'posts',
        foreignKey: 'author_id'
    );
}
```

Custom local key:

```gnr
posts() {
    return hasMany(
        'posts',
        foreignKey: 'author_id',
        localKey: 'uuid'
    );
}
```

# Relationship naming

Use singular names for relationships that return one model:

```gnr
profile() {
    return hasOne('profile');
}

user() {
    return belongsTo('user');
}
```

Use plural names for collection relationships:

```gnr
posts() {
    return hasMany('posts');
}

roles() {
    return belongsToMany('roles');
}
```

# Convention-based relationship resolution

Resource names are resolved through Gungnir conventions.

For example:

```gnr
hasMany('posts')
```

is conceptually resolved as:

```text
posts
  -> singularize
post
  -> model name
Post
  -> resolve model declaration
model Post
```

Application code therefore does not need:

```text
hasMany<Post>()
Post::class
```

Those are native implementation concerns.

# Complete model example

```gnr
model User {
    table = 'users';
    primaryKey = 'id';

    fillable = [
        'name',
        'email',
        'password'
    ];

    hidden = [
        'password',
        'remember_token'
    ];

    casts = {
        'active': 'bool',
        'settings': 'json',
        'verified_at': 'datetime'
    };

    timestamps = true;
    softDeletes = true;

    profile() {
        return hasOne('profile');
    }

    posts() {
        return hasMany('posts');
    }

    roles() {
        return belongsToMany('roles');
    }
}
```

If the model follows all conventions, the declaration can be much smaller:

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

    posts() {
        return hasMany('posts');
    }
}
```

# What is not allowed in a model

Models must not become general-purpose application classes.

The following do not belong inside a Gungnir model:

- arbitrary business methods;
- HTTP actions;
- request validation workflows;
- authorization logic;
- service orchestration;
- mail sending;
- event handling;
- transactions;
- query scopes declared as arbitrary methods;
- computed business workflows;
- arbitrary statement blocks unrelated to relationships.

For example, this should be rejected:

```gnr
model Order {
    approve() {
        sendEmail();
        updateInventory();
    }
}
```

Business behavior belongs in an application/service layer.

Likewise, this is not a relationship declaration and should be rejected:

```gnr
model User {
    posts() {
        const posts = Post::all();
        return posts;
    }
}
```

A relationship declaration must use a recognized relationship form.

# Migrations are the schema authority

Models do not declare database columns.

Do not duplicate migration schema like this:

```gnr
model User {
    string name;
    string email;
    bool active;
}
```

Instead, the migration defines the database schema:

```gnr
migration CreateUsersTable {
    up() {
        Table::create('users', (table) => {
            table.id();
            table.string('name');
            table.string('email');
            table.string('password');
            table.boolean('active');
            table.timestamps();
        });
    }
}
```

The model only declares persistence metadata:

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

This avoids defining the same schema twice.

# Responsibility split

| Concern | Owner |
| --- | --- |
| Database columns | Migration |
| Column types | Migration |
| Default database values | Migration |
| Indexes | Migration |
| Unique constraints | Migration |
| Foreign-key constraints | Migration |
| Table/collection mapping | Model |
| Database connection mapping | Model |
| Primary-key mapping | Model |
| Key behavior | Model |
| Mass-assignment policy | Model |
| Serialization hiding | Model |
| Attribute casts | Model |
| Timestamp behavior | Model |
| Soft-delete behavior | Model |
| ORM relationships | Model |
| Query construction | ORM |
| Business logic | Application/service layer |
| HTTP handling | Controller |
| Input validation | Validation/request layer |
| Authorization | Policy |
| Events | Event/listener |
| Database execution | ORM/database runtime |

# Model grammar

A Gungnir model should ultimately be represented approximately as:

```text
ModelDeclaration
  name
  table?
  connection?
  primaryKey?
  incrementing?
  keyType?
  fillable[]
  hidden[]
  casts{}
  timestamps?
  softDeletes?
  relationships[]
```

A relationship should be represented independently:

```text
RelationshipDeclaration
  name
  kind
  relatedResource
  throughResource?
  foreignKey?
  localKey?
  options
```

The compiler should not represent arbitrary model methods as normal executable methods.

# Semantic validation

The model semantic pass should validate at least:

- only supported model configuration keys are used;
- model configuration values have the correct type;
- duplicate configuration entries are rejected;
- `primaryKey` is a valid attribute name;
- `fillable` contains valid attribute names;
- `hidden` contains valid attribute names;
- `casts` uses supported cast types;
- relationships use supported relationship kinds;
- relationship resources resolve to valid models where project information is available;
- relationship modifiers are compatible with their base relationship;
- relationship key overrides are structurally valid;
- arbitrary model methods are rejected;
- arbitrary statements inside relationship declarations are rejected.

# Compiler contract

A model such as:

```gnr
model User {
    table = 'users';
    primaryKey = 'id';

    fillable = [
        'name',
        'email'
    ];

    hidden = [
        'password'
    ];

    casts = {
        'active': 'bool'
    };

    posts() {
        return hasMany('posts');
    }
}
```

should pass through:

```text
source
  -> lexer
  -> parser
  -> ModelDeclaration AST
  -> model metadata parsing
  -> RelationshipDeclaration AST
  -> symbol resolution
  -> semantic validation
  -> validated model metadata
  -> ORM/native lowering
  -> C++23 generation
```

Before C++23 generation, the validated model representation should already know:

```text
model            User
table            users
connection       default
primaryKey       id
fillable         [name, email]
hidden           [password]
casts            active -> bool
relationship     posts -> hasMany -> Post
```

The transpiler must not rediscover model metadata or relationships by scanning raw source text.

# Design rule

The model contract is intentionally strict:

```text
model = database mapping metadata + relationship declarations
```

Nothing else belongs in a Gungnir model.

