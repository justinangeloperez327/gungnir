# ORM Relationships

Relationships describe how models are connected. Gungnir supports one-to-one, one-to-many, inverse, many-to-many, and through relationships with typed related models and eager loading.

## One to one

```gnr
model User {
    profile() {
        return hasOne<Profile>();
    }
}
```

## One to many

```gnr
model User {
    posts() {
        return hasMany<Post>();
    }
}
```

## Belongs to

```gnr
model Post {
    author() {
        return belongsTo<User>();
    }
}
```

## Many to many

```gnr
model User {
    roles() {
        return belongsToMany<Role>();
    }
}
```

Many-to-many relationships expose pivot-aware relationship operations for attaching and detaching related records.

## Through relationships

Gungnir provides `hasOneThrough` and `hasManyThrough` for relationships reached through an intermediate model.

## Key conventions

Relationship definitions use conventional foreign and local keys by default. Applications can provide explicit keys when their database schema does not follow those conventions.

## Eager loading

```gnr
const users = User::with("posts").get();
```

Eager loading batches parent keys and populates the loaded relationship state. A loaded empty relationship is distinct from a relationship that has not been loaded.

## Relationship access

Accessing a relationship that was not loaded may require a relationship query or explicit eager load, depending on the operation. Gungnir does not silently hide N+1 database access behind ordinary property access.

## Relationship queries

Relationships can be used as query scopes so filtering, ordering, pagination, and other query operations remain available for related records.
