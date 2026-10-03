# Gungnir Language Types

> **Development design specification.** This document describes intended long-term architecture beyond the frozen `development` contract. Examples and requirements may exceed the current implementation. See the [current implementation guide](../language-types.md) before using an API.


## Development alignment

Gungnir is under active development. No 1.0 compatibility contract is frozen; this document may describe intended future architecture, but current implementation guides and executable tests remain authoritative.

Gungnir is statically analyzed and compiled to C++23, but normal `.gnr` code uses a smaller application-oriented type system.

The language type system should describe application values directly without exposing C++ templates, pointers, references, allocators, ownership syntax, or ABI details.

# Design goals

The type system should be:

- predictable;
- statically analyzable;
- small enough for application developers to understand;
- expressive enough for HTTP, ORM, collections, validation, events, and services;
- independent from native C++ implementation details.

# Core scalar types

Canonical scalar types are:

| Gungnir type | Meaning |
| --- | --- |
| `bool` | Boolean |
| `int` | Normal signed application integer |
| `int64` | Explicit signed 64-bit integer |
| `uint64` | Explicit unsigned 64-bit integer |
| `float` | Single-precision floating point |
| `double` | Double-precision floating point |
| `decimal` | Decimal/application monetary numeric value |
| `string` | UTF-8 application text |
| `null` | Absence literal |
| `void` | No returned value |

Framework value types such as `datetime`, `Request`, `Response`, and model types are named types rather than primitive scalar keywords.

# Boolean

~~~gnr
const active = true;
const deleted = false;
~~~

Type:

~~~text
bool
~~~

Boolean conditions must use boolean-compatible expressions rather than arbitrary native C++ integer truthiness.

# Integers

~~~gnr
const count = 25;
~~~

The ordinary integer literal type is:

~~~text
int
~~~

Use explicit larger types in declarations or APIs when the distinction matters:

~~~gnr
function int64 totalRows() {
    // ...
}
~~~

# Floating point

~~~gnr
const ratio = 0.75;
~~~

Floating literals are inferred according to the language numeric literal rules.

`float` and `double` are intended for approximate numeric values.

# Decimal

Use `decimal` for application values that require decimal semantics, especially money:

~~~gnr
function decimal calculateTotal(decimal subtotal, decimal tax) {
    return subtotal + tax;
}
~~~

The generated C++ representation must use a defined decimal implementation rather than silently map exact decimal semantics to an imprecise binary floating-point type.

# String

Gungnir uses `string` for application text:

~~~gnr
const name = 'Justin';
const title = "Users";
~~~

Both single-quoted and double-quoted literals are **strings** in Gungnir.

This is deliberate.

Gungnir does not expose a separate application-level `char` type in normal source.

Therefore:

~~~gnr
'a'
~~~

has type:

~~~text
string
~~~

not a native C++ character type.

The C++ emitter is responsible for generating the appropriate native string representation.

# String quoting

Single quotes are preferred in most framework-facing examples:

~~~gnr
User::where('active', true);
view('users/index');
~~~

Double quotes remain valid when they improve readability or reduce escaping:

~~~gnr
const text = "It's ready";
~~~

The lexer must treat both forms as Gungnir string literals.

# Null

~~~gnr
const result = null;
~~~

`null` represents absence.

It is assignable to optional types, not arbitrary non-optional values.

# Optional types

Append `?` to a type:

~~~gnr
string?
User?
int?
~~~

Examples:

~~~gnr
function User? findUser(int id) {
    return User::find(id);
}
~~~

An optional value may contain:

~~~text
T
or
null
~~~

The native C++ representation may use `std::optional<T>`, nullable handles, or another safe representation depending on the underlying type.

That native representation is not part of the application language contract.

# Non-nullability by default

Types are non-nullable unless marked optional.

This should be rejected:

~~~gnr
const string name = null;
~~~

This is valid:

~~~gnr
const string? name = null;
~~~

Where inference is used, semantic analysis must still track nullability.

# Lists

Ordered application lists use:

~~~text
List<T>
~~~

Example:

~~~gnr
const ids = [
    1,
    2,
    3
];
~~~

Conceptual type:

~~~text
List<int>
~~~

Explicit type:

~~~gnr
function List<string> roles() {
    return [
        'admin',
        'editor'
    ];
}
~~~

A list is a general language value.

It is distinct from ORM `Collection<T>`.

# Collections

ORM/materialized framework collections use:

~~~text
Collection<T>
~~~

Example:

~~~gnr
const users = User::all();
~~~

Type:

~~~text
Collection<User>
~~~

Collection behavior is defined in `collection.md`.

# Maps

Key/value maps use:

~~~text
Map<K, V>
~~~

Example:

~~~gnr
function Map<string, string> labels() {
    return {
        'name': 'Name',
        'email': 'Email'
    };
}
~~~

Map keys and values must satisfy the language's key/value compatibility rules.

# Structured object values

Object literals may contain fields with different value types:

~~~gnr
const payload = {
    'name': user.name,
    'active': user.active,
    'count': 10
};
~~~

This is not forced into an invalid homogeneous `Map<string, T>`.

The compiler may represent it as an inferred structural object shape:

~~~text
{
  name: string,
  active: bool,
  count: int
}
~~~

Structural objects are useful for:

- JSON responses;
- view data;
- validation input;
- configuration;
- named structured arguments.

They are not a replacement for persistent models or declared framework types.

# Named application types

First-class declarations introduce named types.

Examples:

~~~text
model User          -> User
event UserRegistered -> UserRegistered
~~~

Framework declarations such as controllers, middleware, policies, listeners, notifications, and mail also create symbols, though not every declaration is intended to be passed around as ordinary application data.

# Model types

A model declaration introduces a model type:

~~~gnr
model User {
    // persistence metadata
}
~~~

Then:

~~~gnr
function User requireUser(int id) {
    return User::findOrFail(id);
}
~~~

Model instances are strongly associated with their declared model symbol.

# Framework types

Built-in framework types include concepts such as:

~~~text
Request
Response
Next
Collection<T>
Query<T>
datetime
UploadedFile
ValidationResult
Decision
~~~

The exact complete built-in set is maintained by the compiler/framework symbol catalog.

Application code should not need to spell native equivalents.

# Query types

ORM query construction produces a typed query:

~~~gnr
const query = User::where('active', true);
~~~

Conceptually:

~~~text
Query<User>
~~~

Then:

~~~gnr
const users = query.get();
~~~

produces:

~~~text
Collection<User>
~~~

The compiler should track this transition explicitly.

# Iterable and lazy values

Streaming ORM APIs such as cursor/lazy iteration must not pretend to be fully materialized collections.

They should resolve to a lazy iterable semantic type such as:

~~~text
Iterable<User>
~~~

or another finalized lazy type.

The exact public type name may be finalized with the async/iterator contract, but the semantic distinction is mandatory.

# Result/error values

Where an API uses an explicit result instead of framework exception/error propagation, the language may use:

~~~text
Result<T, E>
~~~

This should remain a normal typed value rather than expose C++ `std::expected` syntax directly.

The complete error model belongs in `errors.md`.

# Type inference

Local bindings normally infer their type:

~~~gnr
const user = User::findOrFail(id);
const count = users.count();
const active = true;
~~~

Conceptually:

~~~text
user   User
count  int
active bool
~~~

Developers should not need to repeat obvious local types.

# Explicit local types

An explicit local type may be used when useful:

~~~gnr
const int limit = 25;
const string title = 'Users';
~~~

The initializer must be assignable to the declared type.

# Function signatures

Function parameters and ordinary function return values are typed explicitly:

~~~gnr
function string fullName(string first, string last) {
    return first + ' ' + last;
}
~~~

Framework declarations may define specialized implicit return contracts.

Examples:

~~~text
controller public action -> Response
middleware handle        -> Response
policy action            -> AuthorizationDecision
listener handle          -> void/async completion
~~~

Those rules belong to each declaration's language contract.

# Numeric compatibility

Gungnir should allow safe widening conversions where the result is unambiguous.

Examples may include:

~~~text
int -> int64
int -> decimal
int -> double
float -> double
~~~

Implicit narrowing should not occur.

This should require an explicit conversion:

~~~text
int64 -> int
double -> float
decimal -> int
~~~

The semantic analyzer should diagnose lossy implicit conversions.

# Numeric operators

For:

~~~gnr
a + b
~~~

the compiler determines a common compatible numeric result type.

The exact widening table must be centralized in the type system rather than reimplemented by each lowering pass.

# Equality

Values may be compared when their types have defined equality semantics:

~~~gnr
user.id == owner.id
status == 'active'
count != 0
~~~

Native pointer identity must not leak into application equality accidentally.

# Model equality

If model equality is supported, it should use deterministic model identity semantics:

~~~text
model type
primary key
persistence identity
~~~

rather than generated C++ object addresses.

# Object member typing

For:

~~~gnr
user.name
~~~

the compiler resolves:

~~~text
receiver type  User
member         name
member type    resolved model/application attribute type
~~~

Unknown members should be diagnosed when the receiver type is statically known.

# Collection callback typing

For:

~~~gnr
const names = users.map((user) => {
    return user.name;
});
~~~

the compiler knows:

~~~text
users             Collection<User>
callback input    User
callback result   string
names             Collection<string>
~~~

# Function/callback types

Callbacks are statically typed through context.

Application code should not need to write C++ callable types such as:

~~~text
std::function<...>
function pointer syntax
lambda capture lists
~~~

The compiler represents callback parameter/result types directly.

# No native pointer syntax

Normal Gungnir source should not expose:

~~~text
T*
T&
T&&
new
delete
std::shared_ptr<T>
std::unique_ptr<T>
~~~

Ownership and borrowing remain generated/native implementation concerns unless Gungnir later introduces an explicit safe application abstraction.

# No C++ template syntax in application APIs

Normal source should prefer:

~~~gnr
posts() {
    return hasMany('posts');
}
~~~

not:

~~~text
hasMany<Post>()
~~~

Framework generic types such as `Collection<User>` are language type syntax, not raw C++ template invocation.

# No application-level char type

Because single-quoted literals are strings, Gungnir does not expose C++ `char` semantics as a normal application type.

Native interoperability may still map explicitly when required, but that is not the standard `.gnr` application experience.

# Type aliases

The first stable language contract should keep canonical type spellings small.

Prefer:

~~~text
bool
int
int64
uint64
float
double
decimal
string
~~~

The compiler may temporarily support legacy aliases such as `integer` or `boolean` during migration, but canonical documentation should use one spelling.

# Generic type syntax

Framework/language generic types use angle brackets:

~~~text
List<string>
Map<string, int>
Collection<User>
Query<User>
Result<User, Error>
~~~

User-defined generic types and generic functions are not required for the first stable application-language contract.

This keeps the semantic system smaller while still supporting the framework's typed containers.

# Union types

Arbitrary union syntax such as:

~~~text
string | int
~~~

is not part of the initial language contract.

Use explicit optional types for absence:

~~~text
string?
~~~

Broader sum/variant types may be introduced later only with clear pattern-matching and exhaustiveness semantics.

# Type casts/conversions

Unsafe C-style casts are not part of normal Gungnir syntax.

Conversions should use explicit safe conversion APIs when needed:

~~~gnr
const id = value.toInt();
const text = value.toString();
~~~

Exact conversion helpers belong to the relevant value-type APIs.

# Compiler type model

The semantic type system should be richer than the current minimal implementation.

Conceptually:

~~~text
Type
  Unknown
  Void
  Null
  Bool
  Int
  Int64
  UInt64
  Float
  Double
  Decimal
  String
  Optional<T>
  List<T>
  Map<K,V>
  ObjectShape
  Collection<T>
  Query<T>
  Iterable<T>
  Result<T,E>
  NamedType
  FunctionType
~~~

`Unknown` is a compiler recovery/internal state, not a user-facing type.

# Semantic validation

The type checker should validate at least:

- assignments are type compatible;
- null is only assigned to nullable/optional targets;
- function arguments match parameters;
- return values match function contracts;
- operators are valid for operand types;
- collection callback signatures are compatible;
- object/member access resolves;
- framework calls resolve their typed results;
- no implicit narrowing occurs;
- async values are awaited only where required;
- framework-specific implicit contracts are honored.

# C++23 lowering

Examples:

~~~text
string        -> native Gungnir string representation
T?            -> optional/nullable native representation
List<T>       -> native sequence
Map<K,V>      -> native map
Collection<T> -> Gungnir/native typed collection
Query<T>      -> ORM query builder representation
~~~

The exact C++23 type is an implementation detail.

The language contract is defined by Gungnir semantics, not by whichever standard-library type happens to implement it.

# Design rule

~~~text
Gungnir types describe application values.
C++ types implement them.
~~~

Do not force application developers to reason about native ownership, templates, pointer categories, or ABI details for normal framework code.

