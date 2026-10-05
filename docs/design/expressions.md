# Expressions

> **Future design specification.** This document describes intended long-term architecture beyond the published `1.0` contract. Examples and requirements may exceed the current implementation. See the [current implementation guide](../expressions.md) before using an API.


## Release alignment

This document describes possible future architecture beyond 1.0. Current implementation guides and executable tests define the published contract.

A Gungnir expression produces a value.

Expressions are used in bindings, function arguments, conditions, returns, collection callbacks, ORM calls, response helpers, and framework declarations.

The parser must represent expressions structurally. The transpiler must not recover expression meaning by rescanning raw source.

# Literal expressions

## Null

~~~gnr
null
~~~

## Boolean

~~~gnr
true
false
~~~

## Integer

~~~gnr
0
25
1000
~~~

## Decimal/floating value

~~~gnr
10.5
0.75
~~~

The type system determines the appropriate numeric type according to context and literal rules.

## String

Both quote styles produce Gungnir strings:

~~~gnr
'users'
"users"
~~~

Single quotes do not mean native C++ characters.

# Name expressions

A name refers to a resolved local, parameter, framework symbol, type, function, or other visible declaration:

~~~gnr
user
request
User
Auth
~~~

Name resolution belongs to semantic analysis.

# Member access

Instance member access uses dot syntax:

~~~gnr
user.name
request.input
order.customer.email
~~~

The compiler resolves the receiver type before validating the member.

# Static/framework access

Type or framework static access uses `::`:

~~~gnr
User::all()
Auth::user()
Route::get(...)
Mail::to(...)
~~~

This is Gungnir framework/type syntax. It may lower to different native C++ code.

# Function calls

~~~gnr
calculateTotal(subtotal, tax)
~~~

Arguments are evaluated according to normal call semantics.

# Method calls

~~~gnr
users.count()
request.input('name')
user.save()
~~~

Chaining is supported:

~~~gnr
User::where('active', true)
    .orderBy('name')
    .limit(25)
    .get();
~~~

Each call must have a semantically resolved receiver/result type.

# Named arguments

Named arguments make configuration-heavy calls clearer:

~~~gnr
Auth::attempt(
    credentials,
    remember: true
);
~~~

Relationship override example:

~~~gnr
hasMany(
    'posts',
    foreignKey: 'author_id',
    localKey: 'uuid'
)
~~~

Rules:

- positional arguments come before named arguments;
- a named argument may appear only once;
- names must match known parameter names where the callable is statically known;
- omitted parameters must have defaults or be optional according to the callable contract.

# Grouping

Parentheses control evaluation:

~~~gnr
(a + b) * c
~~~

# List literals

~~~gnr
[
    'admin',
    'editor',
    'user'
]
~~~

The compiler infers a compatible element type.

Conceptually:

~~~text
List<string>
~~~

# Object literals

~~~gnr
{
    'name': user.name,
    'email': user.email,
    'active': true
}
~~~

Object literals are structured values.

They may contain heterogeneous field types.

They are commonly used for:

- JSON responses;
- view data;
- validation rules;
- ORM creation/update data;
- configuration.

# Object keys

Canonical framework examples use string keys:

~~~gnr
{
    'name': 'Justin'
}
~~~

Identifier-style keys may be supported by the finalized object grammar, but string keys remain the unambiguous canonical form.

# Subscript/index expressions

~~~gnr
values[0]
data['name']
~~~

The receiver must support the requested indexing operation.

Out-of-range or missing-key behavior must be safe and type-defined rather than native undefined behavior.

# Unary expressions

Supported core unary operators:

~~~text
!value
-value
+value
~~~

Examples:

~~~gnr
!Auth::check()
-total
+offset
~~~

`!` requires a boolean-compatible operand.

Numeric unary operators require numeric operands.

# Arithmetic expressions

Core arithmetic:

~~~text
*
/
%
+
-
~~~

Examples:

~~~gnr
subtotal + tax
quantity * price
index % 2
~~~

Operand/result compatibility is defined by the type system.

# String concatenation

Strings use `+` for explicit concatenation:

~~~gnr
const fullName = first + ' ' + last;
~~~

The language should not silently stringify arbitrary unrelated values in concatenation.

Use explicit conversion or formatting helpers where required.

# Comparison expressions

~~~text
<
<=
>
>=
~~~

Examples:

~~~gnr
age >= 18
total > 0
~~~

Operands must be comparable.

# Equality expressions

~~~text
==
!=
~~~

Examples:

~~~gnr
status == 'active'
user.id != owner.id
value == null
~~~

Equality semantics are type-defined.

# Logical expressions

~~~text
&&
||
~~~

Examples:

~~~gnr
user.active && user.verified
isAdmin || isOwner
~~~

Operands must be boolean-compatible.

Logical evaluation should short-circuit.

# Operator precedence

From highest to lowest among the initial core operators:

| Precedence | Operators |
| --- | --- |
| 1 | grouping `()`, member `.`, static `::`, call `()`, subscript `[]` |
| 2 | unary `!`, unary `+`, unary `-` |
| 3 | `*`, `/`, `%` |
| 4 | `+`, `-` |
| 5 | `<`, `<=`, `>`, `>=` |
| 6 | `==`, `!=` |
| 7 | `&&` |
| 8 | `||` |

Assignment is a statement-level operation in the initial contract rather than a general value-producing expression.

# Null comparison

Optional values may be checked explicitly:

~~~gnr
if (user == null) {
    // ...
}
~~~

The semantic analyzer must know whether the operand may actually be null.

# Conditional expression

A ternary/conditional expression is not required for the first stable grammar.

Prefer an explicit `if` statement until conditional-expression semantics are intentionally finalized.

This avoids adding syntax merely because C++ has it.

# Null coalescing

A `??` operator may be added later:

~~~text
value ?? fallback
~~~

but it should not be considered stable until optional types and short-circuit semantics are fully implemented.

# Lambda expressions

Callbacks use concise lambda syntax:

~~~gnr
(user) => {
    return user.name;
}
~~~

Example:

~~~gnr
const names = users.map((user) => {
    return user.name;
});
~~~

The callback parameter type is inferred from context where possible.

# Multiple lambda parameters

~~~gnr
(sum, order) => {
    return sum + order.total;
}
~~~

Used by operations such as `reduce()`.

# Typed lambda parameters

Where context does not supply enough information, typed lambda parameters may be allowed:

~~~gnr
(User user) => {
    return user.name;
}
~~~

The exact need for explicit lambda return types should remain minimal.

# Lambda captures

Gungnir does not expose C++ capture lists such as:

~~~text
[&]
[=]
[this]
~~~

Visible lexical bindings are captured according to Gungnir closure semantics.

The compiler/runtime owns the safe native capture representation.

# Callback lifetime

A callback that escapes its current scope must not retain invalid request-scoped/native references.

This is a semantic/lifetime responsibility of the compiler/runtime, especially for async and queued work.

# Await expression

Inside an async context:

~~~gnr
const result = await service.fetch();
~~~

`await` is a unary async expression with special semantic rules.

It is valid only where the enclosing declaration permits async suspension.

The full contract belongs in `async.md`.

# Framework expressions

Framework APIs remain ordinary typed call expressions after parsing.

For example:

~~~gnr
User::findOrFail(id)
~~~

should resolve approximately as:

~~~text
StaticCallExpression
  receiver = ModelSymbol(User)
  method = findOrFail
  arguments = [id]
  result = User
~~~

Likewise:

~~~gnr
json(user, 201)
~~~

resolves to a Response-compatible value.

# Relationship expressions

Inside model relationship declarations:

~~~gnr
hasMany('posts')
~~~

is parsed as a call expression but receives special model-relationship semantic validation.

The transpiler must not identify it by raw string matching.

# Chained expressions

~~~gnr
User::with('posts')
    .where('active', true)
    .orderBy('name')
    .get()
    .pluck('name')
~~~

The compiler should type every stage:

~~~text
User::with(...)          Query<User>
.where(...)              Query<User>
.orderBy(...)            Query<User>
.get()                   Collection<User>
.pluck('name')           Collection<string>
~~~

# Expression statement boundary

An expression may appear as a statement only when discarding its result is meaningful:

~~~gnr
user.save();
logger.info('saved');
~~~

Pure expressions whose result is discarded may produce a lint/tooling diagnostic.

# No implicit assignment expressions

Avoid C/C++ patterns such as:

~~~text
if ((x = find()) != null)
~~~

Assignments should remain statements.

This makes control flow and data mutation easier to analyze.

# No comma operator

Gungnir does not expose C++'s comma operator as an application-language expression.

# No pointer/address expressions

Normal `.gnr` source does not expose:

~~~text
&value
*pointer
new
delete
sizeof
typeid
reinterpret_cast
~~~

Native interoperability belongs to explicit lower-level boundaries.

# Expression AST

The target expression AST should contain dedicated nodes such as:

~~~text
NameExpression
LiteralExpression
MemberExpression
StaticMemberExpression
CallExpression
NamedArgument
SubscriptExpression
GroupExpression
UnaryExpression
BinaryExpression
ListExpression
ObjectExpression
LambdaExpression
AwaitExpression
~~~

Framework meaning should be attached during semantic resolution rather than encoded as ad-hoc text.

# Source spans

Every expression node should retain its source span.

Diagnostics should point to the actual operator, call, argument, or member that caused the error.

# Semantic validation

The compiler should validate:

- names resolve;
- member access is valid for the receiver;
- calls resolve to callable symbols;
- argument counts and names match;
- operator operands are compatible;
- object/list element typing is valid;
- subscripts are valid;
- lambda parameter/result types are compatible with context;
- `await` appears only in async contexts;
- framework expressions satisfy their specialized contracts;
- chained result types remain valid at each stage.

# Lowering

Only a validated expression tree should reach framework/C++ lowering.

Example:

~~~gnr
User::where('active', true)
    .orderBy('name')
    .get()
~~~

must not be lowered by searching source text for `.where(` or `.orderBy(`.

The validated AST already identifies each call and its result type.

# Design rule

~~~text
parse syntax once
resolve meaning once
lower validated expressions
~~~

Expressions are language structures, not strings for the transpiler to reinterpret.

