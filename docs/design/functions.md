# Functions

> **Design specification.** This document preserves the intended contract. Examples and requirements below may exceed the current implementation. See the [current implementation guide](../functions.md) before using an API.

Gungnir functions are typed callable units of application logic.

The language distinguishes ordinary functions from framework-specific actions such as controller actions, middleware handlers, policy abilities, listeners, notifications, and mail composition methods.

Ordinary functions have explicit parameter and return contracts.

Framework declarations may supply specialized implicit return contracts.

# Basic function

A normal function uses the `function` keyword:

~~~gnr
function string fullName(string first, string last) {
    return first + ' ' + last;
}
~~~

General form:

~~~text
function ReturnType name(parameters) {
    statements
}
~~~

# Void function

A function that does not return a value uses `void`:

~~~gnr
function void logUser(User user) {
    Logger::info(user.email);
}
~~~

An explicit empty return is valid:

~~~gnr
return;
~~~

# Parameters

Parameters use:

~~~text
Type name
~~~

Example:

~~~gnr
function decimal calculateTotal(
    decimal subtotal,
    decimal tax
) {
    return subtotal + tax;
}
~~~

Parameter types are explicit in ordinary named functions.

# Multiple parameters

~~~gnr
function bool canAccess(
    User user,
    Project project
) {
    return user.id == project.owner_id;
}
~~~

# Optional parameters

Optional types use `?`:

~~~gnr
function string displayName(
    string name,
    string? suffix
) {
    if (suffix == null) {
        return name;
    }

    return name + ' ' + suffix;
}
~~~

# Default parameters

~~~gnr
function int pageSize(int size = 25) {
    return size;
}
~~~

Rules:

- required parameters come before default parameters;
- default expressions must be valid in the declaration context;
- omitted arguments use the declared default.

# Named arguments

Callers may use named arguments:

~~~gnr
const total = calculatePrice(
    subtotal: subtotal,
    tax: tax
);
~~~

Positional arguments come before named arguments.

Named arguments must match declared parameter names.

# Return types

Ordinary named functions declare their result:

~~~gnr
function User requireUser(int id) {
    return User::findOrFail(id);
}
~~~

The compiler validates every return expression against `User`.

# Return type inference

The first stable contract should not require return-type inference for named application functions.

Explicit result types improve:

- diagnostics;
- module indexing;
- tooling;
- recursion;
- separate compilation;
- generated C++ signatures.

Local lambdas may infer their result type from their body and call context.

# Async functions

An ordinary asynchronous function uses:

~~~gnr
async function User loadUser(int id) {
    return await repository.find(id);
}
~~~

The declared result remains the logical application result:

~~~text
User
~~~

not a native coroutine wrapper.

The generated C++ layer may use a type such as:

~~~text
Task<User>
~~~

That native type is an implementation detail.

# Await

`await` is valid only in async functions or framework contexts that explicitly permit async suspension:

~~~gnr
async function User loadUser(int id) {
    const user = await repository.find(id);

    return user;
}
~~~

# Framework actions

Framework declarations define their own callable syntax and result contracts.

## Controller

~~~gnr
public show(User user) {
    return json(user);
}
~~~

Implicit result:

~~~text
Response
~~~

## Middleware

~~~gnr
public handle(Request request, Next next) {
    return next(request);
}
~~~

Implicit result:

~~~text
Response
~~~

## Policy

~~~gnr
public update(User user, Post post) {
    return user.id == post.user_id;
}
~~~

Implicit result:

~~~text
AuthorizationDecision
~~~

## Listener

~~~gnr
public handle(UserRegistered event) {
    logger.info(event.user.email);
}
~~~

Conceptual result:

~~~text
void completion
~~~

Notification and mail methods use the specialized contracts defined in their own documentation.

# Functions versus methods

An ordinary function:

~~~gnr
function string normalizeEmail(string email) {
    // ...
}
~~~

is different from an instance method:

~~~gnr
users.count()
~~~

and from a framework/static call:

~~~gnr
User::all()
~~~

The semantic analyzer resolves each callable category explicitly.

# Lambdas

Anonymous callbacks use lambda syntax:

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

Lambda parameter types are inferred from context when possible.

# Typed lambda parameters

Where contextual inference is insufficient:

~~~gnr
(User user) => {
    return user.name;
}
~~~

# Lambda result inference

Given:

~~~gnr
const names = users.map((user) => {
    return user.name;
});
~~~

and:

~~~text
users       Collection<User>
user.name   string
~~~

the lambda type is conceptually:

~~~text
(User) -> string
~~~

# Multi-parameter lambdas

~~~gnr
const total = orders.reduce(
    (sum, order) => {
        return sum + order.total;
    },
    0
);
~~~

The callback parameter types are derived from the receiving API and initial accumulator.

# No C++ capture lists

Gungnir does not expose:

~~~text
[&](...)
[=](...)
[this](...)
~~~

Closures capture visible lexical values according to Gungnir semantics.

The generated C++ layer chooses a safe native capture representation.

# Lexical capture

~~~gnr
const minimum = 100;

const largeOrders = orders.filter((order) => {
    return order.total >= minimum;
});
~~~

The callback may read `minimum` from its lexical scope.

Mutation of captured mutable values should be allowed only when the closure/lifetime model can preserve predictable semantics.

# Recursive functions

Named functions may call themselves:

~~~gnr
function int factorial(int value) {
    if (value <= 1) {
        return 1;
    }

    return value * factorial(value - 1);
}
~~~

Explicit return types make recursion straightforward to type-check.

# Function overloading

The first stable Gungnir application language should avoid user-defined overload sets with the same function name.

Prefer distinct names, optional parameters, or default parameters.

This gives:

- simpler name resolution;
- clearer diagnostics;
- less C++ overload-resolution leakage;
- easier language-server behavior;
- easier code generation.

Native C++ interoperability may expose overloads through explicit binding metadata.

# Generic functions

User-defined generic functions are not required for the first stable language contract.

Do not expose raw C++ template syntax such as:

~~~text
template<typename T>
T first(...)
~~~

Framework-provided generic types remain available:

~~~text
Collection<User>
List<string>
Map<string, int>
~~~

User-defined generics may be introduced later through a Gungnir-native type-parameter design.

# Variadic functions

C-style variadic functions are not part of the initial language.

Prefer typed list or collection parameters:

~~~gnr
function string joinNames(List<string> names) {
    return names.join(', ');
}
~~~

# Parameter passing

Gungnir source does not expose native parameter syntax such as:

~~~text
const T&
T&&
T*
~~~

Parameters use application types:

~~~gnr
function string name(User user) {
    return user.name;
}
~~~

The compiler/runtime chooses safe and efficient native passing behavior.

# Parameter bindings

Function parameters are local immutable bindings by default.

Instead of reassigning a parameter:

~~~text
input = normalized;
~~~

create a mutable local:

~~~gnr
function string normalize(string input) {
    let value = input;

    // update value

    return value;
}
~~~

This keeps parameter intent stable.

# Passing models

~~~gnr
function void deactivate(User user) {
    user.active = false;
    user.save();
}
~~~

Whether model values internally use handles, references, or optimized wrappers is a runtime concern.

Application code sees the model type.

# Function scope

Function parameters and local bindings are scoped to the function body.

Nested blocks introduce nested lexical scopes.

# Function visibility and modules

Module/export visibility belongs in `modules.md`.

This document defines callable behavior, not project/module packaging.

Until modules are finalized, ordinary top-level functions should be treated as application/module functions rather than raw process-global C++ symbols.

# Function names

Gungnir application code uses camelCase:

~~~text
fullName
calculateTotal
normalizeEmail
sendReceipt
~~~

# Framework-reserved method meaning

Names such as:

~~~text
handle
via
subject
content
~~~

have special contracts only inside the framework declarations that define those contracts.

Outside those contexts they are ordinary identifiers unless reserved by the grammar.

# First-class function values

Lambdas are first-class callback values.

Whether named functions themselves can be assigned directly as values:

~~~text
const callback = normalizeEmail;
~~~

is not required for the first framework milestone and should be finalized with the module/function-value contract.

# Function types

Semantic analysis should represent callable types directly:

~~~text
FunctionType
  parameters[]
  result
  async
~~~

Example:

~~~text
(string, string) -> string
~~~

Async logical type:

~~~text
async (int) -> User
~~~

The native coroutine wrapper is not part of the logical return type.

# Error propagation

The function contract must integrate with the future `errors.md` design.

Ordinary functions may propagate framework errors according to the Gungnir error model.

Application developers should not need native C++ exception specifications.

# Native interoperability

Native C++ functions may be exposed through explicit bindings.

The Gungnir symbol/type layer should describe their application-facing signature.

Normal application code should not need C++ overload resolution, templates, references, calling conventions, or exception specifications to call a bound function.

# Function AST

Named function:

~~~text
FunctionDeclaration
  name
  async
  parameters[]
  returnType
  body
  sourceSpan
~~~

Parameter:

~~~text
FunctionParameter
  name
  type
  defaultValue?
~~~

Lambda:

~~~text
LambdaExpression
  parameters[]
  body
  inferredReturnType
  captures[]
~~~

Framework actions may use specialized AST nodes when their semantics differ materially from ordinary functions.

# Semantic validation

The compiler should validate:

- function names resolve uniquely in scope/module;
- parameter names are unique;
- parameter types resolve;
- default arguments are type compatible;
- required/default parameter ordering is valid;
- call argument counts are valid;
- named arguments resolve;
- return values match the declared result;
- non-void functions return on every reachable path;
- void functions do not return incompatible values;
- async functions use await legally;
- lambdas satisfy contextual callback types;
- captures obey lifetime rules;
- recursive calls resolve;
- user-defined overload ambiguity is avoided.

# Compiler contract

This function:

~~~gnr
function string fullName(
    string first,
    string last
) {
    return first + ' ' + last;
}
~~~

should pass through:

~~~text
source
  -> lexer
  -> parser
  -> FunctionDeclaration AST
  -> parameter/type resolution
  -> body expression/statement analysis
  -> return/control-flow validation
  -> validated function AST
  -> C++23 lowering
  -> native function
~~~

Async example:

~~~gnr
async function User loadUser(int id) {
    return await repository.find(id);
}
~~~

should preserve:

~~~text
logical result type   User
async                  true
native result          coroutine-backed User
~~~

before code generation.

# Generated C++ boundary

Gungnir:

~~~gnr
function string fullName(string first, string last) {
    return first + ' ' + last;
}
~~~

may lower to an ordinary C++23 function using optimized native string, reference, and move semantics.

Those implementation details do not change the Gungnir function contract.

# Design rule

~~~text
functions declare application inputs and logical outputs
framework actions specialize that contract
C++23 implements it
~~~

Do not expose C++ calling-convention, ownership, template, or coroutine-wrapper syntax in normal Gungnir function declarations.

