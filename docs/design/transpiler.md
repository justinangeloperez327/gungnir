# Gungnir Transpiler

> **Design specification.** This document preserves the intended contract. Examples and requirements below may exceed the current implementation. See the [current implementation guide](../transpiler.md) before using an API.

This document defines the canonical transpilation and code-generation contract for Gungnir.

The transpiler is the final compiler stage that converts a fully validated Gungnir program into ordinary, inspectable C++23.

The intended pipeline is:

~~~text
.gnr source
  -> lexer
  -> parser
  -> Syntax AST
  -> semantic analysis
  -> Validated AST
  -> framework lowering
  -> C++23 IR / codegen model
  -> C++23 emitter
  -> native compiler
~~~

The transpiler begins **after** semantic meaning has already been resolved.

It must not behave like a second parser.

# Core responsibility

The transpiler answers:

~~~text
How should this already-valid Gungnir program be represented as C++23?
~~~

It does not answer:

~~~text
What does this name mean?
Is this model valid?
Does this route action exist?
Is this ORM method legal?
Is this value nullable?
Is this await valid?
Which validation rule was requested?
~~~

Those questions belong to earlier compiler phases.

# Primary design rule

The transpiler consumes the Validated AST.

It should never need to guess application meaning from source spelling.

The target contract is:

~~~text
ValidatedProject
  -> deterministic framework lowering
  -> deterministic C++23 model
  -> deterministic source emission
~~~

# Transpiler versus lowerer

The transpiler is the overall transformation stage.

Individual lowerers handle specific semantic domains.

Conceptually:

~~~text
Transpiler
  ModuleLowerer
  TypeLowerer
  FunctionLowerer
  ModelLowerer
  OrmLowerer
  ControllerLowerer
  RouteLowerer
  MiddlewareLowerer
  MigrationLowerer
  ValidationLowerer
  CollectionLowerer
  AuthenticationLowerer
  PolicyLowerer
  EventLowerer
  ListenerLowerer
  NotificationLowerer
  MailLowerer
  ViewLowerer
  AsyncLowerer
~~~

These lowerers consume validated semantic nodes, not raw source text.

# Lowering versus emission

Lowering and C++ source emission should be separate.

Lowering converts Gungnir semantics into a C++-oriented intermediate representation.

Emission converts that representation into text.

Recommended architecture:

~~~text
Validated AST
  -> CppProgram
  -> CppEmitter
  -> .hpp / .cpp
~~~

This keeps formatting and syntax generation separate from framework semantics.

# C++ IR

The compiler should introduce a small C++ code-generation model rather than constructing large strings throughout lowerers.

Conceptually:

~~~text
CppProgram
  includes[]
  declarations[]
  definitions[]
  registration[]
~~~

Possible declaration nodes:

~~~text
CppNamespace
CppClass
CppStruct
CppFunction
CppMethod
CppConstructor
CppField
CppVariable
CppTypeAlias
CppExpression
CppStatement
CppInitializer
CppTemplateUse
CppCoroutineFunction
~~~

The C++ IR does not need to model the entire C++ language.

It only needs the subset Gungnir generates.

# Why use a C++ IR

A structured C++ IR provides:

- deterministic formatting;
- easier testing;
- safer escaping;
- fewer malformed-source bugs;
- centralized native type mapping;
- centralized name generation;
- easier source mapping;
- easier MSVC/GCC/Clang portability;
- less string concatenation in framework lowerers.

Framework lowerers should produce C++ concepts, not formatted source fragments.

# No source rewriting

Supported Gungnir constructs must not be lowered through source edits such as:

~~~text
replace keyword model with class
insert inheritance text after declaration name
search for .where(
rewrite async tokens
replace list brackets with std::vector
~~~

Those techniques are useful during migration, but they are not the final compiler architecture.

The target path is structural.

Example:

~~~gnr
model User {
    table = 'users';
}
~~~

should become:

~~~text
ValidatedModel
  -> ModelLowerer
  -> CppClass / metadata declarations
~~~

not:

~~~text
find "model"
replace with "class"
insert C++ base class text
~~~

# No token rescanning

Lowerers should not receive the token stream merely to rediscover supported syntax.

Tokens remain useful for:

- diagnostics;
- source mapping;
- formatter;
- compatibility/native escape handling.

Normal lowering should need only:

~~~text
Validated AST
semantic type metadata
symbol metadata
source origins
target configuration
~~~

# No semantic guessing

The transpiler must not contain checks such as:

~~~text
if method_name == "where"
if name == "json"
if string contains "hasMany"
if identifier ends with "Controller"
~~~

for already validated first-party language/framework constructs.

Instead it receives stable semantic identities:

~~~text
OrmOperation::Where
ResponseOperation::Json
RelationshipKind::HasMany
SymbolKind::Controller
~~~

# Deterministic output

The same validated input and compiler configuration must produce the same generated C++.

Output must not depend on:

- hash-map iteration order;
- memory addresses;
- thread scheduling;
- filesystem enumeration order;
- nondeterministic generated IDs.

Stable ordering is important for:

- reproducible builds;
- readable diffs;
- caching;
- compiler tests.

# Generated C++ goals

Generated C++23 should be:

- ordinary;
- standards-compliant;
- inspectable;
- debuggable;
- portable across supported compilers;
- reasonably readable;
- independent from source rewriting;
- optimized enough for framework use;
- explicit about runtime/framework calls.

Gungnir should not deliberately emit obscure template metaprogramming when simpler C++ is sufficient.

# Generated C++ is not public source syntax

Application developers write Gungnir.

Generated C++ is a compilation artifact.

The compiler may use:

~~~text
templates
concepts
coroutines
RAII
move semantics
std::optional
std::vector
internal runtime types
generated namespaces
~~~

even though normal Gungnir source does not expose those mechanisms.

# Native type lowering

All application types should map through one centralized type-lowering service.

Conceptually:

~~~text
TypeLowerer
  lower(TypeId) -> CppType
~~~

Examples:

~~~text
bool                -> bool
int                 -> framework/native integer representation
int64               -> std::int64_t
uint64              -> std::uint64_t
float               -> float
double              -> double
decimal             -> configured decimal runtime type
string              -> Gungnir/native string type
Optional<T>         -> optional/native nullable representation
List<T>             -> native sequence
Collection<T>       -> gungnir collection type
Query<T>            -> ORM query type
Model<User>         -> generated/native User model type
Async<T>            -> coroutine-backed native type when emitted
~~~

No framework lowerer should independently invent native type mappings.

# Native naming

Generated native names should come from one name-generation service.

Conceptually:

~~~text
NameMangler
  symbolName(SymbolId)
  moduleNamespace(ModuleId)
  localName(SymbolId)
  syntheticName(origin, purpose)
~~~

This prevents collisions between modules or generated helpers.

# Source names and generated names

Source:

~~~text
app.models.user::User
~~~

may lower to something conceptually like:

~~~text
gungnir_generated::app::models::user::User
~~~

The exact native namespace format is internal.

Application code must never depend on it.

# Local variable names

Where safe, generated C++ should retain recognizable local names:

~~~gnr
const user = ...
~~~

may produce:

~~~cpp
const auto user = ...;
~~~

If collision avoidance requires mangling, the compiler may append stable internal suffixes.

Avoid unreadable names when there is no collision.

# Module lowering

A Gungnir module becomes a deterministic C++ code-generation unit or namespace grouping.

The implementation may choose:

1. one application-wide generated translation unit;
2. one generated unit per Gungnir module;
3. grouped generated units;
4. native C++ modules in the future.

The Gungnir module contract must not depend on this choice.

# Recommended generated file structure

A scalable default may be:

~~~text
build/gungnir/generated/
  app/
    models/
      user.g.hpp
      user.g.cpp
    controllers/
      user_controller.g.hpp
      user_controller.g.cpp
    events/
      user_registered.g.hpp
      user_registered.g.cpp

  gungnir_app_registry.g.cpp
~~~

The exact names are implementation details.

A single-file generation mode may remain useful for debugging and early implementation.

# Headers versus implementation files

The transpiler may emit declarations and definitions separately when doing so improves incremental native compilation.

For example:

~~~text
User model declaration      -> user.g.hpp
User model definitions      -> user.g.cpp
~~~

Internal generated headers should use normal include guards or pragma mechanisms generated by the compiler.

Gungnir source never writes them.

# Include management

Lowerers should request native dependencies semantically:

~~~text
RequireRuntime(Model)
RequireStandard(Optional)
RequireRuntime(Response)
~~~

A centralized include/import planner determines the actual C++ includes.

Do not scatter:

~~~text
#include <...>
~~~

string generation throughout framework lowerers.

# Model lowering

A ValidatedModel already contains normalized model metadata.

The model lowerer should generate:

- model native type;
- ORM model metadata;
- table/connection metadata;
- key metadata;
- fillable/hidden/cast metadata;
- timestamp/soft-delete metadata;
- relationship metadata/functions required by the runtime.

Example source:

~~~gnr
model User {
    table = 'users';
    primaryKey = 'id';

    fillable = [
        'name',
        'email'
    ];

    timestamps = true;

    posts() {
        return hasMany('posts');
    }
}
~~~

The lowerer should not parse those configuration expressions again.

It receives something conceptually like:

~~~text
ValidatedModel
  table = "users"
  primaryKey = "id"
  fillable = ["name", "email"]
  timestamps = true
  relationships
    posts -> HasMany(Post)
~~~

# ORM lowering

Validated ORM operations should lower through ORM runtime APIs.

Example validated query:

~~~text
Model = User
Where(active == true)
OrderBy(name ASC)
Get
~~~

may lower to native framework calls conceptually like:

~~~text
User::query()
  .where(...)
  .order_by(...)
  .get()
~~~

The exact native API may differ.

The lowerer is responsible for native mapping, not semantic validation.

# Parameter binding

ORM/database lowering must preserve parameter binding.

Source values must never be interpolated into generated SQL text merely because the compiler knows their type.

Example:

~~~gnr
User::where('email', email).first();
~~~

should remain a parameterized database operation.

# Backend separation

The Gungnir transpiler should lower ORM intent to framework database APIs, not directly hard-code PostgreSQL/MySQL/SQLite syntax into controller/model C++ output.

Database grammar selection belongs to the database/ORM backend.

This keeps one Gungnir program portable where semantics are supported.

# Controller lowering

A ValidatedController contains:

~~~text
injections
actions
async flags
parameter types
validated bodies
Response contracts
~~~

The lowerer should generate ordinary native controller classes/functions and dependency wiring.

Source:

~~~gnr
controller UserController {
    inject UserService users;

    public show(int id) {
        return json(users.find(id));
    }
}
~~~

may conceptually become:

~~~text
generated controller type
constructor/container injection for UserService
native show action
response helper calls
registration metadata
~~~

The user should not need to see constructor boilerplate in Gungnir source.

# Dependency injection lowering

Injection:

~~~gnr
inject UserService users;
~~~

should lower from resolved container metadata.

The transpiler knows:

~~~text
requested TypeId
injection SymbolId
container resolution strategy
scope/lifetime
~~~

It should not look up UserService by string at runtime unless the configured container intentionally uses string keys internally.

# Route lowering

Validated routes are already normalized.

Example:

~~~text
GET /users/{user}
handler = UserController.show
middleware = [AuthMiddleware]
name = users.show
binding user -> User
~~~

The route lowerer emits route registration directly from that metadata.

It should not regenerate a Route::get call and then ask the runtime to rediscover controller/action metadata from source-like strings unless that is an intentional runtime API.

# Route registration order

Generated route registration must preserve validated route order where matching precedence depends on order.

Deterministic compilation order must not reorder behavior.

# Middleware lowering

Validated middleware already guarantees:

~~~text
one handle action
Request parameter
Next parameter
Response-compatible result
async validity
resolved injections
~~~

The lowerer emits runtime middleware adapters/pipeline registration without rechecking those contracts.

# Migration lowering

Validated migrations should contain normalized schema operations.

Example:

~~~text
CreateTable(users)
  Id(id)
  String(name)
  String(email, unique=true)
  Timestamps
~~~

The migration lowerer maps these to the schema runtime.

It must not parse method chains from source.

# Validation lowering

Validated validation rules are structured.

Example:

~~~text
email
  Required
  Email
  Unique(users, email)
~~~

The validation lowerer generates rule metadata/runtime calls.

It must not split:

~~~text
required|email|unique:users,email
~~~

again.

# Collection lowering

Validated collection operations already know:

~~~text
operation identity
element type
callback type
result type
~~~

The collection lowerer may generate:

- ordinary loops;
- C++23 ranges;
- runtime collection methods;
- specialized optimized code.

The generated strategy must preserve Gungnir semantics.

# Collection optimization

For:

~~~gnr
users
    .filter(...)
    .map(...)
~~~

the transpiler may fuse loops only when behavior remains identical.

Optimization must preserve:

- order;
- callback evaluation count;
- side effects;
- exception/error propagation;
- short-circuit behavior where applicable.

Native compiler optimization may be sufficient initially.

# Request lowering

Validated request operations lower to typed runtime request APIs.

The lowerer already knows the operation identity and result type.

It should not inspect method spelling.

# Response lowering

Validated response operations may lower to native response builders.

Example:

~~~text
Response.Json
payload = User
status = 201
headers = ...
~~~

may emit the relevant runtime serialization/building calls.

# View lowering

Controller-side view responses lower to the runtime view engine:

~~~text
template
validated data bindings
response metadata
~~~

Template parsing/rendering is a separate view subsystem.

The application transpiler should not convert template HTML into C++ by ad-hoc interpolation unless an explicit compiled-template backend is implemented.

# Authentication lowering

Auth operations lower to the request-scoped authentication runtime.

The transpiler receives resolved:

~~~text
guard/provider
operation
logical result type
~~~

It does not discover the configured user model by searching source.

# Policy lowering

Validated policies lower to ordinary policy types/functions plus registration metadata.

Authorization operations already identify:

~~~text
policy
action
resource type
current user source
~~~

No policy lookup by source string is required during compilation.

# Event lowering

A ValidatedEvent may lower naturally to an ordinary C++ value/struct plus framework metadata.

Event fields are known and typed.

Event construction lowers from resolved field bindings.

# Listener lowering

Validated listener/event relationships become registration metadata directly.

The lowerer does not inspect the handle parameter string to determine the event.

# Notification lowering

Validated notifications already contain selected/possible channels and channel action metadata.

The notification lowerer emits runtime dispatch/message construction adapters.

Transport details stay in channel providers.

# Mail lowering

Validated mail declarations lower to runtime mail message builders.

Mail transport selection remains runtime/application configuration.

The transpiler should not embed SMTP credentials or environment-specific server addresses into generated source.

# Async lowering

Async lowering occurs from explicit ValidatedAwait / async function metadata.

Source:

~~~gnr
public async show(int id) {
    const user = await users.find(id);

    return json(user);
}
~~~

validated:

~~~text
async action
await expression
logical result Response
~~~

may lower to C++23:

~~~text
Task<Response>
co_await
co_return
~~~

or the framework's coroutine abstraction.

The important mapping is:

~~~text
Gungnir async           -> native coroutine function
Gungnir await           -> native suspension
logical T               -> native coroutine wrapper<T>
Gungnir return in async -> coroutine return
~~~

Application code never writes native coroutine syntax.

# Async lifetime lowering

The async semantic phase has already proven that values may safely survive suspension.

The lowerer is responsible for implementing that guarantee.

This may require:

- owning copies rather than string views;
- coroutine-frame storage;
- request-context handles;
- scoped-service handles;
- cancellation propagation;
- move/copy choices.

The lowerer must not emit a dangling native reference merely because a direct textual translation would compile.

# Request context across suspension

Generated async code must preserve:

- current request;
- authentication context;
- request-scoped DI state;
- tracing/request ID;
- cancellation context.

Do not depend on one OS thread remaining assigned to the coroutine.

# Return lowering

Ordinary function return:

~~~gnr
return value;
~~~

normally lowers to native return.

Inside an async function it lowers to the appropriate coroutine return.

This distinction comes from validated function metadata, not token replacement.

# Logical boolean lowering

Logical AND/OR must preserve Gungnir short-circuit behavior.

Generated code may use native:

~~~text
&&
||
~~~

when semantic equivalence is guaranteed.

# Evaluation order

If Gungnir defines a stricter evaluation order than C++ for a construct, lowering must introduce temporaries where needed.

Example conceptually:

~~~text
arg0 = evaluateFirst();
arg1 = evaluateSecond();
call(arg0, arg1);
~~~

rather than relying on native evaluation details.

# Conversion lowering

Validated conversion nodes map through one conversion lowerer.

Examples:

~~~text
int -> decimal
int -> int64
T -> Optional<T>
explicit application conversion
~~~

Lowerers should not insert extra implicit conversions independently.

# Object lowering

Structural object values may lower to:

- typed generated structs;
- framework Data/Value objects;
- JSON object builders;
- view-data containers;

depending on the validated target/context.

The strategy should be context-aware but semantically defined.

Do not collapse every object literal into one dynamic map if a stronger typed representation is available.

# List lowering

A validated List<T> may lower to a native sequence such as std::vector<T> or a runtime wrapper.

The lowerer already knows T.

There is no need to inspect literal elements again to guess the element type.

# Optional lowering

Optional<T> maps through the centralized type/runtime strategy.

Null checks should preserve Gungnir semantics.

The generated representation may use std::optional<T>, nullable model handles, or another runtime type.

# Decimal lowering

decimal must lower to the framework-selected decimal implementation.

Do not silently use double if the language contract promises decimal semantics.

# Error lowering

The full error model belongs to errors.md.

Once defined, validated error/propagation operations should lower centrally.

Do not expose C++ exception syntax as Gungnir semantics merely because exceptions are one possible implementation strategy.

# C++ emitter

The emitter converts C++ IR into source text.

Responsibilities include:

- formatting;
- indentation;
- punctuation;
- include directives;
- namespace blocks;
- class/function syntax;
- coroutine keywords;
- escaping native string literals;
- source-line directives;
- generated comments where helpful.

The emitter must not contain framework semantic logic.

# Formatting generated C++

Generated output should be stable and readable.

Example style:

~~~cpp
namespace gungnir_generated::app::controllers::user_controller {

class UserController final : public gungnir::Controller {
public:
    // ...
};

} // namespace ...
~~~

Exact style may evolve, but generated output should not be intentionally compressed or obfuscated.

# Source line mapping

The compiler should preserve source provenance.

The current compiler already supports line directives.

The target emitter should generate #line directives or equivalent mapping where useful:

~~~text
#line <source-line> "<source-file>"
~~~

This allows native compiler diagnostics and debugger locations to point back toward .gnr source.

# Source-map strategy

A richer source map may track:

~~~text
generated file
generated range
source FileId
source SourceSpan
ValidatedNodeId
~~~

This can power better diagnostic rewriting and debugging.

#line can remain the simple baseline.

# Native compiler diagnostics

Native C++ compilation should normally occur only after Gungnir semantic validation.

Therefore a C++ compiler error in generated code should usually indicate:

- compiler bug;
- runtime header/API mismatch;
- unsupported native binding;
- platform/compiler incompatibility.

Where possible, Gungnir should rewrite generated-file diagnostics back to source locations.

# Internal compiler errors

Unexpected impossible lowering states should produce an internal compiler error with:

- compiler version;
- validated node kind;
- source origin;
- operation/type IDs;
- concise reproducible diagnostic.

Do not convert compiler bugs into vague user syntax errors.

# Runtime ABI boundary

Generated C++ depends on the Gungnir runtime.

That interface should be intentional and versioned internally.

The transpiler should target a runtime/compiler compatibility version so generated code and runtime headers cannot silently drift.

# C++ standard

Generated code targets C++23.

The code generator may therefore rely on supported C++23 facilities where they improve implementation quality.

However, portability across the framework's supported compilers remains required.

# Compiler portability

Generated code should be tested against supported versions of:

~~~text
GCC
Clang
MSVC
~~~

Avoid unnecessary compiler-specific extensions.

Where platform-specific behavior is required, isolate it in runtime/platform abstractions rather than spread conditional code through generated application output.

# Target configuration

Lowering may depend on a TargetConfiguration:

~~~text
platform
compiler family
architecture
debug/release
runtime ABI version
database capabilities
feature flags
source map settings
~~~

Target configuration must not change Gungnir source semantics.

It may affect native implementation strategy.

# Debug builds

Debug generation may include:

- extra source mapping;
- runtime assertions;
- readable generated symbols;
- contract checks;
- optional generated comments.

# Release builds

Release generation may reduce debug metadata and rely more heavily on the native optimizer.

It must not change application behavior.

# Optimization philosophy

Gungnir should generate straightforward C++ and allow mature C++ compilers to optimize ordinary code.

Compiler-specific high-level optimizations should be added only when they provide measurable benefit.

Priority order:

1. correct semantics;
2. predictable generated code;
3. good runtime architecture;
4. native compiler optimization;
5. targeted Gungnir-level optimization.

# Dead code

Semantic control-flow analysis may already identify unreachable paths.

The transpiler may omit unreachable validated statements when this is safe, but it is not required initially.

# Registration generation

Many framework constructs require runtime registration.

Examples:

- routes;
- middleware;
- events/listeners;
- policies;
- migrations;
- notifications/channels;
- mailers;
- model metadata.

Registration should be emitted from validated project metadata.

Avoid hidden native static initialization where explicit startup registration is safer and more deterministic.

# Application registry

A generated application registry may conceptually provide:

~~~text
registerModels(app)
registerRoutes(app)
registerMiddleware(app)
registerPolicies(app)
registerListeners(app)
registerMigrations(app)
~~~

The exact runtime API may differ.

Explicit startup assembly is preferable to fragile global initialization order.

# Static initialization safety

Generated code should minimize dependence on cross-translation-unit static initialization order.

Prefer:

- constexpr metadata;
- function-local safe initialization;
- explicit application bootstrap;
- deterministic registration functions.

# Generated metadata

Framework metadata may be emitted as:

- constexpr structures;
- static arrays;
- registration functions;
- generated descriptor objects.

Choose the representation that keeps startup efficient and code inspectable.

# Incremental code generation

Because modules and validated declarations have stable identities, the compiler should be able to regenerate only affected native units.

Conceptually:

~~~text
validated module changed
  -> regenerate its C++ unit
  -> regenerate affected shared registry/interface units
  -> reuse unaffected generated files
~~~

# Content hashes

Generated outputs may be keyed by:

~~~text
compiler version
runtime ABI version
validated module interface hash
validated module implementation hash
target configuration
~~~

This supports robust incremental builds.

# Write only changed files

The emitter should avoid rewriting generated files whose contents have not changed.

This reduces unnecessary native recompilation.

# Generated source inspection

Developer tooling should make generated C++ easy to inspect.

Recommended command:

~~~text
gungnirc --emit-cpp file.gnr
~~~

Project mode may support:

~~~text
gungnirc build --keep-generated
~~~

The exact CLI can evolve.

# Phase dumps

Useful compiler debugging commands include:

~~~text
--dump-ast
--dump-symbols
--dump-types
--dump-semantic
--dump-validated-ast
--dump-cpp-ir
--emit-cpp
~~~

Each phase should be inspectable independently.

# Transpiler diagnostics

Most user-facing language errors should be gone before transpilation.

Transpiler diagnostics should focus on:

- unsupported target/backend capability;
- invalid native binding contract;
- runtime ABI mismatch;
- platform-specific lowering impossibility;
- internal compiler errors.

A lowerer discovering an unresolved user symbol is a compiler pipeline bug.

# Framework capability checks

Some operations may be semantically valid Gungnir but unsupported on a selected backend.

Example:

~~~text
database feature not supported by MongoDB backend
native transport unavailable on target
platform file API unsupported
~~~

Capability diagnostics should occur before native compilation, ideally during semantic/target validation or early lowering.

They should identify the source operation.

# Security constraints

Code generation must preserve security guarantees from framework APIs.

Examples:

- SQL values remain parameterized;
- HTML output stays escaped unless explicitly raw;
- response headers use validated APIs;
- mail headers use safe encoding;
- filesystem paths remain normalized/validated;
- password operations remain runtime hashing calls;
- authentication state remains request-scoped.

The transpiler must not optimize away security boundaries.

# Native string escaping

When generated source contains compile-time strings, the emitter must correctly escape them for C++ source.

Do not inject source literals directly into generated C++ without escaping.

# User data is runtime data

Runtime user input should not be embedded into generated source.

Values from Request, database records, environment, forms, or external systems remain runtime values.

# Current implementation boundary

The current Transpiler is an important transitional implementation.

Today it approximately performs:

~~~text
source
  -> lexer
  -> parser
  -> semantic analyzer
  -> framework lowerers
  -> source edits
  -> rewritten C++ source
~~~

It already has useful pieces:

- lexer/parser integration;
- semantic diagnostics;
- framework-specific lowerers;
- source spans;
- line directives;
- model/controller/middleware/migration/validation/view lowering;
- async compatibility lowering;
- typed parser nodes for several constructs.

However, it still has architectural responsibilities that should move out as the compiler matures:

~~~text
Transpiler receives raw source
Transpiler receives lexer tokens
lowerers inspect source/tokens
source edits are composed across passes
literal/list/object lowering still infers from syntax
framework class generation is partly textual
async still uses compatibility token-aware lowering
semantic/type decisions can leak into lowering
~~~

This document defines the target architecture, not a claim that the current implementation already satisfies it.

# Compatibility transpiler

During migration, keep the existing source-edit pipeline as an explicit compatibility path.

Conceptually:

~~~text
LegacySourceEditTranspiler
~~~

or:

~~~text
CompatibilityLowering
~~~

This makes the architectural distinction visible.

Newly migrated language features should lower from the Validated AST rather than adding more source-edit rules.

# Migration rule

Do not rewrite everything at once.

Migrate one domain at a time:

~~~text
Validated feature
  -> new structural lowerer
  -> C++ IR/emitter
  -> compare generated behavior
  -> remove legacy source edit for that feature
~~~

# Recommended migration sequence

1. Introduce C++ IR and CppEmitter.
2. Add centralized TypeLowerer and NameMangler.
3. Lower ordinary expressions/statements/functions from Validated AST.
4. Migrate model lowering.
5. Migrate controller and dependency injection lowering.
6. Migrate response/request/view helper lowering.
7. Migrate routes and middleware.
8. Migrate migration/schema lowering.
9. Migrate validation lowering.
10. Migrate ORM and collection operations.
11. Migrate policies/authentication.
12. Migrate events/listeners.
13. Migrate notifications/mail.
14. Migrate async/coroutine lowering.
15. Generate application registration structurally.
16. Stop passing raw source/tokens to migrated lowerers.
17. Remove overlapping source-edit passes.
18. Make ValidatedProject the only normal transpiler input.
19. Keep explicit native escape compatibility only where intentionally supported.
20. Delete obsolete compatibility rewriting after full coverage.

# Proposed API

Target compiler-facing interface:

~~~text
TranspileResult Transpiler::transpile(
    const ValidatedProject& project,
    const SemanticDatabase& semantics,
    const TargetConfiguration& target
);
~~~

Or preferably split further:

~~~text
CppProgram LoweringPipeline::lower(
    const ValidatedProject& project,
    const LoweringContext& context
);

GeneratedFiles CppEmitter::emit(
    const CppProgram& program,
    const EmitOptions& options
);
~~~

The exact C++ signatures can vary.

The key rule is that raw Gungnir source is not the main transpiler input.

# Transpile result

A project transpilation result should include:

~~~text
GeneratedFiles
  path
  contents
  sourceMap
  contentHash

diagnostics
metadata
~~~

A single in-memory code string is useful for tests but is too limited as the long-term project compilation model.

# Generated file metadata

Each generated file may track:

~~~text
source modules
validated declarations
content hash
required native dependencies
source-map entries
~~~

This supports incremental builds and diagnostics.

# Testing strategy

Transpiler tests should operate at several layers.

## Lowerer unit tests

Input:

~~~text
validated node
~~~

Assert:

~~~text
C++ IR structure
~~~

## Emitter tests

Input:

~~~text
C++ IR
~~~

Assert stable C++ text.

## End-to-end compiler tests

Input:

~~~text
.gnr source
~~~

Run:

~~~text
parse
semantics
validated AST
lower
emit
native compile
run
~~~

Assert behavior.

# Generated-code tests

Important generated-code tests include:

- C++23 syntax validity;
- GCC/Clang/MSVC compilation;
- model metadata;
- controller responses;
- DI construction;
- routes;
- async controller/middleware;
- migrations;
- validation;
- ORM queries;
- event/listener dispatch;
- mail/notification construction.

# Differential migration tests

While legacy and new lowerers coexist, selected fixtures should compile through both paths and compare:

- observable behavior;
- route metadata;
- generated response behavior;
- ORM calls;
- diagnostics where relevant.

The generated C++ text does not have to be byte-identical.

# Performance testing

Measure separately:

~~~text
Gungnir parse/semantic time
Validated AST construction
lowering time
C++ emission time
native C++ compile time
runtime performance
~~~

Do not optimize the wrong stage based only on total build time.

# Native compile time

Generated C++ architecture strongly affects build speed.

Avoid generating excessively template-heavy code when runtime functions or compact metadata provide equivalent performance.

Prefer stable generated interfaces that allow incremental native compilation.

# Runtime performance

Performance-critical framework operations should rely on efficient runtime primitives.

The transpiler can specialize when semantics make specialization clearly beneficial, but it should avoid generating huge code for every route/model if shared runtime logic is sufficient.

# Inspectability

A core Gungnir principle is that generated C++ remains understandable.

A developer investigating compiler behavior should be able to inspect a generated action and recognize:

- parameters;
- service calls;
- ORM operations;
- response construction;
- coroutine suspension.

This helps debugging and builds trust in the compiler.

# Example

Gungnir:

~~~gnr
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
~~~

Validated representation already says:

~~~text
controller = UserController
injection users = UserService
action show = async Response
id = int
users.find = async callable -> User?
await result = User?
null branch returns Response(404)
final user reference narrowed to User
json = Response.Json
~~~

Lowering may produce conceptually:

~~~cpp
class UserController final : public gungnir::Controller {
public:
    explicit UserController(UserService users)
        : users_(std::move(users)) {}

    gungnir::Task<gungnir::Response> show(std::int64_t id) {
        auto user = co_await users_.find(id);

        if (!user.has_value()) {
            co_return gungnir::response(nullptr, 404);
        }

        co_return gungnir::json(*user);
    }

private:
    UserService users_;
};
~~~

The exact generated API is illustrative only.

What matters is that the lowerer did **not** need to discover any semantic fact while emitting the C++.

# Relationship to other specifications

~~~text
grammar.md
  legal source syntax

ast.md
  parsed syntax representation

semantics.md
  meaning, symbols, types, control flow, framework validation

validated-ast.md
  normalized fully resolved compiler representation

transpiler.md
  deterministic lowering and C++23 emission
~~~

Framework docs describe the semantics the lowerers implement:

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

The final transpiler should be intentionally boring:

~~~text
Validated AST
  -> choose known lowering rule
  -> build known C++ IR
  -> emit known C++23
~~~

It should not parse, infer, guess, search, or reinterpret application source.

Once Gungnir reaches the transpiler, the language has already been understood.

