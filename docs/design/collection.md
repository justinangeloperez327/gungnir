# Collections

> **Gungnir 1.x design specification.** This document describes intended long-term architecture beyond the frozen `1.0.0-rc.1` contract. Examples and requirements may exceed the current implementation. See the [current implementation guide](../collection.md) before using an API.


## 1.0 RC alignment

The shipped baseline is `1.0.0-rc.1`. The 1.0 contracts are frozen; this design document may describe additive 1.x evolution or future-major work, but current implementation guides and executable tests remain authoritative.

A Gungnir `Collection<T>` is an in-memory, typed sequence of values.

Collections are commonly returned by ORM operations:

```gnr
const users = User::all();
```

Conceptual type:

```text
Collection<User>
```

Collections provide expressive operations for inspecting, transforming, filtering, grouping, sorting, and aggregating values after data has already been loaded.

The collection API follows familiar Laravel-style naming where the behavior maps cleanly to Gungnir.

# Collection responsibility

A collection answers questions such as:

```text
How many values are present?
Is the collection empty?
What is the first or last value?
Which values match a condition?
How should values be transformed?
How should values be grouped or sorted?
Does the collection contain a value?
What aggregate can be calculated from the loaded values?
```

A collection does not represent a pending database query.

That distinction is important:

```gnr
const query = User::where('active', true);
```

is conceptually:

```text
Query<User>
```

while:

```gnr
const users = User::where('active', true).get();
```

is:

```text
Collection<User>
```

Collection methods operate in memory after query execution.

# Creating collections

ORM operations commonly create collections automatically:

```gnr
const users = User::all();
```

A collection may also be created explicitly:

```gnr
const numbers = collect([
    1,
    2,
    3,
    4
]);
```

Conceptual type:

```text
Collection<int>
```

Gungnir should infer the most specific compatible element type.

# Count

```gnr
const total = users.count();
```

`count()` returns the number of items currently in the collection.

This is different from:

```gnr
User::count();
```

which performs a database aggregate.

# Empty checks

```gnr
if (users.isEmpty()) {
    // ...
}
```

Inverse:

```gnr
if (users.isNotEmpty()) {
    // ...
}
```

# First item

```gnr
const user = users.first();
```

A predicate may be supplied:

```gnr
const admin = users.first((user) => {
    return user.role == 'admin';
});
```

If no item exists, `first()` returns the collection's optional/empty-result form.

# First or fail

Where absence should be exceptional:

```gnr
const user = users.firstOrFail();
```

A predicate may also be supported:

```gnr
const admin = users.firstOrFail((user) => {
    return user.role == 'admin';
});
```

# Last item

```gnr
const user = users.last();
```

With a predicate:

```gnr
const user = users.last((user) => {
    return user.active;
});
```

# Get by index

```gnr
const user = users.get(0);
```

A default may be supplied:

```gnr
const value = values.get(10, null);
```

Out-of-range access must not silently produce undefined native memory behavior.

# Contains

Check whether the collection contains a value:

```gnr
if (roles.contains('admin')) {
    // ...
}
```

For model/object collections, a predicate may be used:

```gnr
const hasAdmin = users.contains((user) => {
    return user.role == 'admin';
});
```

# Does not contain

```gnr
if (users.doesntContain((user) => {
    return user.active;
})) {
    // ...
}
```

# Each

Use `each()` for side-effect iteration:

```gnr
users.each((user) => {
    Logger::info(user.email);
});
```

`each()` should preserve the original collection rather than transform its element type.

# Map

Transform each value:

```gnr
const names = users.map((user) => {
    return user.name;
});
```

Conceptually:

```text
Collection<User>
    -> map(User -> string)
Collection<string>
```

The compiler should infer the result element type from the callback.

# Filter

Keep matching values:

```gnr
const activeUsers = users.filter((user) => {
    return user.active;
});
```

Conceptual type remains:

```text
Collection<User>
```

# Reject

Remove matching values:

```gnr
const enabledUsers = users.reject((user) => {
    return user.disabled;
});
```

# Reduce

Reduce a collection to one value:

```gnr
const total = orders.reduce(
    (sum, order) => {
        return sum + order.total;
    },
    0
);
```

The accumulator type must be semantically compatible with the initial value and callback result.

# Pluck

Retrieve one attribute from every item:

```gnr
const names = users.pluck('name');
```

Conceptually:

```text
Collection<User> -> Collection<string>
```

A keyed form may create a key/value collection or map representation:

```gnr
const names = users.pluck('name', 'id');
```

The exact keyed result type must be defined by the type system rather than remaining dynamically ambiguous.

# Only

For keyed/object-oriented collections where applicable:

```gnr
const selected = data.only([
    'name',
    'email'
]);
```

For sequential collections, `only()` should not be overloaded with unclear positional semantics.

# Except

For keyed/object-oriented collections:

```gnr
const safe = data.except([
    'password'
]);
```

Sequential `Collection<T>` behavior should remain distinct from map/object selection semantics.

# Values

Reindex or normalize the collection's sequential values:

```gnr
const values = users.values();
```

This is especially useful after filtering or keyed transformations.

# Keys

For keyed collection forms:

```gnr
const keys = users.keys();
```

The result is a collection of key values.

# Key by

Index items by an attribute:

```gnr
const usersById = users.keyBy('id');
```

Or by a callback:

```gnr
const usersByEmail = users.keyBy((user) => {
    return user.email;
});
```

Key collisions should use one documented deterministic rule. Gungnir should follow the familiar convention that the later item replaces the earlier item unless a stricter keyed collection contract is chosen.

# Group by

Group values:

```gnr
const usersByRole = users.groupBy('role');
```

Or:

```gnr
const usersByRole = users.groupBy((user) => {
    return user.role;
});
```

Conceptually:

```text
Collection<User>
    -> Map<string, Collection<User>>
```

The compiler should represent the result type explicitly.

# Sort

Sort scalar values:

```gnr
const sorted = numbers.sort();
```

A custom comparator may be supplied if the language callback contract supports it.

# Sort by

```gnr
const users = users.sortBy('name');
```

Callback form:

```gnr
const users = users.sortBy((user) => {
    return user.name;
});
```

# Sort descending

```gnr
const users = users.sortByDesc('created_at');
```

# Reverse

```gnr
const reversed = users.reverse();
```

# Unique

Remove duplicate values:

```gnr
const values = values.unique();
```

Unique by attribute:

```gnr
const users = users.unique('email');
```

Callback:

```gnr
const users = users.unique((user) => {
    return user.email;
});
```

Equality semantics must be defined by the value/type system.

For model collections, model identity should use deterministic model identity semantics rather than native pointer identity.

# Take

Take the first N items:

```gnr
const firstTen = users.take(10);
```

Negative-count behavior should be explicitly defined if supported. Gungnir should avoid obscure behavior unless it provides clear value.

# Skip

Skip the first N items:

```gnr
const remaining = users.skip(10);
```

# Slice

Retrieve a contiguous portion:

```gnr
const page = users.slice(20, 10);
```

Conceptually:

```text
start = 20
length = 10
```

# Chunk

Split an in-memory collection into fixed-size collections:

```gnr
const chunks = users.chunk(100);
```

Conceptual type:

```text
Collection<Collection<User>>
```

This is different from ORM/query `chunk()`, which repeatedly queries the database without loading the entire result set at once.

# Flatten

Flatten nested collections:

```gnr
const values = nested.flatten();
```

A depth may be supplied if supported:

```gnr
const values = nested.flatten(1);
```

Type inference must ensure the resulting element type is valid.

# Flat map

Transform and flatten one level:

```gnr
const tags = posts.flatMap((post) => {
    return post.tags;
});
```

# Merge

Combine collections:

```gnr
const combined = first.merge(second);
```

Element types must be compatible.

# Concat

Append values or another collection:

```gnr
const combined = users.concat(otherUsers);
```

`merge()` and `concat()` should have distinct documented keyed-collection behavior if keyed collections are supported.

# Push

Append an item:

```gnr
users.push(user);
```

# Prepend

```gnr
users.prepend(user);
```

Whether mutation-oriented methods return the same collection handle or a modified collection value must be consistent across the API.

For the application language, Gungnir should prefer predictable value semantics even if the native implementation uses optimized storage internally.

# Pop

Remove and return the last item:

```gnr
const user = users.pop();
```

Empty collection behavior must use a safe optional/empty-result form.

# Shift

Remove and return the first item:

```gnr
const user = users.shift();
```

# Sum

```gnr
const total = values.sum();
```

Attribute form:

```gnr
const total = orders.sum('total');
```

Callback form:

```gnr
const total = orders.sum((order) => {
    return order.total;
});
```

# Average

To remain consistent with the ORM convention, Gungnir uses `avg()`:

```gnr
const average = orders.avg('total');
```

Not:

```text
average()
```

# Min and max

```gnr
const minimum = values.min();
const maximum = values.max();
```

Attribute form:

```gnr
const cheapest = products.min('price');
const highest = products.max('price');
```

The exact result of attribute-based `min()` and `max()` must be explicit: either the scalar aggregate or the matching item. Gungnir should use scalar aggregate semantics for consistency with ORM aggregation.

# Median

If included:

```gnr
const median = values.median();
```

This is useful but not required for the first collection implementation.

Core collection support should prioritize the commonly used operations first.

# Join

Join string-compatible values:

```gnr
const text = names.join(', ');
```

Optional final separator behavior may be added later if it remains clear.

# Implode

To avoid duplicate concepts, Gungnir should prefer one canonical join operation.

I recommend:

```text
join()
```

as the Gungnir-facing API rather than exposing both Laravel's `implode()` terminology and `join()`.

This is one intentional place where clarity is preferable to copying every Laravel alias.

# Search

Find the index/key of a value:

```gnr
const index = names.search('Justin');
```

Predicate:

```gnr
const index = users.search((user) => {
    return user.email == email;
});
```

Absence must use a safe optional/result value rather than ambiguous integer sentinels when possible.

# Find

For keyed collections:

```gnr
const user = usersById.find(id);
```

For sequential collections, predicate-based `first()` is preferred over introducing overlapping `find()` semantics.

# Every

Check whether every item satisfies a predicate:

```gnr
const valid = users.every((user) => {
    return user.active;
});
```

# Some

Check whether at least one item satisfies a predicate:

```gnr
const hasAdmin = users.some((user) => {
    return user.role == 'admin';
});
```

Laravel often expresses this through `contains()`, but `some()` provides a clear general predicate operation. If both exist, their semantics must not conflict.

# Partition

Split into matching and non-matching collections:

```gnr
const [active, inactive] = users.partition((user) => {
    return user.active;
});
```

This requires destructuring support in the language. Until destructuring is part of the grammar, `partition()` may remain planned rather than core.

# Zip

Pair corresponding values:

```gnr
const pairs = names.zip(emails);
```

This is useful but not required for the first core implementation.

# Map with keys

For keyed transformations:

```gnr
const usersById = users.mapWithKeys((user) => {
    return {
        user.id: user
    };
});
```

The exact object-key expression syntax must align with the finalized object/map grammar before this API becomes stable.

# Where on loaded collections

Gungnir may support Laravel-style in-memory `where()`:

```gnr
const active = users.where('active', true);
```

However, this is an **in-memory collection filter**.

It is different from:

```gnr
User::where('active', true).get();
```

which filters in the database.

For large datasets, database filtering should be preferred.

# whereIn on loaded collections

```gnr
const selected = users.whereIn('id', ids);
```

Again, this operates on already-loaded values.

# Collection versus query operations

This distinction should be explicit in documentation and tooling.

Database query:

```gnr
const users = User::where('active', true)
    .orderBy('name')
    .get();
```

In-memory operations:

```gnr
const names = users
    .filter((user) => {
        return user.verified;
    })
    .sortBy('name')
    .pluck('name');
```

For performance, prefer pushing predicates, ordering, limits, and aggregates into the database when they can be expressed before `get()`.

Bad for a large table:

```gnr
const users = User::all()
    .filter((user) => {
        return user.active;
    });
```

Prefer:

```gnr
const users = User::where('active', true).get();
```

# Model collections

ORM model collections are still normal typed collections:

```text
Collection<User>
Collection<Post>
Collection<Order>
```

They may expose model-aware operations where useful, but the base collection contract should remain generic rather than creating a completely separate incompatible API.

# Relationship collections

Loaded to-many relationships return collections:

```gnr
const posts = user.posts;
```

Conceptual type:

```text
Collection<Post>
```

Relationship-query methods remain available through the relationship method itself:

```gnr
const published = user.posts()
    .where('published', true)
    .get();
```

This distinction mirrors the query-versus-loaded-collection distinction.

# Serialization

Collections containing serializable values may be returned as JSON:

```gnr
return json(users);
```

A collection may also support:

```gnr
const values = users.toArray();
const jsonText = users.toJson();
```

Model serialization rules such as `hidden` and `casts` must be honored for model elements.

# Conversion to array/list

```gnr
const values = users.toArray();
```

The returned representation should preserve element order.

# Conversion to JSON

```gnr
const jsonText = users.toJson();
```

Normal controllers should usually prefer:

```gnr
return json(users);
```

rather than manually producing JSON text.

# Immutability and mutation

Gungnir should make collection behavior predictable.

Transformation methods such as:

```text
map
filter
reject
sortBy
sortByDesc
unique
values
reverse
take
skip
slice
chunk
flatten
flatMap
```

should return collection values and should not unexpectedly mutate unrelated aliases.

Explicit mutation-like methods such as:

```text
push
prepend
pop
shift
```

must have clearly documented value/ownership behavior.

The language should prefer safe value semantics while allowing the C++23 implementation to optimize copies through move semantics and copy elision.

# Callback typing

Callbacks should be statically analyzable.

For:

```gnr
const names = users.map((user) => {
    return user.name;
});
```

the compiler should know:

```text
users             Collection<User>
callback input    User
callback result   string
names             Collection<string>
```

For:

```gnr
const active = users.filter((user) => {
    return user.active;
});
```

the callback result must be boolean-compatible.

# Collection type inference

Examples:

```gnr
const numbers = collect([1, 2, 3]);
```

```text
Collection<int>
```

```gnr
const names = users.pluck('name');
```

```text
Collection<string>
```

```gnr
const activeUsers = users.filter((user) => {
    return user.active;
});
```

```text
Collection<User>
```

The compiler should reject irreconcilably mixed element types unless the language type system has an explicit compatible union/common type.

# Lazy collections and cursors

ORM operations such as:

```gnr
User::cursor();
User::lazy();
User::lazyById();
```

should not pretend to be fully materialized `Collection<User>` values if they stream or incrementally load records.

They should use a separate lazy/iterable semantic type, for example:

```text
Iterable<User>
LazyCollection<User>
```

The exact name belongs to the finalized type-system specification.

This distinction prevents APIs such as `count()` or random indexing from accidentally forcing unexpected full materialization.

# Collection API reference

The intended core collection surface includes:

| API | Purpose |
| --- | --- |
| `count()` | Number of items |
| `isEmpty()` | Empty check |
| `isNotEmpty()` | Non-empty check |
| `first()` | First matching/item |
| `firstOrFail()` | First item or failure |
| `last()` | Last matching/item |
| `get()` | Safe index/key access |
| `contains()` | Contains value/predicate |
| `doesntContain()` | Inverse contains |
| `each()` | Iterate for side effects |
| `map()` | Transform items |
| `filter()` | Keep matching items |
| `reject()` | Remove matching items |
| `reduce()` | Fold to one value |
| `pluck()` | Extract attribute values |
| `values()` | Normalize sequential values |
| `keys()` | Retrieve keys |
| `keyBy()` | Key items by value |
| `groupBy()` | Group items |
| `sort()` | Sort scalar values |
| `sortBy()` | Sort by field/callback |
| `sortByDesc()` | Descending sort |
| `reverse()` | Reverse order |
| `unique()` | Remove duplicates |
| `take()` | Take first N |
| `skip()` | Skip first N |
| `slice()` | Slice a range |
| `chunk()` | Split in-memory collection |
| `flatten()` | Flatten nested collection |
| `flatMap()` | Map then flatten |
| `merge()` | Merge collections |
| `concat()` | Concatenate values |
| `push()` | Append item |
| `prepend()` | Prepend item |
| `pop()` | Remove last item |
| `shift()` | Remove first item |
| `sum()` | Sum values |
| `avg()` | Average values |
| `min()` | Minimum scalar |
| `max()` | Maximum scalar |
| `join()` | Join text values |
| `search()` | Find key/index |
| `every()` | All items satisfy predicate |
| `some()` | Any item satisfies predicate |
| `where()` | In-memory field filtering |
| `whereIn()` | In-memory set filtering |
| `toArray()` | Convert representation |
| `toJson()` | Serialize to JSON text |

Not every advanced method needs to ship in the first compiler milestone. The type and behavior contract should guide implementation priority.

# Core implementation priority

The first stable collection implementation should prioritize:

```text
count
isEmpty
isNotEmpty
first
last
contains
each
map
filter
reject
reduce
pluck
groupBy
keyBy
sortBy
sortByDesc
unique
take
skip
slice
sum
avg
min
max
toArray
toJson
```

Then add advanced operations after the base callback/type semantics are stable.

# What does not belong in Collection

Collections should not:

- issue hidden database queries;
- mutate database records implicitly;
- perform HTTP requests;
- act as service containers;
- perform authorization;
- hide expensive I/O behind ordinary in-memory methods.

Once a query has returned a `Collection<T>`, collection operations should be in-memory unless an API is explicitly documented otherwise.

# Collection AST and typing

Collection operations should use normal call-expression AST nodes resolved against collection types.

For:

```gnr
const names = users
    .filter((user) => {
        return user.active;
    })
    .map((user) => {
        return user.name;
    });
```

the semantic representation should resolve approximately as:

```text
users
  type Collection<User>

filter
  receiver Collection<User>
  callback User -> bool
  result Collection<User>

map
  receiver Collection<User>
  callback User -> string
  result Collection<string>

names
  type Collection<string>
```

# Semantic validation

The compiler should validate at least:

- collection receiver types are valid;
- callback parameter types are inferred correctly;
- filter/reject/every/some predicates are boolean-compatible;
- map result types are inferred;
- reduce accumulator types remain compatible;
- sort keys are comparable;
- sum/avg/min/max operate on compatible values;
- keyed operations use key-compatible values;
- merge/concat element types are compatible;
- model attribute references used by `pluck`, `sortBy`, `groupBy`, or similar operations are valid where model metadata is known;
- lazy iterable values are not incorrectly treated as fully materialized collections.

# Compiler contract

This source:

```gnr
const names = User::where('active', true)
    .get()
    .filter((user) => {
        return user.verified;
    })
    .sortBy('name')
    .pluck('name');
```

should conceptually pass through:

```text
source
  -> parser
  -> ORM call AST
  -> get() resolves Collection<User>
  -> collection method resolution
  -> callback typing
  -> result type inference
  -> validated AST
  -> collection lowering
  -> C++23 generation
```

Before native generation, the compiler should already know:

```text
get()        Collection<User>
filter()     Collection<User>
sortBy()     Collection<User>
pluck()      Collection<string>
names        Collection<string>
```

The transpiler must not infer collection behavior by scanning method names in raw source text.

# Generated C++ boundary

A Gungnir collection may lower to an optimized native C++23 representation using facilities such as:

```text
std::vector
ranges
iterators
move semantics
copy elision
typed lambdas
generated model metadata
```

Those details are not part of the application-facing contract.

Application code should not need to write template-heavy collection types or range adaptor plumbing for ordinary framework operations.

# Naming convention

Public collection APIs use camelCase where multiple words are required:

```text
isEmpty
isNotEmpty
firstOrFail
doesntContain
keyBy
groupBy
sortBy
sortByDesc
flatMap
whereIn
toArray
toJson
mapWithKeys
```

Simple methods remain single-word:

```text
count
first
last
get
contains
each
map
filter
reject
reduce
pluck
values
keys
sort
reverse
unique
take
skip
slice
chunk
flatten
merge
concat
push
prepend
pop
shift
sum
avg
min
max
join
search
every
some
where
```

# Design rule

The collection contract is intentionally focused:

```text
Collection<T> = typed, materialized, in-memory sequence operations
```

The ORM builds and executes database queries.

Collections operate on values that have already been loaded.

The compiler tracks element and callback types.

The generated C++23 layer implements those operations efficiently without exposing template-heavy machinery to normal Gungnir application code.

