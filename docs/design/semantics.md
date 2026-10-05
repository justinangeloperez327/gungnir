# Gungnir Semantics

> **Future design specification.** This document describes intended long-term architecture beyond the published `1.0` contract. Examples and requirements may exceed the current implementation. See the [current implementation guide](../semantics.md) before using an API.


## Release alignment

This document describes possible future architecture beyond 1.0. Current implementation guides and executable tests define the published contract.

This document defines the canonical semantic-analysis contract for the Gungnir language.

The parser answers:

~~~text
What syntax did the developer write?
~~~

Semantic analysis answers:

~~~text
What does that syntax mean?
Is every referenced name known?
Are the types compatible?
Is the control flow valid?
Do framework declarations satisfy their contracts?
Can this program be lowered without guessing?
~~~

The intended compiler pipeline is:

~~~text
.gnr source
  -> lexer
  -> tokens
  -> parser
  -> Syntax AST
  -> project/module indexing
  -> symbol declaration
  -> import/name resolution
  -> type resolution
  -> expression/type analysis
  -> control-flow analysis
  -> framework semantic analysis
  -> Validated AST
  -> framework lowering
  -> C++23 IR / codegen model
  -> C++23 emitter
~~~

Semantic analysis must operate on the AST defined by `ast.md`.

It must not reconstruct meaning by scanning source text.

# Core semantic principles

Gungnir semantics should follow these rules:

1. **Resolve names before lowering.**
2. **Resolve types before generating native types.**
3. **Validate framework contracts before code generation.**
4. **Reject ambiguity instead of relying on source/import order.**
5. **Keep syntax AST separate from semantic facts.**
6. **Use stable symbols and type identities rather than strings as identities.**
7. **Perform project-aware analysis for cross-file framework features.**
8. **Keep C++ implementation details out of application semantics.**
9. **Allow recovery/unknown states so diagnostics can continue after an error.**
10. **Only validated semantic structures may enter normal framework lowering.**

# Semantic model

Semantic information should be represented independently from the immutable syntax AST.

Conceptually:

~~~text
SemanticModel
  moduleTable
  symbolTable
  scopeTable
  importResolution
  typeTable
  expressionTypes
  memberResolution
  callResolution
  constantValues
  controlFlow
  frameworkResolution
  diagnostics
~~~

Typical side-table relationships:

~~~text
NodeId -> SymbolId
NodeId -> TypeId
NodeId -> ConstantValue
CallExpressionId -> CallableId
MemberExpressionId -> MemberId
TypeSyntaxId -> TypeId
DeclarationId -> DeclaredSymbolId
Function/ActionId -> ControlFlowGraph
RouteDeclarationId -> ResolvedRoute
~~~

# Semantic identities

Names are source text.

Symbols are compiler identities.

Types are compiler identities.

Do not use strings such as:

~~~text
"User"
"UserController::show"
"Collection<User>"
~~~

as the primary semantic identity after resolution.

Instead use identifiers such as:

~~~text
SymbolId
TypeId
CallableId
ModuleId
MemberId
~~~

Diagnostic rendering may convert those identities back to human-readable names.

# Project semantic context

Semantic analysis is project-aware.

A complete project context should know:

~~~text
modules
imports
top-level declarations
framework declaration kinds
functions
models
controllers/actions
events/listeners
policies
mail/notifications
routes
native bindings
framework prelude
~~~

This allows cross-file references to resolve before lowering.

# Analysis modes

The compiler may support two useful modes.

## Closed project mode

When the compiler knows the full project/module graph:

~~~text
closedWorld = true
~~~

unknown application references are errors.

This is appropriate for:

- normal builds;
- CI;
- full project checks;
- release compilation.

## Open/editor mode

During incomplete IDE editing, unresolved external/native symbols may temporarily remain unknown so the language server can continue providing partial diagnostics.

This must not make an unresolved program eligible for production lowering.

# Module resolution

Module semantics are defined in `modules.md`.

The module phase resolves:

~~~text
source file -> ModuleId
import syntax -> imported ModuleId
alias -> ModuleId
~~~

It validates:

- module existence;
- explicit module/path consistency;
- duplicate module identities;
- duplicate aliases;
- self imports;
- ambiguous imported names;
- dependency cycles.

Module imports are compile-time dependencies only.

# Declaration indexing

Before analyzing declaration bodies, the compiler should index declaration headers.

Example:

~~~gnr
model User {
    // ...
}

controller UserController {
    // ...
}
~~~

produces symbols conceptually like:

~~~text
Symbol(User)
  kind = Model
  module = app.models.user

Symbol(UserController)
  kind = Controller
  module = app.controllers.user_controller
~~~

This allows forward and cross-file references without relying on parse order.

# Symbol kinds

The symbol system should distinguish at least:

~~~text
Module
Model
Controller
Migration
Middleware
Policy
Event
Listener
Notification
Mail
Function
Parameter
Local
Injection
Action
Relationship
EventField
NotificationField
MailField
FrameworkBuiltin
NativeBinding
~~~

Additional kinds can be introduced without changing source syntax.

# Scope hierarchy

Semantic analysis creates lexical scopes.

Conceptually:

~~~text
Project
  Module
    Declaration
      Function / action
        Block
          Nested block
          Loop
          Lambda
~~~

Each scope knows its parent and declarations.

# Local name resolution

Within a function/action body, lookup should use a deterministic order.

A reasonable initial order is:

1. nearest local lexical binding;
2. parameter;
3. injected/member value appropriate to the declaration;
4. declaration-local symbols;
5. current-module top-level symbols;
6. imported symbols / module aliases;
7. framework prelude;
8. explicit native bindings.

A closer lexical declaration shadows an outer lexical declaration only if the language permits that shadowing.

Ambiguous same-level candidates are errors.

# Duplicate symbols

The compiler should reject duplicate declarations in the same semantic namespace where they cannot coexist.

Examples:

~~~gnr
controller UserController {
    public index() {
        // ...
    }

    public index() {
        // duplicate
    }
}
~~~

and:

~~~gnr
function string format(int value) {
    // ...
}

function string format(string value) {
    // user overloads not supported initially
}
~~~

Because user-defined overloading is not part of the initial function contract, duplicate function names in the same module scope are errors.

# Shadowing

Confusing local shadowing should be rejected or diagnosed.

Example:

~~~gnr
const user = Auth::user();

if (condition) {
    const user = otherUser;
}
~~~

The first stable implementation should at least warn.

Same-scope redeclaration is always an error.

# Framework prelude resolution

Built-in framework symbols such as:

~~~text
Route
Request
Response
Next
Auth
Gate
Mail
Notification
Validator
Password
Collection
List
Map
~~~

resolve from the framework prelude.

Prelude symbols are semantic symbols, not magic textual substitutions.

If a project declaration conflicts with a protected prelude symbol, the compiler should report a clear conflict rather than silently changing behavior.

# Type system

Semantic types are defined conceptually in `language-types.md`.

A target semantic type model includes:

~~~text
Unknown
Error
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
Async<T>
Result<T,E>
NamedType(SymbolId)
FunctionType
~~~

The semantic implementation may intern types and identify them through:

~~~text
TypeId
~~~

# Unknown versus Error type

These should be distinct concepts.

`Unknown` means analysis does not yet have enough information, often during recovery/editor interoperability.

`Error` means a diagnostic has already invalidated the expression/type.

Both should suppress cascades appropriately.

Neither is a valid type for the final validated AST.

# Resolving TypeSyntax

Syntax:

~~~text
NamedTypeSyntax(User)
~~~

is resolved to:

~~~text
TypeId(ModelType(UserSymbol))
~~~

Syntax:

~~~text
Collection<User>
~~~

becomes:

~~~text
TypeId(Collection<ModelType(UserSymbol)>)
~~~

Syntax:

~~~text
User?
~~~

becomes:

~~~text
TypeId(Optional<ModelType(UserSymbol)>)
~~~

Type resolution must not generate native C++ types yet.

# Literal typing

Examples:

~~~text
true            -> bool
25              -> int
10.5            -> decimal/default numeric literal type
'users'         -> string
"users"         -> string
null            -> null
~~~

Both quote styles resolve to the same application `string` type.

# List inference

For:

~~~gnr
const ids = [1, 2, 3];
~~~

infer:

~~~text
List<int>
~~~

For mixed but safely widenable numeric values:

~~~gnr
const values = [1, 2.5];
~~~

the compiler may infer a common safe numeric type according to the widening rules.

For irreconcilable values:

~~~gnr
const values = [1, user];
~~~

the compiler should reject the list unless an explicit supported common type exists.

Do not silently degrade every mixed list to a dynamic `Value`.

# Empty list inference

An empty list has insufficient element information without contextual typing:

~~~gnr
const values = [];
~~~

The compiler should either:

- infer from an explicit/contextual target type; or
- report that the element type cannot be inferred.

Example with context:

~~~gnr
const List<string> names = [];
~~~

# Object-shape inference

For:

~~~gnr
const payload = {
    'name': user.name,
    'active': user.active
};
~~~

infer a structural shape:

~~~text
ObjectShape {
  name: string
  active: bool
}
~~~

Do not force heterogeneous object literals into a homogeneous map.

A homogeneous `Map<K,V>` remains a separate semantic type.

# Optional semantics

`null` is assignable to optional types.

Valid:

~~~gnr
const User? user = null;
~~~

Invalid:

~~~gnr
const User user = null;
~~~

For:

~~~gnr
if (user == null) {
    // ...
}
~~~

the compiler can use control-flow narrowing so the else branch may treat `user` as non-null where safe.

# Flow-sensitive nullability

Example:

~~~gnr
const user = User::find(id);

if (user == null) {
    return response(null, 404);
}

return json(user);
~~~

If `User::find(id)` returns `User?`, then after the terminating null branch the final `user` reference can be narrowed to:

~~~text
User
~~~

Flow-sensitive narrowing should be conservative and invalidated by mutations that can change the value.

# Numeric assignability

Safe implicit widening may include:

~~~text
int -> int64
int -> decimal
int -> double
float -> double
~~~

Unsafe narrowing requires an explicit conversion.

Examples:

~~~text
int64 -> int
double -> float
decimal -> int
~~~

The exact conversion matrix belongs in one centralized type-system component.

# Assignment compatibility

For:

~~~gnr
let int count = 0;
count = expression;
~~~

the type of `expression` must be assignable to `int`.

Assignments must not change the declared/inferred variable type dynamically.

# Mutability

Semantic analysis tracks binding mutability.

~~~gnr
const count = 1;
count = 2;
~~~

is invalid.

~~~gnr
let count = 1;
count = 2;
~~~

is valid.

Member mutability is determined by the receiver/member contract.

# Operator semantics

Operators resolve through semantic rules rather than C++ overload resolution.

Examples:

~~~text
+  numeric addition or string concatenation when valid
-  numeric
*  numeric
/  numeric
%  compatible integer/numeric contract

< <= > >=
   comparable compatible operands

== !=
   defined equality-compatible operands

&& ||
   boolean operands, short-circuiting
~~~

Unsupported combinations are Gungnir diagnostics before C++ generation.

# Boolean conditions

Conditions in:

~~~text
if
while
filter predicates
policy decisions
~~~

must be boolean-compatible according to Gungnir rules.

Gungnir should not inherit arbitrary native C++ integer/pointer truthiness.

# Constant evaluation

Semantic analysis should evaluate compile-time-safe expressions where useful.

Examples:

~~~gnr
timestamps = true;
table = 'users';
return response(null, 204);
~~~

A constant evaluator may support:

~~~text
null
booleans
numeric literals
strings
lists/objects of constants
simple unary operations
simple binary operations
~~~

Constant evaluation should not execute arbitrary application functions.

# ConstantValue model

Conceptually:

~~~text
ConstantValue
  Null
  Bool
  Integer
  Decimal
  String
  List<ConstantValue>
  Object<key, ConstantValue>
~~~

Framework semantic passes can require constants where necessary.

# Expression analysis

Every expression should receive a semantic result.

Conceptually:

~~~text
ExpressionInfo
  type
  valueCategory?
  symbol?
  callable?
  constantValue?
  frameworkOperation?
  diagnosticsState
~~~

The exact implementation may split these fields into separate tables.

# Name expression semantics

For:

~~~gnr
user
~~~

semantic analysis resolves:

~~~text
NodeId -> SymbolId(user)
NodeId -> TypeId(User)
~~~

An unresolved required name is an error.

# Member access semantics

For:

~~~gnr
user.email
~~~

the compiler:

1. resolves `user`;
2. obtains its type;
3. resolves member `email`;
4. checks visibility/access contract;
5. returns the member type.

For a model, member resolution uses model/application metadata, not native C++ reflection.

# Static access semantics

For:

~~~gnr
User::all
~~~

the compiler resolves:

~~~text
User -> model symbol
all  -> model static ORM callable
~~~

For:

~~~gnr
UserController::show
~~~

in a route context, the compiler resolves:

~~~text
UserController -> controller symbol
show           -> controller action symbol
~~~

These use the same syntax node but different semantic member categories.

# Call resolution

Call resolution should produce a stable callable identity.

Conceptually:

~~~text
ResolvedCall
  callableId
  receiver?
  parameterBindings[]
  resultType
  async
  frameworkOperation?
~~~

The compiler should validate:

- callable exists;
- argument count;
- named argument names;
- duplicate named arguments;
- positional-before-named rule;
- default arguments;
- argument assignability;
- async result behavior.

# Named arguments

For:

~~~gnr
Auth::attempt(
    credentials,
    remember: true
);
~~~

semantic analysis maps:

~~~text
argument 0 -> credentials parameter
remember  -> remember parameter
~~~

The validated representation may reorder arguments into parameter order after preserving source spans for diagnostics.

# Function semantics

Ordinary named functions have explicit signatures.

For:

~~~gnr
function string fullName(
    string first,
    string last
) {
    return first + ' ' + last;
}
~~~

semantic analysis validates:

~~~text
first: string
last: string
return contract: string
all paths return string
~~~

# Default arguments

Default values are analyzed in declaration context and must be assignable to their parameter type.

They should not depend on later parameters.

# Lambda semantics

For:

~~~gnr
users.map((user) => {
    return user.name;
});
~~~

the receiving callable supplies contextual type information:

~~~text
map receiver          Collection<User>
lambda parameter      User
lambda result         string
map result            Collection<string>
~~~

Lambda capture is determined by referenced outer symbols.

Captures are semantic facts, not syntax.

# Closure capture analysis

The compiler should identify:

~~~text
captured symbol
read/write usage
lifetime requirement
async escape requirement
~~~

This is especially important when a lambda escapes its immediate call or participates in async/queued work.

# Async semantics

Async rules are defined in `async.md`.

Semantic analysis tracks:

~~~text
function/action async flag
callable async flag
Async<T> expression type
AwaitExpression
logical result T
~~~

For:

~~~gnr
const user = await loadUser(id);
~~~

if:

~~~text
loadUser: async (int) -> User
~~~

then:

~~~text
loadUser(id): Async<User>
await ...: User
~~~

# Await validation

Reject:

~~~gnr
function User load() {
    return await service.load();
}
~~~

because the enclosing function is not async.

Reject:

~~~gnr
const value = await 25;
~~~

because `int` is not awaitable.

Unawaited async results should be diagnosed where the call would otherwise be discarded or misused.

# Async framework contracts

Controller:

~~~text
public async action(...)
logical contract = Response
native lowering   = coroutine-backed Response
~~~

Middleware:

~~~text
public async handle(...)
logical contract = Response
~~~

Listener:

~~~text
public async handle(Event event)
logical contract = completion
~~~

Semantic analysis validates logical contracts without introducing `Task<T>` into the source type model.

# Statement semantics

Statements are analyzed for:

~~~text
scope
binding declaration
mutability
expression validity
control-flow effects
reachability
return behavior
loop context
~~~

# Definite declaration

A local must be declared before use.

~~~gnr
const result = count + 1;
~~~

is invalid if `count` is not resolvable.

# Definite assignment

Because canonical local declarations require initializers:

~~~gnr
let count = 0;
~~~

normal local variables begin initialized.

If future declarations allow uninitialized values, definite-assignment analysis must be added before use is permitted.

# If control flow

For:

~~~gnr
if (condition) {
    return first();
} else {
    return second();
}
~~~

the control-flow analyzer knows both branches terminate.

For:

~~~gnr
if (condition) {
    return first();
}
~~~

fallthrough remains possible.

# Loops

Semantic analysis validates:

- iterable type for `for ... in`;
- inferred loop-item type;
- boolean condition for `while`;
- `break` only inside loops;
- `continue` only inside loops.

# Reachability

Obviously unreachable code should be diagnosed.

~~~gnr
return json(user);
logger.info('never reached');
~~~

Initially this may be a warning.

# Control-flow graph

Functions and framework actions should eventually build a CFG.

Conceptually:

~~~text
ControlFlowGraph
  blocks[]
  entry
  exits[]
~~~

CFG analysis supports:

- reachability;
- definite return;
- null narrowing;
- future definite assignment;
- async suspension analysis;
- diagnostics.

# Definite return

For a function returning non-void:

~~~gnr
function string status(bool active) {
    if (active) {
        return 'active';
    }

    return 'inactive';
}
~~~

all normal reachable completion paths return a string.

This is invalid:

~~~gnr
function string status(bool active) {
    if (active) {
        return 'active';
    }
}
~~~

# Framework return contracts

Framework declarations provide implicit semantic contracts.

## Controller action

~~~text
result contract = Response
~~~

Every normal reachable completion path must produce a Response-compatible value.

## Middleware handle

~~~text
result contract = Response
~~~

## Policy action

~~~text
result contract = AuthorizationDecision
~~~

Accepted source values may include:

~~~text
bool
Decision
~~~

which normalize later.

## Listener handle

~~~text
result contract = void/completion
~~~

## Notification actions

Result depends on action/channel.

## Mail actions

Result depends on composition action.

# Framework semantic passes

Framework declarations should use dedicated semantic validators over the shared symbol/type/control-flow infrastructure.

Recommended passes:

~~~text
ModelSemanticAnalyzer
ControllerSemanticAnalyzer
MigrationSemanticAnalyzer
MiddlewareSemanticAnalyzer
PolicySemanticAnalyzer
EventSemanticAnalyzer
ListenerSemanticAnalyzer
NotificationSemanticAnalyzer
MailSemanticAnalyzer
RouteSemanticAnalyzer
OrmSemanticAnalyzer
ValidationSemanticAnalyzer
CollectionSemanticAnalyzer
AuthenticationSemanticAnalyzer
ViewSemanticAnalyzer
~~~

These may be implemented as one visitor initially, but the conceptual responsibilities should remain separable.

# Model semantics

The canonical model contract is:

~~~text
model = persistence mapping metadata + relationships
~~~

Semantic analysis validates:

- model name uniqueness;
- allowed configuration keys;
- configuration value types;
- duplicate configuration;
- primary-key metadata consistency;
- fillable/hidden/cast structures;
- relationship declaration shape;
- relationship kind;
- related model resolution;
- through-model resolution;
- key override validity;
- no arbitrary business methods;
- no migration/schema field declarations.

Example:

~~~gnr
model User {
    table = 'users';
    timestamps = true;

    posts() {
        return hasMany('posts');
    }
}
~~~

should normalize to semantic model metadata before lowering.

# ORM semantics

ORM calls are resolved from the receiver type.

For:

~~~gnr
User::where('active', true)
    .orderBy('name')
    .get()
~~~

semantic typing proceeds:

~~~text
User::where(...)    Query<User>
.orderBy(...)       Query<User>
.get()              Collection<User>
~~~

The ORM semantic layer validates:

- operation exists;
- model attributes exist where statically known;
- argument types;
- relationship names;
- eager-loading paths;
- aggregate result types;
- query versus collection operation boundaries.

# Migration semantics

A migration must define:

~~~text
one up()
one down()
~~~

Schema operations are valid only inside migration/schema contexts.

Semantic analysis validates:

- table operation forms;
- column operation names;
- argument types;
- duplicate operations where statically detectable;
- foreign-key structure;
- index structure;
- supported backend-portable syntax where required.

Migrations define schema.

Models do not.

# Controller semantics

A controller contains only:

~~~text
inject declarations
public actions
~~~

Validate:

- injection type resolution;
- duplicate injection names;
- public action uniqueness;
- parameter type resolution;
- route-model-compatible parameter types;
- async/await legality;
- Response-compatible return paths;
- no arbitrary mutable fields;
- no explicit constructors/destructors;
- no private/protected action syntax in the initial contract.

# Routing semantics

Route syntax is normalized to semantic route metadata.

For:

~~~gnr
Route::get('/users/{user}', UserController::show)
    .middleware(AuthMiddleware)
    .name('users.show');
~~~

resolve:

~~~text
method       GET
path         /users/{user}
controller   UserController symbol
action       show symbol
middleware   AuthMiddleware symbol
name         users.show
parameters   user
~~~

Validate:

- HTTP operation;
- path syntax;
- route parameter uniqueness;
- controller/action existence;
- action parameter compatibility;
- middleware references;
- route-name uniqueness;
- constraint compatibility;
- model-binding compatibility;
- route ambiguity/precedence rules where statically checkable.

# Route-model binding

For:

~~~gnr
Route::get('/users/{user}', UserController::show);

public show(User user) {
    // ...
}
~~~

semantic analysis can resolve:

~~~text
route parameter user
parameter type User
binding target model User
~~~

Custom binding keys such as:

~~~text
{post:slug}
~~~

must resolve against compatible model metadata.

# Middleware semantics

Middleware validates:

- only injections plus one `handle`;
- exactly one handle action;
- Request-compatible first parameter;
- Next-compatible second parameter;
- async/await legality;
- Response-compatible completion;
- dependency resolution;
- no arbitrary fields/methods.

# Request/response semantics

`Request` and response helpers are framework symbols with typed APIs.

Example:

~~~gnr
request.integer('page')
~~~

resolves to an integer-compatible type.

~~~gnr
json(user, 201)
~~~

resolves to a Response-compatible type.

The compiler validates helper argument shapes and known status/header/cookie constraints where statically possible.

# Validation semantics

For:

~~~gnr
request.validate({
    'email': 'required|email'
});
~~~

semantic analysis should identify the validation operation from the resolved Request method, not from source spelling alone.

Validation analysis may parse/normalize rule strings into structured rule semantics.

It validates:

- rule names;
- rule arity;
- rule argument types;
- structured `Rule::...` calls;
- object shape;
- database rule structure;
- known incompatible combinations.

The validated AST should receive normalized validation metadata so lowering does not reparse the rule string.

# Collection semantics

For:

~~~gnr
users
    .filter((user) => user.active)
    .map((user) => user.name)
~~~

the compiler tracks:

~~~text
users              Collection<User>
filter callback    User -> bool
filter result      Collection<User>
map callback       User -> string
map result         Collection<string>
~~~

Semantic analysis validates callback contracts and aggregate/comparison operations.

# Authentication semantics

`Auth` resolves from the framework prelude.

Examples:

~~~text
Auth::check()        -> bool
Auth::guest()        -> bool
Auth::user()         -> configured user type?
Auth::id()           -> configured identity key type?
Auth::attempt(...)   -> bool + authentication side effect
~~~

The configured authenticatable model/provider should influence semantic result types where project configuration is available.

Authentication and authorization remain separate semantic domains.

# Policy semantics

Policy analysis validates:

- policy declaration structure;
- injection resolution;
- action parameter types;
- resource types;
- boolean/Decision-compatible results;
- optional `before` contract;
- policy-to-resource mapping;
- authorization calls such as `authorize('update', post)`.

Undefined authorization must fail closed semantically/runtime-wise rather than default allow.

# Event semantics

Events are data-only declarations.

Validate:

- field types;
- field-name uniqueness;
- no arbitrary methods;
- constructibility from named arguments;
- event-dispatch target type.

For:

~~~gnr
UserRegistered(user: user)
~~~

map named arguments to event fields.

# Listener semantics

Validate:

- exactly one `handle`;
- event parameter resolves to an Event symbol;
- injection types;
- async legality;
- listener/event registration relation;
- completion contract.

The event-to-listener link should become semantic metadata, not string convention.

# Notification semantics

Validate:

- field/injection types;
- `via` existence and result compatibility;
- selected channel names;
- corresponding channel methods;
- channel-specific result contracts;
- notifiable recipient compatibility.

Channel names may be constants when statically available.

# Mail semantics

Validate mail action contracts:

~~~text
subject      -> string
content      -> view/mail-content compatible
text         -> string
from         -> address
replyTo      -> address
attachments  -> attachment list
headers      -> structured header map
~~~

Mail transport configuration is runtime/application configuration, not mail-declaration syntax semantics.

# View semantics

The controller-side:

~~~gnr
view('users/show', {
    'user': user
})
~~~

resolves to a ViewResponse compatible with Response.

The semantic model can record:

~~~text
template = users/show
view data:
  user -> User
~~~

Template-file parsing has its own syntax tree as defined by `view.md`.

Where project template information is available, view analysis may validate known paths/attributes/helpers.

# Dependency injection semantics

For:

~~~gnr
inject UserService users;
~~~

resolve:

~~~text
type syntax -> UserService symbol/type
injection symbol -> users
container requirement -> UserService
~~~

Container construction feasibility may be validated at compile/build time where registration information is known.

The generated constructor/container plumbing is lowering, not syntax semantics.

# Native interoperability

Native bindings may participate in semantic analysis only through explicit metadata.

A native binding should provide:

~~~text
Gungnir-visible name
parameter types
logical result type
async behavior
member/static classification
lifetime constraints where relevant
~~~

Unknown native C++ text should not be accepted as if it were semantically validated Gungnir code.

# Error propagation semantics

The complete language error model belongs in `errors.md`.

Semantic analysis must still know whether an expression/function can terminate abnormally when performing control-flow analysis.

A guaranteed throw/error path may satisfy definite-return flow similarly to a return once the error model is formalized.

# Diagnostics

Semantic diagnostics should be stable, phase-specific, and source-oriented.

Useful categories include:

~~~text
module/name resolution
duplicate symbol
unknown symbol
ambiguous symbol
unknown type
type mismatch
invalid conversion
invalid operator
invalid call
argument mismatch
invalid assignment
immutable assignment
invalid return
missing return
invalid await
unawaited async result
invalid break/continue
unreachable code
framework contract violation
route resolution
relationship resolution
validation-rule error
~~~

# Diagnostic codes

Existing `GNR1xxx` ranges should be preserved where already public/stable.

As semantic analysis expands, diagnostics should use intentional ranges rather than ad-hoc numbers.

For example, a future organization might use:

~~~text
GNR20xx  module/name resolution
GNR21xx  type analysis
GNR22xx  expressions/calls
GNR23xx  statements/control flow
GNR24xx  async/lifetime
GNR25xx  framework semantic contracts
~~~

Exact ranges should be chosen once and then kept stable.

Do not renumber existing diagnostics casually.

# Diagnostic recovery

After reporting one error, the semantic analyzer should continue where safe.

Use:

~~~text
ErrorType
ErrorSymbol
poisoned expression state
~~~

to suppress predictable cascades.

Example:

~~~gnr
const user = UnknownType::find(id);
return json(user);
~~~

After reporting the unknown type, avoid producing ten misleading secondary errors about `find`, `user`, and `json` if they derive only from the same root failure.

# Warnings versus errors

Errors prevent validated AST/code generation.

Warnings may include:

- unused local;
- unused import;
- shadowing;
- unreachable statement;
- async function without a suspension point;
- discarded pure value;
- suspicious in-memory collection filtering after loading large data, where detectable.

Warnings should not change program semantics.

# Semantic pass ordering

A practical ordering is:

~~~text
1. module discovery
2. import graph / cycle validation
3. top-level declaration indexing
4. symbol table construction
5. type declaration resolution
6. declaration-header validation
7. body scope construction
8. expression / call type analysis
9. statement and control-flow analysis
10. async/lifetime analysis
11. framework-specific semantic passes
12. project-wide consistency checks
13. validated-AST construction
~~~

Some passes may iterate when cross-references require it.

# Multi-pass analysis

Gungnir should not force every semantic decision into one AST walk.

Cross-file and recursive references require multiple phases.

For example:

~~~text
pass A: declare UserService
pass B: resolve controller injection UserService
pass C: analyze action calls on injected service
~~~

This is more reliable than a source-order-only analyzer.

# Project-wide consistency checks

After local declaration analysis, perform checks such as:

- duplicate route names;
- duplicate/ambiguous module symbols;
- listener targets;
- policy mapping;
- model relationship targets;
- middleware route references;
- migration identity conflicts;
- framework configuration collisions.

These require the project index.

# SemanticModel API

A useful compiler-facing API should allow queries such as:

~~~text
symbolOf(NodeId) -> SymbolId?
typeOf(NodeId) -> TypeId
constantOf(NodeId) -> ConstantValue?
callableOf(CallExpressionId) -> CallableId?
memberOf(MemberExpressionId) -> MemberId?
moduleOf(NodeId) -> ModuleId
controlFlowOf(FunctionLikeId) -> CFG
frameworkOperationOf(NodeId) -> FrameworkOperation?
~~~

Compiler passes should use these queries rather than inspect raw strings.

# Framework operation identity

For resolved framework calls, the semantic model may attach stable operation enums/IDs.

Examples:

~~~text
OrmOperation::Where
OrmOperation::Get
ResponseOperation::Json
AuthOperation::User
ValidationOperation::Validate
CollectionOperation::Map
RouteOperation::Get
~~~

This is safer than later lowerers comparing method-name strings.

Third-party/native methods can remain normal resolved callable IDs without framework operation enums.

# Side-effect metadata

The semantic model may record broad effect categories where useful:

~~~text
Pure
ReadsState
WritesState
IO
AsyncIO
Throws
AuthenticationMutation
DatabaseMutation
~~~

A full effect type system is not required initially.

Even coarse metadata can improve diagnostics such as discarded pure expressions or unsafe compile-time evaluation.

# Validated program eligibility

A module/project may enter normal Validated AST construction only when:

- required modules/imports resolve;
- required symbols resolve;
- required types resolve;
- calls resolve;
- assignments are valid;
- return contracts are satisfied;
- control flow is valid;
- async rules are valid;
- framework declaration contracts pass;
- project-wide required references resolve;
- no error-level semantic diagnostics remain.

Warnings do not block validation.

# Validated AST handoff

Semantic analysis should normalize enough information that `validated-ast.md` can remove ambiguity.

For example, syntax:

~~~gnr
User::where('active', true).get()
~~~

semantic model:

~~~text
User -> ModelSymbol(User)
where -> OrmOperation::Where
get -> OrmOperation::Get
true -> bool
result -> Collection<User>
~~~

validated form can then become:

~~~text
ValidatedOrmQuery
  model = UserSymbol
  predicates[]
  terminal = Get
  resultType = Collection<User>
~~~

Lowering no longer needs name lookup or method-name guessing.

# No semantic work in transpiler

The transpiler/lowering stage should not decide:

- whether a name is a model;
- whether a method is ORM;
- whether a controller action exists;
- whether a route parameter binds a model;
- whether a validation rule exists;
- whether a return is Response-compatible;
- whether `await` is legal;
- whether a collection callback returns bool.

Those are semantic responsibilities.

Lowering should receive validated facts.

# Current implementation boundary

The existing semantic layer already provides useful foundations:

- project-level indexing of framework declarations;
- duplicate declaration checks;
- duplicate member/parameter checks;
- some relationship-target validation;
- some scalar type inference;
- literal inference;
- simple assignability;
- local type tracking;
- some expression/call analysis;
- framework action indexing;
- closed-world unresolved-reference behavior.

However, the current implementation remains transitional.

Important limitations include:

~~~text
symbols keyed mainly by strings
framework declarations indexed in flat maps
types represented with TypeKind + string name
no stable SymbolId / TypeId identity layer
limited generic/optional/object-shape typing
single analyzer performing many unrelated checks
expression meaning constrained by generic AST nodes
limited module-aware lookup
no complete call/member resolution model
no full CFG/control-flow model
no separate semantic side tables keyed by NodeId
framework operations still partly recognized through names
no validated-AST handoff yet
~~~

This document defines the target semantic architecture rather than claiming those capabilities already exist.

# Migration strategy

Recommended implementation order:

1. Introduce stable `SymbolId`, `TypeId`, `ModuleId`, and semantic side tables.
2. Build module-aware project declaration indexing from the new AST.
3. Replace string-based type identities with interned semantic types.
4. Implement lexical scope trees and deterministic name resolution.
5. Implement dedicated type-syntax resolution.
6. Implement expression typing and operator validation.
7. Implement structured call/member resolution.
8. Implement local mutability and assignment checks.
9. Build control-flow graphs and definite-return analysis.
10. Move async legality/await typing fully into semantic analysis.
11. Split framework semantic rules into focused analyzers.
12. Normalize validation/ORM/route/framework operations to stable semantic IDs.
13. Add project-wide consistency passes.
14. Construct the Validated AST.
15. Remove semantic guessing from lowerers/transpiler.

This can be done incrementally without rewriting the runtime.

# Compiler performance

Semantic architecture should support incremental/project-scale compilation.

Useful implementation techniques include:

- interned symbols/types;
- ID-indexed dense side tables;
- module interface hashes;
- dependency-driven invalidation;
- per-function/body semantic caches;
- immutable syntax trees;
- lazily computed expensive semantic queries where appropriate.

Correctness and deterministic diagnostics come before premature optimization.

# Language server integration

The same semantic model should power IDE features.

Examples:

~~~text
go to definition      -> SymbolId -> declaration span
hover                  -> TypeId / symbol metadata
find references        -> resolved SymbolId usage index
completion             -> visible symbols + receiver members
rename                 -> SymbolId reference set
diagnostics            -> semantic diagnostics
signature help         -> CallableId
~~~

The language server should not maintain a separate incompatible parser/semantic interpretation.

# Semantic debugging

Developer tooling should expose semantic dumps.

Recommended commands:

~~~text
gungnirc --dump-ast
gungnirc --dump-symbols
gungnirc --dump-types
gungnirc --dump-semantic
gungnirc --dump-cfg
gungnirc --dump-validated-ast
~~~

These make compiler-phase bugs observable.

# Complete semantic example

Source:

~~~gnr
module app.controllers.user_controller;

import app.models.user;
import app.services.user_service;

controller UserController {
    inject UserService users;

    public async show(int id) {
        const user = await users.find(id);

        if (user == null) {
            return response(null, 404);
        }

        return json(user);
    }
}

Route::get('/users/{user}', UserController::show)
    .name('users.show');
~~~

After syntax parsing, semantic analysis should establish facts conceptually like:

~~~text
module
  app.controllers.user_controller -> ModuleId 12

imports
  app.models.user                 -> ModuleId 3
  app.services.user_service       -> ModuleId 8

symbols
  UserController                  -> ControllerSymbol 51
  UserService                     -> Service/Native/ApplicationSymbol 22
  show                            -> ActionSymbol 52
  id                              -> ParameterSymbol 53
  users                           -> InjectionSymbol 54
  user                            -> LocalSymbol 55

types
  id                              -> int
  users                           -> UserService
  users.find(id)                  -> Async<User?>
  await users.find(id)            -> User?
  user after null guard           -> User
  response(null, 404)             -> Response
  json(user)                      -> Response

async
  action show                     -> async
  await                           -> legal

control flow
  null branch                     -> returns Response
  fallthrough after null branch   -> user narrowed to User
  final return                    -> Response
  all normal paths                -> Response

route
  method                          -> GET
  controller                      -> UserController
  action                          -> show
  path parameter                  -> user
  action parameter compatibility -> validated according to route contract
  route name                      -> users.show
~~~

Only after these facts are established should the compiler build the validated representation and lower to C++23.

# Relationship to other specifications

~~~text
grammar.md
  defines valid syntax

ast.md
  defines syntax-tree representation

semantics.md
  defines meaning, resolution, typing, and validity

validated-ast.md
  defines normalized compiler representation after semantics

transpiler.md
  defines framework/native lowering from validated structures
~~~

Framework documents define their domain-specific semantic rules:

~~~text
model.md
orm.md
migration.md
controller.md
routing.md
middleware.md
request.md
response.md
validation.md
collection.md
authentication.md
policy.md
event.md
listener.md
notification.md
mail.md
view.md
async.md
modules.md
~~~

# Design rule

The semantic phase is the compiler's source of truth for meaning:

~~~text
syntax AST
  -> resolved symbols
  -> resolved types
  -> validated control flow
  -> validated framework contracts
  -> normalized semantic operations
  -> Validated AST
~~~

By the time lowering begins, the compiler should not be asking:

~~~text
"What does this name probably mean?"
"Is this method maybe ORM?"
"Does this route action exist?"
"Should this value be a Response?"
~~~

Those questions must already have deterministic semantic answers.

