# Statements

> **Development design specification.** This document describes intended long-term architecture beyond the frozen `development` contract. Examples and requirements may exceed the current implementation. See the [current implementation guide](../statements.md) before using an API.


## Development alignment

Gungnir is under active development. No 1.0 compatibility contract is frozen; this document may describe intended future architecture, but current implementation guides and executable tests remain authoritative.

A Gungnir statement performs an action.

Statements define local bindings, mutate values, control execution, call functions, and return from functions or framework actions.

Statements are parsed structurally and validated against lexical scope and type rules before lowering to C++23.

# Statement categories

The initial statement contract includes:

~~~text
block
const binding
let binding
assignment
expression statement
if / else
for
while
return
break
continue
~~~

Async statements may contain `await` expressions where the enclosing context permits them.

# Blocks

A block contains zero or more statements:

~~~gnr
{
    const user = User::findOrFail(id);

    return json(user);
}
~~~

Blocks create lexical scope.

Bindings declared inside a block are not visible outside that block.

# Immutable bindings

Use `const` for an immutable local binding:

~~~gnr
const user = User::findOrFail(id);
const title = 'Users';
const count = users.count();
~~~

The type is normally inferred from the initializer.

A `const` binding cannot be reassigned:

~~~gnr
const count = 1;
count = 2;
~~~

The second statement is a semantic error.

# Explicit const type

A type may be supplied when useful:

~~~gnr
const int limit = 25;
const string title = 'Users';
~~~

The initializer must be assignable to the declared type.

# Mutable bindings

Use `let` for local state that must be reassigned:

~~~gnr
let page = 1;

page = page + 1;
~~~

This is the canonical mutable-binding syntax.

Gungnir should not rely on "first assignment implicitly declares a variable" as the long-term language contract because that makes misspelled assignments difficult to distinguish from declarations.

Therefore:

~~~gnr
let users = User::all();
users = User::where('active', true).get();
~~~

is preferred over an implicit declaration by bare assignment.

# Explicit mutable type

~~~gnr
let int attempts = 0;

attempts = attempts + 1;
~~~

# Declaration before use

A local must be declared before it is referenced.

Bindings are resolved lexically.

# Assignment

Assignment updates an existing mutable target:

~~~gnr
let status = 'pending';

status = 'complete';
~~~

Assignment is a statement-level operation.

It does not produce a value.

This prevents C/C++ patterns such as assignment inside conditions.

# Member assignment

Writable members may be assigned:

~~~gnr
user.name = 'Updated Name';
user.active = true;
~~~

Whether a member is writable is determined by its type/framework contract.

# Subscript assignment

Where the container supports mutation:

~~~gnr
values[0] = 'first';
data['name'] = 'Justin';
~~~

The receiver, key, and value types must be compatible.

# Compound assignment

The language may support common compound assignments:

~~~gnr
count += 1;
total -= discount;
quantity *= 2;
~~~

These must use normal typed arithmetic semantics.

Ordinary assignment remains sufficient if compound assignment is deferred:

~~~gnr
count = count + 1;
~~~

# Expression statements

A call may be used as a statement when its result is intentionally ignored:

~~~gnr
user.save();
logger.info('saved');
event(UserRegistered(user: user));
~~~

Pure expressions whose values are discarded may produce a tooling warning.

# If statement

~~~gnr
if (user.active) {
    return json(user);
}
~~~

The condition must be boolean-compatible.

# If / else

~~~gnr
if (user == null) {
    return response(null, 404);
} else {
    return json(user);
}
~~~

# Else if

~~~gnr
if (status == 'draft') {
    // ...
} else if (status == 'published') {
    // ...
} else {
    // ...
}
~~~

# Boolean conditions

Gungnir should not copy native C++ integer truthiness.

Prefer explicit comparisons:

~~~gnr
if (count > 0) {
    // ...
}
~~~

Optional and collection truthiness may exist only when explicitly defined by their type contracts.

# For iteration

Use `for ... in`:

~~~gnr
for (const user in users) {
    logger.info(user.email);
}
~~~

The item type is inferred from the iterable.

For:

~~~text
users: Collection<User>
~~~

the loop item is:

~~~text
user: User
~~~

# Iterating lists

~~~gnr
for (const role in roles) {
    logger.info(role);
}
~~~

# Iterating lazy results

~~~gnr
for (const user in User::cursor()) {
    // ...
}
~~~

The lazy/iterator contract must preserve streaming behavior.

# While loop

~~~gnr
let attempts = 0;

while (attempts < 3) {
    attempts = attempts + 1;
}
~~~

The condition must be boolean-compatible.

# Break

~~~gnr
for (const user in users) {
    if (user.id == targetId) {
        break;
    }
}
~~~

`break` outside a loop is a semantic error.

# Continue

~~~gnr
for (const user in users) {
    if (!user.active) {
        continue;
    }

    logger.info(user.email);
}
~~~

`continue` outside a loop is a semantic error.

# Return

Return a value:

~~~gnr
return user;
~~~

Return a controller response:

~~~gnr
return json(user);
~~~

Return from a void function:

~~~gnr
return;
~~~

The returned value must satisfy the enclosing function/framework contract.

# Controller returns

Controller actions have an implicit Response contract:

~~~gnr
public show(User user) {
    return json(user);
}
~~~

# Middleware returns

Middleware `handle` has an implicit Response contract:

~~~gnr
public handle(Request request, Next next) {
    return next(request);
}
~~~

# Policy returns

Policy actions return authorization-compatible values:

~~~gnr
public update(User user, Post post) {
    return user.id == post.user_id;
}
~~~

# Listener completion

Listeners normally complete without a result:

~~~gnr
public handle(UserRegistered event) {
    logger.info(event.user.email);
}
~~~

# Early return

~~~gnr
public show(int id) {
    const user = User::find(id);

    if (user == null) {
        return response(null, 404);
    }

    return json(user);
}
~~~

# Reachability

The semantic analyzer should diagnose obviously unreachable code:

~~~gnr
return json(user);

logger.info('never reached');
~~~

This may initially be a warning.

# Lexical scope

Each block creates a nested scope:

~~~gnr
const status = 'outer';

if (condition) {
    const message = 'inside';
}
~~~

`message` is not visible outside that block.

# Shadowing

The first stable language should discourage or reject confusing same-name shadowing:

~~~gnr
const user = Auth::user();

if (condition) {
    const user = otherUser;
}
~~~

At minimum, tooling should warn.

# Parameter scope

Parameters are bindings in the function/action body:

~~~gnr
public show(int id) {
    return json(User::findOrFail(id));
}
~~~

Redeclaring `id` in the same scope is invalid.

# Loop scope

The loop item exists only inside the loop body.

# Async statements

`await` appears inside expressions:

~~~gnr
const result = await service.fetch();
~~~

It is valid only inside an async context.

# Await in return

~~~gnr
return await next(request);
~~~

This is valid when the awaited result satisfies the enclosing return contract.

# Switch

A C-style `switch` is not required for the first stable grammar.

If multi-branch value matching is added later, a structured `match` construct is preferable to C/C++ fallthrough semantics.

# Try / catch

Error handling belongs to `errors.md`.

Gungnir should not copy native C++ exception syntax before the language error model is finalized.

# No goto

Gungnir does not expose `goto` or native labels.

# No preprocessor statements

C/C++ preprocessor directives are not application statements.

# Semicolons

Simple statements use semicolons:

~~~gnr
const user = User::findOrFail(id);
user.save();
return json(user);
~~~

Blocks do not require trailing semicolons.

# Statement AST

The target AST should include:

~~~text
BlockStatement
ConstBindingStatement
LetBindingStatement
AssignmentStatement
ExpressionStatement
IfStatement
ForStatement
WhileStatement
ReturnStatement
BreakStatement
ContinueStatement
~~~

Supported statements should not remain generic raw-source nodes.

# Binding AST

A binding retains:

~~~text
name
mutability
explicitType?
initializer
sourceSpan
resolvedType
symbol
~~~

Example:

~~~gnr
const users = User::all();
~~~

Conceptually:

~~~text
Binding
  name         users
  mutable      false
  initializer  User::all()
  type         Collection<User>
~~~

# Assignment AST

~~~gnr
user.name = 'Justin';
~~~

Conceptually:

~~~text
Assignment
  target
    Member(user, name)
  value
    StringLiteral('Justin')
~~~

# Control-flow validation

Semantic analysis should validate:

- conditions are boolean-compatible;
- bindings are declared before use;
- immutable bindings are not reassigned;
- assignments target writable values;
- break and continue appear inside loops;
- return values satisfy the enclosing contract;
- required function paths return;
- async suspension occurs only in async contexts;
- lexical scopes are respected;
- unreachable paths are diagnosable.

# Definite return

A non-void function must return on every reachable path:

~~~gnr
function string status(bool active) {
    if (active) {
        return 'active';
    }

    return 'inactive';
}
~~~

This is incomplete:

~~~gnr
function string status(bool active) {
    if (active) {
        return 'active';
    }
}
~~~

# Framework action return validation

Controller actions similarly require response-compatible completion on every reachable normal path.

The compiler should catch missing returns before native C++ compilation.

# Lowering

Only validated statements should reach C++23 lowering.

The application contract remains Gungnir semantics even when the generated output is ordinary C++ control flow.

# Design rule

~~~text
statements express application control flow
AST owns structure
semantic analysis owns validity
C++23 implements the validated result
~~~

Do not use raw-source rewriting for supported statements.

