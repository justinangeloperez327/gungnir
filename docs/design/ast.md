# Gungnir Abstract Syntax Tree

> **Design specification.** This document preserves the intended contract. Examples and requirements below may exceed the current implementation. See the [current implementation guide](../ast.md) before using an API.

This document defines the canonical **syntax AST** for the Gungnir language.

The AST is produced by the parser from the token stream defined by `grammar.md`.

Its job is to represent **what the developer wrote** in a structured, loss-minimized form that later compiler phases can resolve, validate, and lower.

The compiler pipeline is:

~~~text
.gnr source
  -> lexer
  -> tokens
  -> parser
  -> Syntax AST
  -> symbol / name resolution
  -> semantic analysis
  -> type analysis
  -> Validated AST
  -> framework lowering
  -> C++23 IR / code generation model
  -> C++23 emitter
~~~

The syntax AST must not contain generated C++ concepts merely because C++23 is the compilation target.

# Core design principles

The Gungnir AST should follow these rules:

1. **Mirror Gungnir syntax, not C++ syntax.**
2. **Use dedicated nodes for supported language constructs.**
3. **Do not rediscover parsed structure from raw source text.**
4. **Keep syntax facts separate from semantic facts.**
5. **Preserve precise source locations on every meaningful node.**
6. **Represent framework declarations explicitly.**
7. **Keep error-recovery nodes so parsing can continue safely.**
8. **Prefer stable node identities over fragile vector indices.**
9. **Do not use strings as substitutes for already-parsed structure.**
10. **Keep the syntax AST immutable after parsing where practical.**

# Syntax AST versus semantic data

The syntax AST contains unresolved source structure.

For example:

~~~gnr
User::findOrFail(id)
~~~

the parser should know:

~~~text
static/member expression
callee name = findOrFail
receiver syntax = User
argument syntax = id
~~~

The syntax AST should **not yet assume**:

~~~text
User is definitely a model
findOrFail is definitely an ORM method
id is definitely int
result is definitely User
~~~

Those facts belong to name resolution and semantic/type analysis.

Likewise:

~~~gnr
return json(user);
~~~

is syntactically a return statement containing a call expression.

Later phases resolve `json` as a response helper and determine that the result is Response-compatible.

# Syntax AST versus Validated AST

The syntax AST answers:

~~~text
What syntax was written?
~~~

The validated AST answers:

~~~text
What does this valid program mean after names, types,
framework rules, and control-flow contracts are resolved?
~~~

These should not be the same mutable data structure with arbitrary semantic fields added everywhere.

A clean implementation may use:

~~~text
SyntaxTree
SemanticModel keyed by NodeId
ValidatedProgram
~~~

or another equivalent separation.

The important boundary is architectural, not the exact C++ class layout.

# Program structure

At project level:

~~~text
ProjectSyntax
  modules[]
~~~

A single source file parses to:

~~~text
ModuleUnit
  id
  span
  sourceFile
  moduleDeclaration?
  imports[]
  declarations[]
~~~

A module unit corresponds to one `.gnr` source file.

# Node identity

Every AST node should have a stable identity:

~~~text
NodeId
~~~

A `NodeId` allows later compiler phases to attach information without embedding semantic state directly into syntax nodes.

Conceptually:

~~~text
NodeId -> SourceSpan
NodeId -> SymbolId
NodeId -> TypeId
NodeId -> semantic diagnostics
NodeId -> validated/lowered representation
~~~

The exact representation may be an integer arena key, generational ID, or another compact compiler-owned identifier.

# Source spans

Every meaningful syntax node must retain a source span.

Recommended model:

~~~text
SourceSpan
  file
  beginOffset
  endOffset
~~~

Line and column information may be derived through the source manager or cached where useful.

A source span should represent the complete source construct.

Subcomponents that need precise diagnostics should retain their own spans.

Example:

~~~text
ControllerDeclaration
  span
  keywordSpan
  nameSpan
  bodySpan
~~~

This allows diagnostics to underline the exact token rather than the whole declaration.

# Source manager

The AST should reference source files through a compiler-owned source identity:

~~~text
FileId
~~~

rather than copying file paths into every node.

Conceptually:

~~~text
SourceManager
  FileId -> path
  FileId -> source buffer
  FileId -> line table
~~~

# Trivia and comments

Whitespace and comments are not normally semantic AST nodes.

They remain available through:

~~~text
token stream
source buffer
trivia ranges
~~~

for formatting and source-preserving tooling.

The formatter should not require comments to be converted into application AST expressions.

# Root node

Recommended root:

~~~text
ModuleUnit
  id
  span
  file
  declaredModule?
  imports[]
  declarations[]
~~~

This replaces a purely flat:

~~~text
Program
  nodes[]
~~~

as the primary structural model.

Compiler arenas may still store nodes compactly, but logical parent/child relationships must remain explicit.

# Module declaration node

For:

~~~gnr
module app.controllers.user_controller;
~~~

AST:

~~~text
ModuleDeclaration
  id
  span
  name
    segments
      app
      controllers
      user_controller
~~~

The module name should not remain one opaque string once parsed.

# Import node

For:

~~~gnr
import app.services.billing as Billing;
~~~

AST:

~~~text
ImportDeclaration
  id
  span
  moduleName
    segments[]
  alias?
    name = Billing
    span
~~~

Semantic resolution later attaches the imported module identity.

# Declaration node family

Top-level declarations should use a closed variant/tagged hierarchy:

~~~text
Declaration
  ModelDeclaration
  ControllerDeclaration
  MigrationDeclaration
  MiddlewareDeclaration
  PolicyDeclaration
  EventDeclaration
  ListenerDeclaration
  NotificationDeclaration
  MailDeclaration
  FunctionDeclaration
  RouteDeclaration
  ErrorDeclaration
~~~

Do not represent all framework declarations as one generic class-like node when their grammars and semantics differ.

Shared implementation helpers are fine internally.

The public AST model should preserve the declaration kind.

# Identifier syntax

A source identifier should retain:

~~~text
IdentifierSyntax
  text
  span
~~~

Semantic resolution later maps it to a symbol.

Do not store a resolved C++ name in the syntax node.

# Type syntax

Types need a dedicated syntax tree.

Recommended hierarchy:

~~~text
TypeSyntax
  BuiltinTypeSyntax
  NamedTypeSyntax
  GenericTypeSyntax
  OptionalTypeSyntax
  ErrorTypeSyntax
~~~

# Built-in type syntax

For:

~~~text
string
int
bool
decimal
~~~

AST:

~~~text
BuiltinTypeSyntax
  keyword
  span
~~~

The parser records syntax.

The type system later resolves it to a semantic `TypeId`.

# Named type syntax

For:

~~~text
User
Billing::InvoiceService
~~~

AST:

~~~text
NamedTypeSyntax
  parts[]
    Billing
    InvoiceService
~~~

Do not flatten qualified type names into an uninterpreted string.

# Generic type syntax

For:

~~~text
Collection<User>
Map<string, int>
~~~

AST:

~~~text
GenericTypeSyntax
  base
  arguments[]
~~~

Example:

~~~text
GenericTypeSyntax
  base = Collection
  arguments
    NamedTypeSyntax(User)
~~~

This represents Gungnir generic type syntax, not a C++ template specialization.

# Optional type syntax

For:

~~~text
User?
~~~

AST:

~~~text
OptionalTypeSyntax
  inner
    NamedTypeSyntax(User)
~~~

Nullability is explicit structural syntax.

# Function declaration

For:

~~~gnr
function string fullName(
    string first,
    string last
) {
    return first + ' ' + last;
}
~~~

AST:

~~~text
FunctionDeclaration
  id
  span
  async = false
  name
  returnType
  parameters[]
  body
~~~

Async function:

~~~gnr
async function User loadUser(int id) {
    return await users.find(id);
}
~~~

AST:

~~~text
FunctionDeclaration
  async = true
  returnType = NamedTypeSyntax(User)
  ...
~~~

The syntax return type remains logical `User`; `Task<User>` is never inserted into the syntax AST.

# Parameter node

~~~text
Parameter
  id
  span
  type
  name
  defaultValue?
~~~

Example:

~~~gnr
int size = 25
~~~

becomes:

~~~text
Parameter
  type = int
  name = size
  defaultValue = IntegerLiteral(25)
~~~

# Injection node

For:

~~~gnr
inject UserService users;
~~~

AST:

~~~text
InjectDeclaration
  id
  span
  type
    NamedTypeSyntax(UserService)
  name
    users
~~~

The syntax node should not include `controller_name` as duplicated string metadata.

Ownership comes from tree structure:

~~~text
ControllerDeclaration
  members
    InjectDeclaration(...)
~~~

# Model declaration

Target model AST:

~~~text
ModelDeclaration
  id
  span
  name
  configurations[]
  relationships[]
~~~

Models do not contain arbitrary fields or generic methods under the current canonical model contract.

Schema fields belong to migrations.

# Model configuration

For:

~~~gnr
table = 'users';
timestamps = true;
fillable = ['name', 'email'];
~~~

AST:

~~~text
ModelConfiguration
  id
  span
  key
  value: Expression
~~~

Recommended key enum:

~~~text
ModelConfigurationKey
  Table
  Connection
  PrimaryKey
  Incrementing
  KeyType
  Fillable
  Hidden
  Casts
  Timestamps
  SoftDeletes
~~~

The value should be a parsed expression, not a pre-decoded string/bool pair.

Semantic validation checks the expected value type.

# Model relationship

For:

~~~gnr
posts() {
    return hasMany(
        'posts',
        foreignKey: 'author_id'
    );
}
~~~

AST:

~~~text
ModelRelationshipDeclaration
  id
  span
  name
  body
    ReturnStatement
      CallExpression(...)
~~~

The parser may also normalize this into a specialized relationship syntax node once the grammar pattern is recognized:

~~~text
ModelRelationshipDeclaration
  name
  relationshipCall
~~~

What matters is that it is not treated as an arbitrary generic method requiring source rescanning later.

Relationship semantics such as `HasMany` belong to semantic analysis.

# Controller declaration

~~~text
ControllerDeclaration
  id
  span
  name
  injections[]
  actions[]
~~~

Example source:

~~~gnr
controller UserController {
    inject UserService users;

    public async show(int id) {
        const user = await users.find(id);

        return json(user);
    }
}
~~~

AST:

~~~text
ControllerDeclaration
  name = UserController
  injections
    InjectDeclaration(UserService users)
  actions
    ControllerAction
      name = show
      public = true
      async = true
      parameters
        int id
      body
        ...
~~~

There is no explicit return type syntax on controller actions.

The Response contract belongs to controller semantics.

# Controller action

~~~text
ControllerAction
  id
  span
  name
  async
  parameters[]
  body
~~~

Do not store an invented source return type such as `Response`.

That is a semantic contract, not syntax.

# Migration declaration

~~~text
MigrationDeclaration
  id
  span
  name
  up
  down
~~~

Where:

~~~text
MigrationBlock
  id
  span
  body
~~~

Example:

~~~gnr
migration CreateUsersTable {
    up() {
        // ...
    }

    down() {
        // ...
    }
}
~~~

The parser should represent `up` and `down` explicitly rather than as anonymous generic framework methods.

# Middleware declaration

~~~text
MiddlewareDeclaration
  id
  span
  name
  injections[]
  handle
~~~

Handle:

~~~text
MiddlewareHandle
  id
  span
  async
  parameters[]
  body
~~~

Semantic validation later enforces:

~~~text
exactly one handle
Request-compatible first parameter
Next-compatible second parameter
Response-compatible completion
~~~

# Policy declaration

~~~text
PolicyDeclaration
  id
  span
  name
  injections[]
  actions[]
~~~

Action:

~~~text
PolicyAction
  id
  span
  name
  parameters[]
  body
~~~

The optional `before` method remains a policy action syntactically and may be classified specially in semantic analysis.

# Event declaration

Events are data-only:

~~~text
EventDeclaration
  id
  span
  name
  fields[]
~~~

Field:

~~~text
EventField
  id
  span
  type
  name
~~~

Example:

~~~gnr
event UserRegistered {
    User user;
    datetime registeredAt;
}
~~~

No generic method list belongs on the canonical event syntax node.

# Listener declaration

~~~text
ListenerDeclaration
  id
  span
  name
  injections[]
  handle
~~~

Handle:

~~~text
ListenerHandle
  id
  span
  async
  parameters[]
  body
~~~

Semantic analysis resolves the parameter event type and verifies the listener contract.

# Notification declaration

~~~text
NotificationDeclaration
  id
  span
  name
  fields[]
  injections[]
  actions[]
~~~

Field:

~~~text
NotificationField
  type
  name
~~~

Action:

~~~text
NotificationAction
  name
  parameters[]
  body
~~~

Semantic analysis classifies recognized actions such as:

~~~text
via
mail
database
sms
push
~~~

The parser should not hard-code every possible channel name into its grammar.

# Mail declaration

~~~text
MailDeclaration
  id
  span
  name
  fields[]
  injections[]
  actions[]
~~~

Semantic analysis classifies mail actions such as:

~~~text
subject
content
text
from
replyTo
attachments
headers
~~~

Again, syntax should preserve declared actions while the mail semantic pass validates which names are meaningful.

# Route declaration

Routes deserve a structural AST node.

For:

~~~gnr
Route::get('/users/{user}', UserController::show)
    .middleware(AuthMiddleware)
    .whereNumber('user')
    .name('users.show');
~~~

Target AST:

~~~text
RouteDeclaration
  id
  span
  baseCall
  modifiers[]
~~~

A parser-owned normalized form may be:

~~~text
RouteDeclaration
  operation
    name = get
    arguments[]
  modifiers
    RouteModifier(middleware, ...)
    RouteModifier(whereNumber, ...)
    RouteModifier(name, ...)
~~~

Semantic analysis later resolves:

~~~text
HTTP method
path
controller
action
middleware
constraints
route name
model binding
~~~

The AST should not be limited to one controller/action and one middleware string.

# Route group

For:

~~~gnr
Route::prefix('/admin')
    .middleware(AuthMiddleware)
    .group(() => {
        Route::get('/users', AdminUserController::index);
    });
~~~

the lambda body already contains nested route declarations.

The route semantic pass can interpret the call chain as a route group without reparsing the source.

# Statements

Statement AST family:

~~~text
Statement
  BlockStatement
  ConstBindingStatement
  LetBindingStatement
  AssignmentStatement
  ExpressionStatement
  IfStatement
  ForInStatement
  WhileStatement
  ReturnStatement
  BreakStatement
  ContinueStatement
  ErrorStatement
~~~

A generic `MethodStatement` with optional fields for every possible statement kind should be phased out.

# Block statement

~~~text
BlockStatement
  id
  span
  statements[]
~~~

Blocks create lexical scopes during semantic analysis.

# Const binding

For:

~~~gnr
const user = User::findOrFail(id);
~~~

AST:

~~~text
ConstBindingStatement
  id
  span
  explicitType?
  name
  initializer
~~~

# Let binding

For:

~~~gnr
let int attempts = 0;
~~~

AST:

~~~text
LetBindingStatement
  id
  span
  explicitType = int
  name = attempts
  initializer = IntegerLiteral(0)
~~~

Mutability is represented by the node kind rather than a loose boolean where possible.

# Assignment

For:

~~~gnr
user.name = 'Justin';
~~~

AST:

~~~text
AssignmentStatement
  id
  span
  target
    MemberExpression
  operator
    Assign
  value
    StringLiteral
~~~

Compound operators may use:

~~~text
Assign
AddAssign
SubtractAssign
MultiplyAssign
DivideAssign
~~~

Assignment is not a BinaryExpression.

# Expression statement

~~~text
ExpressionStatement
  id
  span
  expression
~~~

Example:

~~~gnr
user.save();
~~~

# If statement

~~~text
IfStatement
  id
  span
  condition
  thenBlock
  elseBranch?
~~~

Else branch may contain:

~~~text
BlockStatement
IfStatement
~~~

so `else if` remains naturally recursive.

# For-in statement

For:

~~~gnr
for (const user in users) {
    logger.info(user.email);
}
~~~

AST:

~~~text
ForInStatement
  id
  span
  binding
    name = user
    immutable = true
  iterable
    NameExpression(users)
  body
~~~

Do not retain C-style `initializer/condition/update` fields for the canonical Gungnir for-loop.

# While statement

~~~text
WhileStatement
  id
  span
  condition
  body
~~~

# Return statement

~~~text
ReturnStatement
  id
  span
  value?
~~~

The syntax node does not know whether it returns Response, User, bool, or void.

That is semantic information.

# Break and continue

~~~text
BreakStatement
  id
  span

ContinueStatement
  id
  span
~~~

Loop-context validity belongs to semantic/control-flow analysis.

# Expressions

Expression AST family:

~~~text
Expression
  NullLiteralExpression
  BoolLiteralExpression
  IntegerLiteralExpression
  DecimalLiteralExpression
  StringLiteralExpression
  NameExpression
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
  ErrorExpression
  NativeEscapeExpression?
~~~

Supported expression syntax should not be represented as:

~~~text
Expression
  kind
  text
  arguments[]
~~~

once a dedicated node exists.

# Literal nodes

Examples:

~~~text
NullLiteralExpression

BoolLiteralExpression
  value

IntegerLiteralExpression
  tokenText
  parsedValue?

DecimalLiteralExpression
  tokenText

StringLiteralExpression
  value
  quoteStyle?
~~~

Preserving original token text can help formatting and diagnostics.

Semantic numeric interpretation belongs to type analysis.

# Name expression

~~~text
NameExpression
  id
  span
  name
~~~

Later:

~~~text
SemanticModel[NodeId] -> SymbolId
~~~

The syntax node itself remains unresolved.

# Member expression

For:

~~~gnr
user.profile.name
~~~

nested AST:

~~~text
MemberExpression
  receiver
    MemberExpression
      receiver
        NameExpression(user)
      member = profile
  member = name
~~~

# Static member expression

For:

~~~gnr
User::all
~~~

AST:

~~~text
StaticMemberExpression
  receiver
    NameExpression(User)
  member
    all
~~~

This syntax node is also used for route action references:

~~~gnr
UserController::index
~~~

The semantic result differs by context.

# Call expression

For:

~~~gnr
User::findOrFail(id)
~~~

AST:

~~~text
CallExpression
  callee
    StaticMemberExpression(...)
  arguments
    PositionalArgument(NameExpression(id))
~~~

A call owns its argument nodes directly.

# Arguments

Recommended:

~~~text
CallArgument
  PositionalArgument
  NamedArgument
~~~

Named:

~~~gnr
remember: true
~~~

AST:

~~~text
NamedArgument
  id
  span
  name
    remember
  value
    BoolLiteral(true)
~~~

Named arguments should not be encoded as object entries.

# Subscript expression

~~~gnr
values[0]
~~~

AST:

~~~text
SubscriptExpression
  receiver
  index
~~~

# Group expression

~~~gnr
(a + b)
~~~

AST:

~~~text
GroupExpression
  expression
~~~

Although grouping does not normally change semantic value, retaining it in syntax AST improves source tooling and diagnostics.

A later normalized/validated AST may discard unnecessary grouping nodes.

# Unary expression

~~~text
UnaryExpression
  operator
  operand
~~~

Operator enum:

~~~text
LogicalNot
Positive
Negative
~~~

`await` should have its own node rather than be a generic unary operator because its semantic/lifetime behavior is substantially different.

# Await expression

~~~text
AwaitExpression
  id
  span
  operand
~~~

Later semantic analysis attaches:

~~~text
awaitable type
logical result type
async context
lifetime/cancellation constraints
~~~

# Binary expression

~~~text
BinaryExpression
  left
  operator
  right
~~~

Operator enum:

~~~text
Multiply
Divide
Modulo
Add
Subtract
Less
LessEqual
Greater
GreaterEqual
Equal
NotEqual
LogicalAnd
LogicalOr
~~~

Assignment is excluded.

# List expression

For:

~~~gnr
[
    'admin',
    'editor',
]
~~~

AST:

~~~text
ListExpression
  id
  span
  elements[]
~~~

Trailing comma is syntax metadata only if tooling needs it.

# Object expression

For:

~~~gnr
{
    'name': user.name,
    'active': true,
}
~~~

AST:

~~~text
ObjectExpression
  id
  span
  entries[]
~~~

Entry:

~~~text
ObjectEntry
  id
  span
  key
    StringLiteralExpression
  value
    Expression
~~~

An object entry is not a general standalone Expression node.

# Lambda expression

For:

~~~gnr
(user) => {
    return user.name;
}
~~~

AST:

~~~text
LambdaExpression
  id
  span
  async = false
  parameters[]
  body
~~~

Lambda parameter:

~~~text
LambdaParameter
  id
  span
  explicitType?
  name
~~~

Captures are **not syntax** in canonical Gungnir because there is no source capture list.

Capture analysis belongs to semantic/lifetime analysis.

# Error expression

When expression parsing fails but recovery continues:

~~~text
ErrorExpression
  id
  span
  diagnosticId?
~~~

Later phases must treat it as poisoned/unknown rather than inventing a valid value.

# Native escape expression

If Gungnir retains a native interoperability escape hatch, it should use an explicit node such as:

~~~text
NativeEscapeExpression
  span
  sourceRange
~~~

rather than using `RawExpression` as a fallback for ordinary syntax the parser should understand.

Native escape syntax must be explicit in the language specification before it is considered stable.

# Framework calls stay normal expressions

The syntax AST should not require unique expression nodes for every framework method.

Examples:

~~~gnr
User::where('active', true)
request.validate({...})
json(user)
Auth::user()
Mail::to(email)
~~~

remain ordinary call/member/static expressions.

The semantic model resolves them to framework operations.

This prevents the parser from becoming coupled to every ORM/helper method name.

# When specialized AST nodes are appropriate

Use specialized nodes when the **grammar itself** is specialized.

Good examples:

~~~text
ControllerDeclaration
InjectDeclaration
ModelConfiguration
ModelRelationshipDeclaration
RouteDeclaration
MigrationDeclaration
EventDeclaration
~~~

Do not specialize solely because a normal call happens to belong to the framework.

# Syntax values versus semantic values

Avoid syntax nodes like:

~~~text
ModelConfiguration
  value: string
  enabled: bool
~~~

because this destroys the original expression structure.

Prefer:

~~~text
ModelConfiguration
  key
  value: Expression
~~~

Then semantic analysis derives:

~~~text
timestamps = true
  -> bool constant true
~~~

# No duplicated owner names

Avoid fields such as:

~~~text
controller_name
model_name
owner_name
~~~

on child nodes when ownership already exists structurally.

Prefer:

~~~text
ControllerDeclaration
  actions[]
~~~

over:

~~~text
ControllerMethod
  controller_name = "UserController"
~~~

Duplicated ownership strings can become inconsistent after refactoring.

# No flat member indices as language structure

The current transitional AST uses member indices into a flat `Program::nodes` vector.

That can work as storage, but it should not be the semantic shape exposed to compiler phases.

Preferred logical structure:

~~~text
ControllerDeclaration
  actions: NodeList<ControllerActionId>
~~~

with typed/stable IDs if arenas are used.

Do not use untyped `size_t` indices that require callers to inspect a variant and guess the child kind.

# Typed arena option

A performant C++ implementation may use arenas:

~~~text
AstArena
  expressions
  statements
  declarations
  types
~~~

with typed IDs:

~~~text
ExpressionId
StatementId
DeclarationId
TypeSyntaxId
~~~

or a unified `NodeId` plus safe tagged access.

Both are acceptable.

The logical AST contract remains tree-shaped.

# Semantic side tables

Recommended semantic information should live outside the syntax AST.

Examples:

~~~text
NameResolution
  NodeId -> SymbolId

TypeTable
  NodeId -> TypeId

CallResolution
  CallExpressionId -> CallableId

MemberResolution
  MemberExpressionId -> MemberId

FrameworkResolution
  NodeId -> FrameworkOperation

ConstantValues
  NodeId -> ConstantValue

ControlFlowInfo
  Function/Action NodeId -> CFG
~~~

This makes syntax immutable and lets semantic phases be rerun independently.

# Symbols are not AST nodes

A symbol represents the resolved declaration identity.

Example:

~~~text
Symbol
  id
  name
  kind
  declarationNode
  module
  visibility
~~~

The declaration AST node is the source syntax.

The symbol table is semantic compiler state.

Keep them distinct.

# Types are not TypeSyntax

For:

~~~text
User?
~~~

syntax:

~~~text
OptionalTypeSyntax(
  NamedTypeSyntax(User)
)
~~~

semantic type:

~~~text
Optional<ModelType(UserSymbol)>
~~~

This distinction is essential.

A misspelled type can still exist as valid parsed syntax while semantic resolution reports it as unknown.

# Project indexing

Before full semantic analysis, the compiler can index declaration headers from the syntax AST:

~~~text
module
declaration name
declaration kind
function signature syntax
framework action signature syntax
source span
~~~

This enables cross-file symbol resolution without parsing raw text again.

# Parser recovery

The parser should construct recovery/error nodes when possible.

Example malformed source:

~~~gnr
public show(User user {
    return json(user);
}
~~~

The parser may create:

~~~text
ControllerAction
  parameters
    ErrorParameter / recovered parameter
  body
    ...
~~~

plus diagnostics.

The exact recovery node family may be smaller than the normal node family, but later phases must be able to distinguish recovered syntax from confirmed-valid syntax.

# Missing syntax representation

For required missing tokens, parsers often use synthetic/missing-token information.

Gungnir may track:

~~~text
MissingToken
  expectedKind
  insertionOffset
~~~

inside parser diagnostics or syntax nodes.

Do not silently fabricate source text and then treat it as developer-written syntax.

# AST immutability

After parsing, the syntax AST should ideally be immutable.

Semantic passes should not mutate:

~~~text
NameExpression("User")
~~~

into:

~~~text
ResolvedModelExpression(...)
~~~

inside the same node object.

Instead either:

1. attach semantic data through side tables; or
2. build the separate validated AST.

This keeps compiler phase boundaries clear.

# Validated AST handoff

After symbol resolution, type analysis, framework validation, and control-flow checks, the compiler should produce a validated representation.

Example syntax AST:

~~~text
CallExpression
  callee
    StaticMemberExpression(User, findOrFail)
  arguments
    NameExpression(id)
~~~

may become conceptually:

~~~text
ValidatedOrmCall
  model = UserSymbol
  operation = FindOrFail
  arguments
    TypedLocal(id: int)
  resultType = User
~~~

That conversion belongs to `validated-ast.md`.

# Current AST implementation

The current `include/gungnir/language/ast.hpp` already contains important foundations:

- `SourceSpan`;
- framework declaration kinds;
- `FrameworkDeclaration`;
- parser-owned method parameters and method bodies;
- expression and statement structures;
- `ModelConfiguration`;
- `ModelRelationship`;
- `InjectDeclaration`;
- `RouteDeclaration`;
- variant-based node storage.

These are useful transitional steps away from source-string scanning.

However, the current structure still contains transitional patterns that should be removed as the frontend matures:

~~~text
flat Program::nodes ownership
untyped size_t member indices
generic FrameworkMethod for semantically different declarations
ControllerMethod duplication
Expression.text
ExpressionKind::raw as broad fallback
Expression.arguments used for unrelated child shapes
generic MethodStatement with many optional-purpose fields
model configuration values stored as pre-decoded strings/bools
owner/model/controller names duplicated as strings
route node limited to a narrow controller/middleware shape
~~~

This document defines the target architecture, not a claim that all of these changes are already implemented.

# Migration strategy from current AST

The AST should evolve incrementally rather than through one risky rewrite.

Recommended order:

1. Introduce stable `NodeId` / typed node IDs and a source manager.
2. Add dedicated `TypeSyntax` nodes.
3. Replace generic expression representation with dedicated expression variants.
4. Replace generic `MethodStatement` with dedicated statement variants.
5. Introduce `ModuleUnit` as the file root.
6. Split generic framework declarations into specialized declaration nodes.
7. Replace flat string ownership metadata with structural ownership.
8. Expand route AST to preserve full route call/modifier structure.
9. Add explicit parser recovery/error nodes.
10. Remove broad raw-expression fallback for supported Gungnir grammar.
11. Build semantic side tables keyed by node IDs.
12. Build the separate validated AST.

Each step should keep generated C++ inspectable and existing framework behavior working.

# Recommended C++ shape

The exact implementation is flexible, but a target could conceptually resemble:

~~~text
struct ModuleUnit {
    NodeId id;
    SourceSpan span;
    optional<ModuleDeclarationId> module;
    vector<ImportDeclarationId> imports;
    vector<DeclarationId> declarations;
};

using Declaration = variant<
    ModelDeclaration,
    ControllerDeclaration,
    MigrationDeclaration,
    MiddlewareDeclaration,
    PolicyDeclaration,
    EventDeclaration,
    ListenerDeclaration,
    NotificationDeclaration,
    MailDeclaration,
    FunctionDeclaration,
    RouteDeclaration,
    ErrorDeclaration
>;

using Statement = variant<
    BlockStatement,
    ConstBindingStatement,
    LetBindingStatement,
    AssignmentStatement,
    ExpressionStatement,
    IfStatement,
    ForInStatement,
    WhileStatement,
    ReturnStatement,
    BreakStatement,
    ContinueStatement,
    ErrorStatement
>;

using Expression = variant<
    NullLiteralExpression,
    BoolLiteralExpression,
    IntegerLiteralExpression,
    DecimalLiteralExpression,
    StringLiteralExpression,
    NameExpression,
    MemberExpression,
    StaticMemberExpression,
    CallExpression,
    SubscriptExpression,
    GroupExpression,
    UnaryExpression,
    BinaryExpression,
    ListExpression,
    ObjectExpression,
    LambdaExpression,
    AwaitExpression,
    ErrorExpression
>;
~~~

Whether these variants contain values or typed IDs into arenas is an implementation choice.

# Complete AST example

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

Conceptual syntax AST:

~~~text
ModuleUnit
  ModuleDeclaration
    app.controllers.user_controller

  Imports
    ImportDeclaration
      app.models.user
    ImportDeclaration
      app.services.user_service

  Declarations
    ControllerDeclaration
      name = UserController

      injections
        InjectDeclaration
          type = UserService
          name = users

      actions
        ControllerAction
          name = show
          async = true

          parameters
            Parameter
              type = int
              name = id

          body
            ConstBindingStatement
              name = user
              initializer
                AwaitExpression
                  CallExpression
                    callee
                      MemberExpression
                        receiver = NameExpression(users)
                        member = find
                    arguments
                      NameExpression(id)

            IfStatement
              condition
                BinaryExpression(Equal)
                  NameExpression(user)
                  NullLiteralExpression

              thenBlock
                ReturnStatement
                  CallExpression
                    callee = NameExpression(response)
                    arguments
                      NullLiteralExpression
                      IntegerLiteralExpression(404)

            ReturnStatement
              CallExpression
                callee = NameExpression(json)
                arguments
                  NameExpression(user)

    RouteDeclaration
      baseCall
        Route::get
        '/users/{user}'
        UserController::show

      modifiers
        middleware(AuthMiddleware)
        name('users.show')
~~~

Notice what is **not** present yet:

~~~text
resolved User model symbol
resolved UserService service symbol
resolved response/json helpers
Response result type
User? result type from users.find
async native Task<Response>
route model-binding semantics
HTTP route metadata
generated C++ names
~~~

Those are later-phase facts.

# AST verification

Parser tests should assert AST structure directly.

Useful tests include:

- one node per supported grammar construct;
- correct nested expression precedence;
- correct statement nesting;
- exact source spans;
- module/import hierarchy;
- specialized framework declarations;
- async flags;
- optional/generic type syntax;
- named arguments;
- list/object literals;
- lambdas;
- nested routes/groups;
- parser recovery nodes;
- no raw node for supported syntax.

Golden/tree snapshots may be useful if kept stable and readable.

# Debug AST printer

The compiler should provide a developer/debug AST dump.

Conceptually:

~~~text
gungnirc --dump-ast file.gnr
~~~

The output should be deterministic and source-oriented.

This is especially valuable while replacing source-scanning lowerers.

A later option may provide:

~~~text
--dump-semantic
--dump-validated-ast
--dump-lowered-ir
~~~

so each compiler phase can be inspected independently.

# Performance considerations

AST quality should not be sacrificed for premature micro-optimization.

Still, C++ implementation should avoid unnecessary duplication:

- intern repeated identifiers where useful;
- store source slices by offset rather than copying large raw strings;
- use arenas/typed IDs for stable compact ownership;
- avoid recursive ownership patterns that make moves/copies expensive;
- keep semantic side tables dense and ID-indexed where practical.

The primary objective is correctness and clear phase separation.

# AST stability

The AST is an internal compiler contract.

It does not need permanent public ABI compatibility.

However, compiler passes should depend on intentional node interfaces rather than direct token/source assumptions.

Refactors should preserve the language semantics defined by the documentation even if C++ AST structs change.

# Relationship to grammar

`grammar.md` defines which syntax is valid and how tokens form constructs.

`ast.md` defines how those constructs are represented after parsing.

Example:

~~~text
grammar:
  ifStatement
    := 'if' '(' expression ')' block elseClause?

AST:
  IfStatement
    condition
    thenBlock
    elseBranch?
~~~

The AST should be simpler than the concrete grammar where punctuation no longer matters, but it must preserve all semantic source structure.

# Relationship to semantics

`semantics.md` will define:

- symbol tables;
- name resolution;
- scope;
- module resolution;
- type analysis;
- framework operation resolution;
- control-flow rules;
- diagnostics;
- constant evaluation;
- authorization/ORM/framework checks.

These phases consume the syntax AST.

# Relationship to Validated AST

`validated-ast.md` will define the compiler representation after semantic analysis succeeds.

The validated AST may intentionally normalize or erase syntax-only distinctions.

Examples:

~~~text
GroupExpression may disappear.
Named arguments may be reordered to resolved parameters.
Model configuration keys may become typed configuration fields.
Route call chains may become normalized route metadata.
Framework calls may become explicit validated operations.
~~~

That normalization must happen **after** syntax parsing and validation.

# Design rule

The syntax AST should be boring, explicit, and complete:

~~~text
source syntax
    -> one structural representation
    -> no reparsing
    -> no hidden C++ assumptions
    -> no framework meaning guessed from strings
~~~

Gungnir's parser should build the language once.

Every later compiler phase should work from that structure.

