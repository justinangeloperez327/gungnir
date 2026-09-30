# Models

Gungnir models provide the application-facing interface to persisted data.

The normal application syntax is written in `.gnr`. Model declarations are convention-first and intentionally hide C++ inheritance, CRTP, field wrappers, template-heavy relationship types, and generated ORM metadata.

This document defines the intended Gungnir model language contract. Compiler support may be implemented progressively, but application-facing syntax should converge on this specification rather than exposing lower-level C++ mechanics.

## Defining a model

A model is declared with the `model` keyword:

```gnr
model User {
    string name;
    string email;
}
```

Gungnir infers the conventional table name and primary key.

For `User`, the defaults are:

```text
model       User
table       users
primary key id
```

The compiler generates the native C++23 model plumbing required by the ORM.

## Fields

Fields are declared using a type followed by a field name:

```gnr
model User {
    string name;
    string email;
    bool active = true;
}
```

Common scalar types include:

```text
string
bool
int
int64
uint64
float
double
decimal
```

## Nullable fields

A nullable field uses `?`:

```gnr
model User {
    string name;
    string? nickname;
}
```

Gungnir maps nullable values to the appropriate native representation during C++23 generation.

## Default values

Fields may define defaults:

```gnr
model User {
    bool active = true;
    string status = "pending";
}
```

## Model configuration

Gungnir uses conventions by default. Explicit configuration is only needed when an application differs from those conventions.

```gnr
model AuditUser {
    table = "legacy_users";
    connection = "reporting";
    timestamps = false;
    softDeletes = true;

    string name;
}
```

The standard model configuration properties are:

| Property | Purpose |
| --- | --- |
| `table` | Override the inferred table name |
| `connection` | Use a specific database connection |
| `timestamps` | Enable or disable managed timestamps |
| `softDeletes` | Enable soft deletion |

## Relationships

Relationships are ordinary model methods.

Gungnir favors resource names instead of C++ model types in application source:

```gnr
posts() {
    return hasMany('posts');
}
```

The compiler resolves the resource name to its related model and applies conventional table and key rules.

For example:

```gnr
model User {
    posts() {
        return hasMany('posts');
    }
}
```

is understood as a relationship from `User` to `Post`, normally using:

```text
related model   Post
related table   posts
foreign key     posts.user_id
local key       users.id
result          Collection<Post>
```

### hasOne

Use `hasOne` when a model owns one related record.

```gnr
model User {
    profile() {
        return hasOne('profile');
    }
}
```

Convention:

```text
profiles.user_id -> users.id
```

### hasMany

Use `hasMany` when a model owns multiple related records.

```gnr
model User {
    posts() {
        return hasMany('posts');
    }
}
```

Convention:

```text
posts.user_id -> users.id
```

The relationship resolves to a collection of `Post` models.

### belongsTo

Use `belongsTo` when the current model references its parent.

```gnr
model Post {
    user() {
        return belongsTo('user');
    }
}
```

Convention:

```text
posts.user_id -> users.id
```

### belongsToMany

Use `belongsToMany` for a many-to-many relationship.

```gnr
model User {
    roles() {
        return belongsToMany('roles');
    }
}
```

The inverse relationship may use the same declaration:

```gnr
model Role {
    users() {
        return belongsToMany('users');
    }
}
```

By convention Gungnir infers a pivot table from the two related models.

For `User` and `Role`:

```text
pivot table role_user
user_id
role_id
```

### hasOneThrough

Use `hasOneThrough` to access one related model through an intermediate model.

Example relationship:

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

### hasManyThrough

Use `hasManyThrough` to access multiple related records through an intermediate model.

Example relationship:

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

The relationship resolves to `Collection<Post>`.

## Polymorphic relationships

Polymorphic relationships allow one related table to belong to more than one model type.

These relationships are part of the planned model language contract but may be implemented after the six core relationship types.

### morphOne

Use `morphOne` when multiple model types may each own one record of the same related type.

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

A conventional polymorphic table can contain:

```text
imageable_id
imageable_type
```

### morphMany

Use `morphMany` when multiple model types may each own many records of the same related type.

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

The `comments` table can contain:

```text
commentable_id
commentable_type
```

### morphTo

Use `morphTo` on the inverse side of a polymorphic one-to-one or one-to-many relationship.

```gnr
model Comment {
    commentable() {
        return morphTo('commentable');
    }
}
```

A comment can therefore resolve its parent as a `Post`, `Video`, or another supported model type according to its polymorphic metadata.

### morphToMany

Use `morphToMany` for the owning side of a polymorphic many-to-many relationship.

```gnr
model Post {
    tags() {
        return morphToMany('tags', 'taggable');
    }
}

model Video {
    tags() {
        return morphToMany('tags', 'taggable');
    }
}
```

A conventional pivot table may contain:

```text
taggables
    tag_id
    taggable_id
    taggable_type
```

### morphedByMany

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

## Relationship reference

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

## Relationship modifiers

Relationship modifiers refine an existing relationship rather than defining a separate relationship type.

### latestOfMany

```gnr
latestOrder() {
    return hasOne('orders').latestOfMany();
}
```

### oldestOfMany

```gnr
oldestOrder() {
    return hasOne('orders').oldestOfMany();
}
```

### ofMany

```gnr
largestOrder() {
    return hasOne('orders').ofMany('price', 'max');
}
```

## Overriding relationship conventions

Normal application code should rely on convention. Explicit key arguments are available when a database schema does not follow Gungnir conventions.

### Custom foreign key

```gnr
posts() {
    return hasMany(
        'posts',
        foreignKey: 'author_id'
    );
}
```

### Custom local key

```gnr
posts() {
    return hasMany(
        'posts',
        foreignKey: 'author_id',
        localKey: 'uuid'
    );
}
```

## Relationship naming rules

Relationship methods should describe the application concept rather than database implementation details.

Use singular names for relationships returning one model:

```gnr
profile() {
    return hasOne('profile');
}

user() {
    return belongsTo('user');
}
```

Use plural names for relationships returning collections:

```gnr
posts() {
    return hasMany('posts');
}

roles() {
    return belongsToMany('roles');
}
```

## Convention-based model resolution

Resource strings are resolved through Gungnir model conventions.

For example:

```gnr
hasMany('posts')
```

is resolved conceptually as:

```text
posts
  -> singularize
post
  -> model name
Post
  -> model lookup
model Post
```

Application code therefore does not need syntax such as:

```text
hasMany<Post>()
Post::class
```

Those are implementation details of the generated/native layer.

## Eager loading

Relationships are designed to participate in ORM eager loading so applications can avoid N+1 query patterns.

A query may request relationships before execution:

```gnr
const users = User::with('posts').get();
```

Nested relationships may use a dotted path:

```gnr
const users = User::with('posts.comments').get();
```

The ORM should batch relationship queries where possible rather than issuing one query for every parent record.

## Querying models

Models expose concise query entry points:

```gnr
const users = User::all();

const user = User::find(id);

const activeUsers = User::where('active', true)
    .orderBy('name')
    .get();
```

Query syntax remains application-oriented. The compiler may lower expressive Gungnir method names to differently named native C++ runtime operations.

## Creating models

Model creation should accept object-style data:

```gnr
const user = User::create({
    'name': 'Justin',
    'email': 'justin@example.com'
});
```

Framework-managed fields such as timestamps should not need to be supplied manually.

## Updating models

Loaded models can be changed and saved through the model interface:

```gnr
user.name = 'Updated Name';
user.save();
```

## Deleting models

```gnr
user.delete();
```

When `softDeletes = true`, deletion should use the model's soft-delete behavior instead of physically removing the record.

## Model conventions

Gungnir models should follow these principles:

- Prefer convention over explicit configuration.
- Keep application syntax free from C++ template and inheritance plumbing.
- Infer table names, foreign keys, pivot tables, local keys, and relationship result types when unambiguous.
- Keep relationship declarations readable without requiring model class literals.
- Resolve relationship resources during semantic analysis rather than treating them as unchecked strings at code-generation time.
- Detect invalid relationship targets and incompatible key configuration before C++23 emission.
- Generate ordinary, inspectable C++23.
- Preserve native C++ interoperability without making native C++ syntax the default application experience.

## Compiler contract

A relationship such as:

```gnr
model User {
    posts() {
        return hasMany('posts');
    }
}
```

should ultimately pass through the language pipeline as structured model syntax:

```text
source
  -> lexer
  -> parser
  -> model/relationship AST
  -> symbol resolution
  -> semantic validation
  -> typed/validated AST
  -> ORM lowering
  -> C++23 generation
```

The transpiler should not rediscover relationships by scanning raw source text after parsing.

The validated representation should already know that:

```text
owner model       User
relationship      posts
relationship kind hasMany
related resource  posts
related model     Post
foreign key       user_id
local key         id
result type       Collection<Post>
```

before the C++23 backend begins emitting code.
