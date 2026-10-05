# Gungnir Validated AST

> **Future design specification.** This document describes intended long-term architecture beyond the published `1.0` contract. Examples and requirements may exceed the current implementation. See the [current implementation guide](../validated-ast.md) before using an API.


## Release alignment

This document describes possible future architecture beyond 1.0. Current implementation guides and executable tests define the published contract.

This document defines the canonical **Validated AST** for the Gungnir compiler.

The Validated AST is produced only after syntax parsing and semantic analysis succeed.

It is the compiler's normalized, typed, fully resolved application representation immediately before framework lowering and C++23 code generation.

The compiler pipeline is:

~~~text
.gnr source
  -> lexer
  -> tokens
  -> parser
  -> Syntax AST
  -> module / symbol resolution
  -> type analysis
  -> control-flow analysis
  -> framework semantic analysis
  -> Validated AST
  -> framework lowering
  -> C++23 IR / codegen model
  -> C++23 emitter
  -> native compiler
~~~

The Syntax AST represents what the developer wrote.

The Semantic Model records what that syntax means.

The Validated AST represents that meaning in a form that no longer requires interpretation or guessing.

# Core definition

The Validated AST should satisfy:

~~~text
Validated AST
  = resolved symbols
  + resolved types
  + resolved calls
  + normalized arguments
  + explicit conversions
  + validated control flow
  + explicit framework operations
  + preserved source provenance
~~~

It must not contain unresolved application meaning.

# Primary design rule

By the time code reaches the Validated AST, lowering should never need to ask:

~~~text
What does this name refer to?
Is this type known?
Is this call ORM?
Is this helper json()?
Does this controller action exist?
Which parameter does this named argument bind to?
Is this route model binding valid?
Is this await legal?
Does this policy action return the right thing?
Is this assignment allowed?
~~~

All of those questions belong to semantic analysis.

Lowering consumes answers.

# Validated AST is not Syntax AST

Syntax AST:

~~~gnr
User::findOrFail(id)
~~~

may look conceptually like:

~~~text
CallExpression
  callee
    StaticMemberExpression
      receiver = NameExpression(User)
      member = findOrFail
  arguments
    NameExpression(id)
~~~

The Validated AST should contain something closer to:

~~~text
ValidatedCall
  callable = OrmCallableId(Model.User, FindOrFail)
  receiver = ModelSymbol(User)
  arguments
    LocalReference(id : int)
  resultType = User
  operation = OrmOperation::FindOrFail
~~~

The source spelling has already been interpreted.

# Validated AST is not C++ IR

The Validated AST should still describe Gungnir application semantics.

For example:

~~~text
ValidatedJsonResponse
ValidatedOrmCall
ValidatedRoute
ValidatedModel
ValidatedAwait
~~~

are reasonable high-level concepts.

The Validated AST should **not** contain C++ implementation details such as:

~~~text
std::optional<User>
std::vector<User>
gungnir::Task<Response>
co_await
co_return
std::move
std::string_view
generated namespace names
native member pointers
C++ template arguments
include files
allocator choices
~~~

Those belong to lowering and emission.

# Validated AST versus Semantic Model

The Semantic Model may remain a queryable side-table representation:

~~~text
NodeId -> SymbolId
NodeId -> TypeId
CallId -> CallableId
NodeId -> ConstantValue
FunctionId -> CFG
~~~

The Validated AST is a new compiler representation built from:

~~~text
Syntax AST
+
Semantic Model
~~~

It is intentionally easier for lowering to consume.

The Semantic Model remains valuable for:

- diagnostics;
- language-server features;
- source navigation;
- hover;
- rename;
- incremental re-analysis.

The Validated AST is optimized for correct lowering.

# Construction eligibility

A normal module or project may enter Validated AST construction only when no blocking semantic errors remain.

Required invariants include:

- module identities resolve;
- imports resolve;
- dependency graph is valid;
- required symbols resolve;
- required types resolve;
- calls resolve;
- named arguments bind;
- assignments are valid;
- required conversions are known;
- return contracts are satisfied;
- control flow is valid;
- async rules are valid;
- framework declaration contracts are valid;
- project-wide framework references are valid.

Warnings do not block construction.

# No Unknown type

The Validated AST must not contain:

~~~text
Type::Unknown
~~~

A type must either be fully resolved or compilation must stop before normal lowering.

# No Error type

Likewise:

~~~text
Type::Error
~~~

is a semantic recovery mechanism.

It must not enter a normal Validated AST.

# No unresolved symbol

This should never appear in a valid lowering input:

~~~text
ValidatedName("User")
~~~

without a resolved symbol identity.

Use:

~~~text
ValidatedSymbolReference
  symbol = SymbolId(...)
  type = TypeId(...)
~~~

Human-readable names may remain for debugging, but identity comes from IDs.

# No raw source fallback

The Validated AST must not contain generic source fragments such as:

~~~text
RawExpression
RawStatement
raw_source
source_text_to_reparse
~~~

for supported Gungnir grammar.

If native interoperability is supported, it must use an explicit validated native-boundary node with a known contract.

# Validated project

Recommended project root:

~~~text
ValidatedProject
  modules[]
  entryModules[]
  frameworkMetadata
~~~

Each module:

~~~text
ValidatedModule
  moduleId
  sourceFile
  declarations[]
  dependencies[]
~~~

Module imports have already been resolved.

The Validated AST does not need to preserve an unresolved textual import statement for lowering.

# Module dependencies

For:

~~~gnr
import app.models.user;
~~~

the validated module can retain:

~~~text
dependencies
  ModuleId(app.models.user)
~~~

The source import syntax remains available through Syntax AST / source provenance.

# Validated IDs

Recommended identities include:

~~~text
ModuleId
SymbolId
TypeId
CallableId
MemberId
FrameworkOperationId
ValidatedNodeId
~~~

The compiler may reuse syntax `NodeId` for provenance, but validated nodes should have their own stable identity when useful.

# Source provenance

Every validated node should retain enough source provenance for:

- generated `#line` mapping;
- lowering diagnostics;
- internal compiler errors;
- debugging;
- generated-code traceability.

Recommended:

~~~text
SourceOrigin
  syntaxNodeId
  span
~~~

Synthetic nodes created during normalization should point back to the source construct that caused them.

# Validated declaration family

Recommended top-level representation:

~~~text
ValidatedDeclaration
  ValidatedModel
  ValidatedController
  ValidatedMigration
  ValidatedMiddleware
  ValidatedPolicy
  ValidatedEvent
  ValidatedListener
  ValidatedNotification
  ValidatedMail
  ValidatedFunction
  ValidatedRoute
~~~

There should be no generic:

~~~text
ValidatedFrameworkClass
~~~

that erases important declaration semantics.

# Validated types

Validated nodes refer to semantic types through `TypeId`.

Conceptually:

~~~text
TypeArena
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
  Model(SymbolId)
  Event(SymbolId)
  Named(SymbolId)
  Function(...)
~~~

The Validated AST does not repeat type syntax such as:

~~~text
Collection<User>
~~~

as its identity.

It references the already resolved semantic type.

# Explicit implicit conversions

If semantic analysis permits an implicit widening conversion, the Validated AST should make it explicit.

Source:

~~~gnr
const decimal total = 10;
~~~

Validated form:

~~~text
ValidatedConstBinding
  type = decimal
  initializer
    ValidatedConversion
      kind = NumericWiden
      from = int
      to = decimal
      value = IntegerLiteral(10)
~~~

Lowering should not have to rediscover conversion rules.

# No implicit narrowing

Unsafe narrowing should already have failed semantic analysis unless the source used an explicit supported conversion.

# Null conversion

Source:

~~~gnr
const User? user = null;
~~~

may normalize to:

~~~text
ValidatedOptionalConstruct
  targetType = Optional<User>
  value = Null
~~~

or another equivalent typed representation.

The exact node is an implementation choice.

The important rule is that nullability is known before lowering.

# Validated expressions

Recommended expression family:

~~~text
ValidatedExpression
  Literal
  LocalReference
  ParameterReference
  InjectionReference
  FieldReference
  MemberReference
  FunctionReference
  ModelReference
  Call
  FrameworkCall
  Subscript
  Unary
  Binary
  List
  Object
  Lambda
  Await
  Conversion
  Construct
~~~

Every validated expression has a `TypeId`.

Conceptually:

~~~text
ValidatedExpression
  type
  origin
  payload
~~~

# Literals

Validated literals contain already classified values.

Examples:

~~~text
ValidatedBoolLiteral(true)
  type = bool

ValidatedIntegerLiteral(25)
  type = int

ValidatedStringLiteral("users")
  type = string

ValidatedNullLiteral
  type = null
~~~

Single-quoted and double-quoted source strings have already converged to the same semantic string type.

Quote style is syntax/trivia information, not lowering semantics.

# Symbol references

Source:

~~~gnr
user
~~~

validated form:

~~~text
ValidatedLocalReference
  symbol = LocalSymbolId(user)
  type = User
~~~

Source:

~~~gnr
users
~~~

inside a controller where it is injected:

~~~text
ValidatedInjectionReference
  symbol = InjectionSymbolId(users)
  type = UserService
~~~

Lowering does not perform lexical lookup.

# Member access

Source:

~~~gnr
user.email
~~~

validated form:

~~~text
ValidatedMemberAccess
  receiver
    LocalReference(user : User)
  member = MemberId(User.email)
  resultType = string
~~~

The lowerer does not look up the member by name.

# Static/model access

Source:

~~~gnr
User::all
~~~

validated form may be:

~~~text
ValidatedModelMemberReference
  model = ModelSymbolId(User)
  member = OrmCallableId(All)
~~~

For:

~~~gnr
UserController::show
~~~

validated route action reference:

~~~text
ValidatedControllerActionReference
  controller = ControllerSymbolId(UserController)
  action = ActionSymbolId(show)
~~~

# Generic validated calls

Normal application function/service calls may use:

~~~text
ValidatedCall
  callable
  receiver?
  arguments[]
  resultType
  async
~~~

Every argument has already been bound to a parameter.

# Argument binding

Source:

~~~gnr
Auth::attempt(
    credentials,
    remember: true
);
~~~

Validated representation:

~~~text
ValidatedCall
  callable = AuthAttempt
  arguments
    parameter credentials -> expression(credentials)
    parameter remember    -> true
  resultType = bool
~~~

Named-argument syntax no longer needs to be interpreted by lowering.

# Default arguments

When a source argument is omitted, the validated representation may either:

1. materialize the validated default expression; or
2. store an explicit resolved default-argument binding.

Example:

~~~text
ArgumentBinding
  parameter = remember
  source = DefaultValue
  expression = false
~~~

Lowering should not search the declaration to determine defaults again.

# Call evaluation order

Validated calls should preserve the source language's argument evaluation order.

If the language defines left-to-right argument evaluation, normalization must not reorder runtime evaluation merely because named arguments are stored in parameter order.

A robust representation can distinguish:

~~~text
evaluationOrder[]
parameterBindings[]
~~~

This becomes important when arguments have side effects.

# Framework operations

Framework-specific calls should be represented with stable semantic operation identities.

Example categories:

~~~text
OrmOperation
CollectionOperation
RequestOperation
ResponseOperation
AuthOperation
ValidationOperation
RouteOperation
MailOperation
NotificationOperation
SchemaOperation
ViewOperation
AuthorizationOperation
~~~

The lowerer switches on semantic operation identity, not method-name strings.

# ORM example

Source:

~~~gnr
User::where('active', true)
    .orderBy('name')
    .get()
~~~

Validated form may remain nested typed calls:

~~~text
ValidatedFrameworkCall
  operation = Orm.Get
  receiver
    ValidatedFrameworkCall
      operation = Orm.OrderBy
      receiver
        ValidatedFrameworkCall
          operation = Orm.Where
          receiver = ModelReference(User)
          arguments
            ModelFieldReference(User.active)
            BoolLiteral(true)
          resultType = Query<User>
      arguments
        ModelFieldReference(User.name)
      resultType = Query<User>
  resultType = Collection<User>
~~~

or normalize further to:

~~~text
ValidatedOrmQuery
  model = User
  operations
    Where(field=active, value=true)
    OrderBy(field=name, direction=asc)
  terminal = Get
  resultType = Collection<User>
~~~

Either approach is valid.

The important requirement is that ORM meaning is explicit and fully typed.

# Query normalization boundary

The Validated AST should normalize **semantic identity**, but it does not need to become a database query planner.

For example:

~~~text
Where(User.active == true)
~~~

is sufficient.

SQL generation, backend grammar selection, binding slots, and execution-plan details belong to ORM/database lowering/runtime layers.

# Collection example

Source:

~~~gnr
users
    .filter((user) => {
        return user.active;
    })
    .pluck('name')
~~~

Validated representation establishes:

~~~text
users          Collection<User>

filter
  callback     (User) -> bool
  result       Collection<User>

pluck
  member       MemberId(User.name)
  result       Collection<string>
~~~

The lowerer does not infer callback types.

# Request operation

Source:

~~~gnr
request.integer('page')
~~~

validated form:

~~~text
ValidatedFrameworkCall
  operation = Request.Integer
  receiver = RequestReference(...)
  arguments
    StringLiteral("page")
  resultType = int?
~~~

The exact optionality is determined by the Request API contract.

# Response helper

Source:

~~~gnr
return json(user, 201);
~~~

validated form:

~~~text
ValidatedReturn
  value
    ValidatedFrameworkCall
      operation = Response.Json
      arguments
        user : User
        201 : int
      resultType = Response
~~~

No response compatibility check remains for lowering.

# Validation operation

Source:

~~~gnr
const data = request.validate({
    'name': 'required|string',
    'email': 'required|email'
});
~~~

The validated representation should not force lowerers to parse rule strings.

Normalize rules before handoff:

~~~text
ValidatedValidationCall
  source = request
  fields
    name
      Required
      String
    email
      Required
      Email
  resultType
    ObjectShape {
      name: string
      email: string
    }
~~~

Where validation semantics cannot infer an exact validated output field type, use an intentional framework data type rather than an unknown compiler type.

# Authentication operation

Source:

~~~gnr
Auth::user()
~~~

validated form:

~~~text
ValidatedFrameworkCall
  operation = Auth.User
  resultType = Optional<User>
  authProvider = configured provider identity
~~~

where configuration is available.

# Authorization operation

Source:

~~~gnr
authorize('update', post);
~~~

validated form:

~~~text
ValidatedAuthorization
  policy = PostPolicy
  action = PolicyActionId(update)
  userSource = CurrentAuthenticatedUser
  resource
    post : Post
  result = authorization enforcement
~~~

The lowerer should not look up a policy from the string `'update'`.

# Validated statements

Recommended family:

~~~text
ValidatedStatement
  Block
  ConstBinding
  LetBinding
  Assignment
  ExpressionStatement
  If
  ForIn
  While
  Return
  Break
  Continue
~~~

No `ErrorStatement` belongs in a normal Validated AST.

# Binding

Source:

~~~gnr
const user = User::findOrFail(id);
~~~

validated form:

~~~text
ValidatedConstBinding
  symbol = LocalSymbolId(user)
  type = User
  initializer = ValidatedOrmFindOrFail(...)
~~~

# Mutable binding

~~~gnr
let int attempts = 0;
~~~

validated:

~~~text
ValidatedLetBinding
  symbol = LocalSymbolId(attempts)
  type = int
  initializer = IntegerLiteral(0)
~~~

# Assignment

Source:

~~~gnr
attempts = attempts + 1;
~~~

validated:

~~~text
ValidatedAssignment
  target
    LocalReference(attempts : int)
  value
    Binary(Add)
      LocalReference(attempts : int)
      IntegerLiteral(1)
  type = int
~~~

Mutability has already been validated.

# If statement

Validated:

~~~text
ValidatedIf
  condition : bool
  thenBlock
  elseBranch?
~~~

The condition type is guaranteed boolean-compatible.

# Flow-sensitive narrowing

Source:

~~~gnr
const user = User::find(id);

if (user == null) {
    return response(null, 404);
}

return json(user);
~~~

Validated references after the null guard may use a narrowed type:

~~~text
LocalReference(user)
  declaredType = Optional<User>
  effectiveType = User
~~~

or an explicit refinement operation.

The representation must make the post-guard non-null guarantee available to lowering without repeating CFG analysis.

# For-in

Source:

~~~gnr
for (const user in users) {
    logger.info(user.email);
}
~~~

validated:

~~~text
ValidatedForIn
  itemSymbol = user
  itemType = User
  iterable
    users : Collection<User>
  body
~~~

Iteration compatibility has already been checked.

# While

~~~text
ValidatedWhile
  condition : bool
  body
~~~

# Return

Every return has already been checked against the enclosing contract.

Example controller:

~~~text
ValidatedReturn
  value : Response
~~~

Ordinary function:

~~~text
ValidatedReturn
  value : declared return TypeId
~~~

# Function representation

~~~text
ValidatedFunction
  symbol
  module
  async
  parameters[]
  logicalReturnType
  body
  controlFlowSummary
~~~

Parameter:

~~~text
ValidatedParameter
  symbol
  type
  defaultValue?
~~~

# Async function representation

For:

~~~gnr
async function User loadUser(int id) {
    return await repository.find(id);
}
~~~

validated:

~~~text
ValidatedFunction
  async = true
  logicalReturnType = User

  body
    ValidatedReturn
      ValidatedAwait
        operand : Async<User>
        resultType = User
~~~

There is no `Task<User>` in the Validated AST.

# Await

~~~text
ValidatedAwait
  operand
  resultType
  suspensionContext
~~~

The operand is guaranteed awaitable.

The enclosing callable is guaranteed async.

Lifetime/context checks required by the async contract have already succeeded.

# Unawaited async calls

A discarded unresolved async operation must not survive into normal validated lowering unless it is wrapped in an explicit lifecycle operation such as queue/spawn/task scope that defines ownership.

# Lambda

Validated lambda:

~~~text
ValidatedLambda
  parameters[]
  captures[]
  async
  logicalReturnType
  body
  functionType
~~~

Unlike Syntax AST, captures are explicit here because capture analysis is complete.

Capture metadata may include:

~~~text
symbol
captureKind
lifetimeClass
mutableAccess
~~~

The exact native capture representation is still deferred to lowering.

# Validated model

Recommended representation:

~~~text
ValidatedModel
  symbol
  module
  table
  connection
  primaryKey
  incrementing
  keyType
  fillable[]
  hidden[]
  casts[]
  timestamps
  softDeletes
  relationships[]
~~~

Configuration values have already been validated and normalized.

For example:

~~~gnr
timestamps = true;
~~~

becomes:

~~~text
timestamps = Enabled
~~~

not a generic expression requiring re-evaluation.

# Model metadata constants

Model configuration requiring compile-time metadata should contain validated constants.

Example:

~~~text
table = "users"
primaryKey = "id"
timestamps = true
~~~

Dynamic runtime expressions should be rejected for configuration entries that require compile-time identity.

# Validated relationship

~~~text
ValidatedRelationship
  symbol
  ownerModel
  name
  kind
  relatedModel
  throughModel?
  foreignKey?
  localKey?
  relatedKey?
  pivotTable?
  options
~~~

No relationship target remains as an unresolved string when it can be semantically resolved.

If source uses conventional resource strings such as:

~~~gnr
return hasMany('posts');
~~~

semantic analysis maps that source convention to the related model symbol before this stage.

# Validated controller

~~~text
ValidatedController
  symbol
  module
  injections[]
  actions[]
~~~

Injection:

~~~text
ValidatedInjection
  symbol
  requestedType
  containerResolution
~~~

Action:

~~~text
ValidatedControllerAction
  symbol
  async
  parameters[]
  body
  resultContract = Response
  controlFlowSummary
~~~

No explicit syntax return type is invented.

# Controller route binding metadata

A controller action may carry metadata needed by routing:

~~~text
parameter
  symbol
  type
  routeBinding?
    model
    key
~~~

This can also live on `ValidatedRoute`.

Avoid duplicating authoritative data in two places unless IDs link them.

# Validated migration

Recommended:

~~~text
ValidatedMigration
  symbol
  upOperations[]
  downOperations[]
~~~

Migration bodies should normalize schema calls into schema operations when semantics are known.

Example source:

~~~gnr
table.string('email').unique();
~~~

validated operation:

~~~text
SchemaAddColumn
  type = String
  name = email
  nullable = false
  constraints
    Unique
~~~

This prevents the transpiler from interpreting schema method chains.

# Migration boundary

The Validated AST expresses backend-neutral schema intent where the language contract is backend-neutral.

Backend-specific SQL syntax belongs to migration/database lowering.

# Validated middleware

~~~text
ValidatedMiddleware
  symbol
  injections[]
  handle
~~~

Handle:

~~~text
ValidatedMiddlewareHandle
  async
  requestParameter : Request
  nextParameter : Next
  body
  resultContract = Response
~~~

The structural contract is already proven.

# Validated policy

~~~text
ValidatedPolicy
  symbol
  resourceMappings[]
  injections[]
  before?
  actions[]
~~~

Action:

~~~text
ValidatedPolicyAction
  symbol
  parameters[]
  body
  resultContract = AuthorizationDecision
~~~

Boolean returns may be normalized to authorization decisions later or represented with an explicit conversion:

~~~text
Bool -> AuthorizationDecision
~~~

so lowering does not guess.

# Validated event

~~~text
ValidatedEvent
  symbol
  fields[]
~~~

Field:

~~~text
ValidatedEventField
  symbol
  type
  position
~~~

Event construction:

~~~gnr
UserRegistered(
    user: user
)
~~~

becomes:

~~~text
ValidatedConstruct
  target = EventSymbol(UserRegistered)
  fieldBindings
    UserRegistered.user -> expression(user : User)
  resultType = UserRegistered
~~~

# Validated listener

~~~text
ValidatedListener
  symbol
  event = EventSymbol(UserRegistered)
  injections[]
  handle
  async
~~~

The listener-to-event relation is explicit.

No registration code later searches method parameter strings.

# Validated notification

~~~text
ValidatedNotification
  symbol
  fields[]
  injections[]
  via
  channels[]
~~~

Channel example:

~~~text
ValidatedNotificationChannel
  kind = Mail
  action = NotificationActionId(mail)
  resultType = WelcomeMail
~~~

When `via()` is statically constant, selected channel identities can be normalized.

Dynamic channel selection may retain a typed validated expression plus the allowed channel set.

# Validated mail

~~~text
ValidatedMail
  symbol
  fields[]
  injections[]
  subject
  htmlContent?
  textContent?
  from?
  replyTo?
  attachments?
  headers?
~~~

Each action has already satisfied its type contract.

# Validated route

Routes should be strongly normalized.

Source:

~~~gnr
Route::get('/users/{user}', UserController::show)
    .middleware(AuthMiddleware)
    .whereNumber('user')
    .name('users.show');
~~~

validated:

~~~text
ValidatedRoute
  method = GET
  path
    literal = "/users/{user}"
    parameters
      user
        constraint = Number
        binding?
          model = User
          key = primary key
  handler
    controller = UserControllerSymbol
    action = ShowActionSymbol
  middleware
    AuthMiddlewareSymbol
  name = "users.show"
~~~

No route call-chain parsing remains for lowering.

# Route group normalization

Source route groups may be flattened/normalized into route entries carrying inherited metadata.

Example:

~~~gnr
Route::prefix('/admin')
    .middleware(AuthMiddleware)
    .group(() => {
        Route::get('/users', AdminUserController::index);
    });
~~~

validated route may become:

~~~text
method = GET
path = /admin/users
middleware = [AuthMiddleware]
handler = AdminUserController::index
~~~

The Validated AST may preserve group provenance for diagnostics/debugging.

# Route ordering

If route registration order affects matching precedence, validated routes must preserve deterministic source/application order.

Normalization must not reorder routes in a way that changes behavior.

# Validated validation rules

Validation rules should use structured identities:

~~~text
ValidationRule
  Required
  String
  Email
  Integer
  Boolean
  Min(value)
  Max(value)
  Exists(model/table/field)
  Unique(model/table/field, ignore?)
  ...
~~~

The transpiler/runtime receives structured rules.

It should not split `required|email` strings again.

# Validated collection operations

Collection operations can use stable operation identities.

Example:

~~~text
Collection.Map
Collection.Filter
Collection.Pluck
Collection.GroupBy
Collection.SortBy
~~~

Their callback/member/result types are already resolved.

# Validated view response

Source:

~~~gnr
view('users/show', {
    'user': user
})
~~~

validated:

~~~text
ValidatedViewResponse
  template = "users/show"
  data
    user
      value = LocalReference(user)
      type = User
  resultType = ViewResponse
~~~

If project view analysis is enabled, it may also carry:

~~~text
resolvedTemplateFile
templateContract
~~~

but template rendering internals remain separate from application C++ lowering.

# Validated response operations

Examples:

~~~text
Response.Text
Response.Json
Response.View
Response.Redirect
Response.File
Response.Download
Response.NoContent
~~~

Headers, cookies, status, and builder chains can be normalized into explicit response metadata operations where beneficial.

# Native interoperability node

If explicit native interoperability exists, validated representation should require a known binding:

~~~text
ValidatedNativeCall
  binding = NativeBindingId
  arguments[]
  logicalResultType
  async
  lifetimeContract
~~~

There must be no "unknown C++ call but emit it anyway" path in the normal validated compiler pipeline.

A legacy compatibility path may exist temporarily outside the strict validated pipeline.

# Framework-independent lowerability

The Validated AST should contain enough meaning that framework lowerers do not need the source text.

A lowerer may need:

- symbol metadata;
- type metadata;
- operation identity;
- normalized arguments;
- source origin.

It should not need:

- lexer tokens;
- parser heuristics;
- regex;
- string scanning;
- method-name guessing.

# Control-flow summary

The Validated AST does not necessarily need to embed the entire CFG into every function body.

It should retain or reference a validated control-flow summary:

~~~text
ControlFlowSummary
  allPathsReturn
  mayFallThrough
  suspensionPoints[]
  exits[]
~~~

The full CFG may remain in semantic compiler state.

# Guaranteed invariants

Every normal Validated AST should guarantee:

### Names

~~~text
every required reference has SymbolId / MemberId / CallableId
~~~

### Types

~~~text
every expression has a non-Unknown, non-Error TypeId
~~~

### Calls

~~~text
every call is bound to a callable
every argument is bound to a parameter
all defaults/conversions are known
~~~

### Assignments

~~~text
target is writable
binding mutability is respected
value is assignable
~~~

### Control flow

~~~text
conditions are boolean-compatible
break/continue contexts are valid
required return paths are complete
~~~

### Async

~~~text
await operand is awaitable
await context is async
logical async result is known
lifetime checks passed
~~~

### Framework declarations

~~~text
model/controller/middleware/etc. contracts are satisfied
~~~

### Cross-file references

~~~text
routes, relationships, policies, listeners, injections, etc. resolve
~~~

# What the Validated AST may normalize away

Syntax-only distinctions may disappear after validation.

Examples:

~~~text
redundant GroupExpression
quote style
import syntax
module alias spelling
named argument source order
syntactic route modifier chaining
validation rule string syntax
model configuration expression wrappers
relationship source shorthand
~~~

Only information needed for semantics, diagnostics, tooling provenance, or behavior must survive.

# What must not be normalized away

Do not erase behaviorally significant information such as:

- evaluation order;
- route registration order;
- short-circuit boolean behavior;
- explicit await/suspension points;
- source-level async boundaries;
- response middleware order;
- event listener ordering metadata;
- transaction boundaries;
- error propagation behavior;
- source origins required for diagnostics.

Normalization must preserve Gungnir semantics exactly.

# Short-circuit behavior

Validated binary logical operations should preserve:

~~~text
LogicalAnd
LogicalOr
~~~

as short-circuit operations.

They must not lower as eager function calls unless the generated semantics remain identical.

# Evaluation order

Gungnir should define deterministic expression/call evaluation order.

The Validated AST must retain enough ordering information to preserve it during C++ emission even where native C++ expression-order rules differ or could be surprising.

Where necessary, lowering may introduce temporary variables.

# Stable framework enums

Example:

~~~text
enum class OrmOperation {
    All,
    Find,
    FindOrFail,
    Where,
    WhereIn,
    OrderBy,
    With,
    Get,
    First,
    Count,
    Paginate,
    ...
};
~~~

Similar stable internal enums/IDs should exist for other first-party framework domains where that improves correctness.

These are compiler implementation details, but the architectural principle is part of this contract.

# Extensible operations

Third-party framework extensions should not require editing a giant hard-coded AST variant for every method.

A validated framework call can carry:

~~~text
FrameworkCallableId
logical operation metadata
typed signature
lowering provider / binding identity
~~~

First-party operations may additionally use enums for fast/exhaustive lowering.

# Validated AST and optimization

The Validated AST is semantic, not primarily optimization IR.

Safe high-level normalization is appropriate.

Low-level optimization such as:

- dead-code elimination;
- temporary elimination;
- C++ move optimization;
- SQL query optimization;
- coroutine-frame optimization;

belongs to later lowering/native compiler stages unless needed for language semantics.

# Constant folding

Compile-time constants may be folded while constructing the Validated AST when doing so is semantics-preserving.

Example:

~~~gnr
const limit = 20 + 5;
~~~

may become:

~~~text
IntegerLiteral(25)
~~~

but preserving the source origin remains useful.

Constant folding is optional for performance.

Constant **evaluation for semantic metadata** is more important.

# Validated AST immutability

Once constructed, the Validated AST should be immutable where practical.

Framework lowerers should consume it rather than annotate it with native codegen state.

If lowering needs mappings, use separate lowering context/side tables.

# Lowering context

Conceptually:

~~~text
LoweringContext
  validatedProject
  semanticTypeTable
  symbolMetadata
  sourceManager
  targetConfiguration
  backendCapabilities
  generatedNameTable
~~~

The Validated AST itself should not become polluted with target-specific generated names.

# Backend independence

The same Validated AST should be usable for different supported C++ compilers:

~~~text
Clang
GCC
MSVC
~~~

and different database backends where the application semantics are portable:

~~~text
SQLite
MySQL
PostgreSQL
SQL Server
MongoDB
~~~

Backend-specific lowering may reject operations that are explicitly unsupported, but the application semantics should not depend on accidental C++ compiler behavior.

# Validated AST serialization/debugging

A deterministic debug representation is strongly recommended:

~~~text
gungnirc --dump-validated-ast file.gnr
~~~

Example output should show:

- resolved symbols;
- resolved types;
- framework operation identities;
- normalized route metadata;
- async boundaries;
- explicit conversions.

It should avoid dumping unstable pointer addresses.

# Example debug form

~~~text
Controller UserController [Symbol#51]
  Injection users : UserService [Symbol#54]

  Action show [Symbol#52]
    async
    result: Response

    Parameter id : int [Symbol#53]

    Const user : User?
      Await : User?
        Call UserService.find [Callable#91]
          receiver: Injection#54
          arg id -> Parameter#53

    If
      Equal
        Local user : User?
        null

      Then
        Return
          Response(404)

    Return
      Json
        Local user : User
~~~

The precise debug syntax is not part of the language.

# Validated AST tests

Tests should verify invariants directly.

Important cases:

- every expression has a TypeId;
- no Unknown/Error type reaches validation output;
- every name resolves;
- every call has CallableId;
- named/default arguments are bound;
- numeric conversions are explicit;
- null narrowing is reflected;
- async calls/await are typed;
- controller returns are Response-compatible;
- model relationships resolve;
- routes resolve handlers/middleware/bindings;
- validation rules are normalized;
- listener event identities resolve;
- policy actions resolve;
- mail/notification channel contracts resolve;
- no raw nodes exist for supported syntax.

# Golden validated-AST fixtures

A small number of human-readable golden fixtures are useful for compiler development.

Recommended fixtures include:

~~~text
basic function
controller + injection
async controller
model + relationships
migration schema
route + middleware + model binding
validation
collection pipeline
policy
event + listener
notification + mail
module imports
~~~

Do not rely only on snapshot tests; semantic assertions should also use structured test APIs.

# Compiler assertions

In debug/development builds, lowering should assert validated invariants aggressively.

Examples:

~~~text
assert(type != Unknown)
assert(callable is resolved)
assert(framework operation is known)
assert(route handler is resolved)
~~~

A failure here indicates a compiler bug, not a user source error.

User source errors should already have been reported by earlier phases.

# Invalid-program representation

The strict Validated AST should not need error nodes.

If tooling needs a partially validated representation for IDE features, name it separately, for example:

~~~text
PartialSemanticTree
BoundTreeWithErrors
~~~

Do not weaken the invariant of `ValidatedProgram` by allowing unresolved/error nodes merely for editor convenience.

# Current implementation boundary

Gungnir does not yet have a dedicated Validated AST layer.

Today, parts of parsing, semantic analysis, framework-specific lowering, and compatibility rewriting are still connected more directly.

The existing compiler already has useful foundations:

- parser-owned framework declaration nodes;
- parser-owned method bodies;
- some expression structure;
- project-level semantic indexing;
- relationship checking;
- basic type inference/assignability;
- route nodes;
- injection nodes;
- async token-aware lowering;
- framework lowerers.

However, without a dedicated Validated AST, downstream lowering may still need to infer or re-check facts that should already be resolved.

This document defines the target handoff boundary.

# Migration plan

Implement the Validated AST after the new Syntax AST and semantic identities are established.

Recommended sequence:

1. Introduce `SymbolId`, `TypeId`, `CallableId`, `ModuleId`.
2. Build semantic side tables keyed by syntax `NodeId`.
3. Require all expressions to receive resolved TypeIds.
4. Require names/members/calls to receive resolved identities.
5. Add explicit conversion nodes.
6. Add validated statement/expression variants.
7. Add validated ordinary functions.
8. Add validated controllers/middleware/policies/listeners.
9. Add normalized models/relationships.
10. Add normalized routes.
11. Add normalized validation rules.
12. Add normalized migrations/schema operations.
13. Add ORM/collection/framework operation identities.
14. Add event/notification/mail construction metadata.
15. Add async/capture/lifetime validated metadata.
16. Build `ValidatedProject`.
17. Switch framework lowerers to consume only the Validated AST.
18. Remove source/token rescanning from lowerers.
19. Remove compatibility semantic guessing.
20. Add `--dump-validated-ast` and invariant tests.

This should be implemented incrementally, feature by feature, while preserving existing runtime behavior.

# Suggested C++ architecture

Conceptually:

~~~text
namespace gungnir::language {

struct ValidatedProject {
    std::vector<ValidatedModuleId> modules;
};

struct ValidatedModule {
    ModuleId module;
    std::vector<ValidatedDeclarationId> declarations;
};

struct ValidatedExpressionBase {
    TypeId type;
    SourceOrigin origin;
};

using ValidatedExpression = variant<
    ValidatedLiteral,
    ValidatedLocalReference,
    ValidatedParameterReference,
    ValidatedInjectionReference,
    ValidatedMemberAccess,
    ValidatedCall,
    ValidatedFrameworkCall,
    ValidatedConstruct,
    ValidatedSubscript,
    ValidatedUnary,
    ValidatedBinary,
    ValidatedList,
    ValidatedObject,
    ValidatedLambda,
    ValidatedAwait,
    ValidatedConversion
>;

using ValidatedStatement = variant<
    ValidatedBlock,
    ValidatedConstBinding,
    ValidatedLetBinding,
    ValidatedAssignment,
    ValidatedExpressionStatement,
    ValidatedIf,
    ValidatedForIn,
    ValidatedWhile,
    ValidatedReturn,
    ValidatedBreak,
    ValidatedContinue
>;

}
~~~

The actual implementation may use typed arenas instead of nested value variants.

# Typed arena recommendation

For a compiler-scale C++ implementation, typed IDs are attractive:

~~~text
ValidatedExpressionId
ValidatedStatementId
ValidatedDeclarationId
ValidatedModuleId
~~~

with arenas such as:

~~~text
ValidatedAstArena
  expressions
  statements
  declarations
  modules
~~~

This provides:

- stable references;
- compact storage;
- fewer recursive ownership issues;
- easier side tables;
- predictable traversal.

The exact storage design is not part of source-language semantics.

# Complete example

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
    .middleware(AuthMiddleware)
    .name('users.show');
~~~

After parsing:

~~~text
Syntax AST
  unresolved identifiers
  type syntax
  call syntax
  route call chain
~~~

After semantics:

~~~text
UserService resolves
users resolves to injection
id resolves to int parameter
users.find resolves to async callable
await result resolves to User?
null guard narrows user to User
response resolves to Response helper
json resolves to Response helper
UserController::show resolves to action
AuthMiddleware resolves to middleware
route metadata validates
~~~

Validated AST:

~~~text
ValidatedModule(app.controllers.user_controller)

  ValidatedController(UserController)

    Injection
      symbol = users
      type = UserService

    Action show
      async = true
      resultContract = Response

      Parameter
        symbol = id
        type = int

      ConstBinding
        symbol = user
        type = User?
        initializer
          Await
            Call
              callable = UserService.find
              receiver = injection users
              argument
                parameter id
            resultType = User?

      If
        condition
          Equal
            local user : User?
            null

        then
          Return
            ResponseOperation
              kind = General
              status = 404
              body = null

      Return
        ResponseOperation
          kind = Json
          value
            local user : User

  ValidatedRoute
    method = GET
    path = /users/{user}
    handler = UserController.show
    middleware = [AuthMiddleware]
    name = users.show
~~~

There is no ambiguity left for lowering.

# Relationship to compiler phases

~~~text
grammar.md
  Which syntax is legal?

ast.md
  How is source syntax represented?

semantics.md
  What does the source mean and is it valid?

validated-ast.md
  What normalized, fully resolved program is handed to lowering?

transpiler.md
  How is the validated program transformed into C++23?
~~~

# Design rule

The Validated AST is the compiler's **semantic firewall**:

~~~text
before Validated AST
  parsing
  lookup
  inference
  ambiguity
  diagnostics
  validation

after Validated AST
  deterministic lowering
  deterministic code generation
~~~

Once a program becomes a `ValidatedProject`, no ordinary lowerer should need to interpret source spelling again.

That boundary is essential for making Gungnir reliable, optimizable, testable, and maintainable as the language grows.

