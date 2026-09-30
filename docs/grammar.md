# Gungnir Grammar

This document defines the canonical syntax grammar for the Gungnir application language.

Gungnir source files use the `.gnr` extension and compile to ordinary C++23.

This grammar is intentionally application-oriented. It does not expose ordinary C++ class syntax, pointers, references, templates, preprocessor directives, coroutine keywords, or ownership machinery in normal framework code.

Where older examples in `language.md` conflict with this document and the newer concept-specific language contracts, this grammar and those concept-specific documents are authoritative.

# Grammar notation

The grammar uses an EBNF-like notation:

~~~text
rule        := sequence
alternative := first | second
optional    := item?
repeat      := item*
oneOrMore   := item+
group       := ( ... )
literal     := 'keyword'
~~~

Square brackets such as `[` and `]` shown inside quoted grammar literals are actual source tokens, not grammar notation.

Semantic restrictions are described separately from purely syntactic production rules.

# Source file

~~~text
sourceFile
  := moduleDeclaration?
     importDeclaration*
     topLevelDeclaration*
     EOF
~~~

A source file may omit an explicit module declaration when its module identity can be inferred from its path.

Imports must appear before ordinary top-level declarations.

# Module declarations

~~~text
moduleDeclaration
  := 'module' moduleName ';'

moduleName
  := Identifier ('.' Identifier)*
~~~

Example:

~~~gnr
module app.controllers.user_controller;
~~~

# Imports

~~~text
importDeclaration
  := 'import' moduleName importAlias? ';'

importAlias
  := 'as' Identifier
~~~

Examples:

~~~gnr
import app.models.user;
import app.services.billing as Billing;
~~~

Imports are static, top-level compile-time dependencies.

# Top-level declarations

~~~text
topLevelDeclaration
  := modelDeclaration
   | controllerDeclaration
   | migrationDeclaration
   | middlewareDeclaration
   | policyDeclaration
   | eventDeclaration
   | listenerDeclaration
   | notificationDeclaration
   | mailDeclaration
   | functionDeclaration
   | routeStatement
~~~

Future language declarations such as `interface` and `enum` are reserved for later specifications and are not part of the initial stable grammar.

# Identifiers

Conceptually:

~~~text
Identifier
  := IdentifierStart IdentifierPart*

IdentifierStart
  := Letter | '_'

IdentifierPart
  := Letter | Digit | '_'
~~~

Identifiers are case-sensitive.

Recommended naming conventions:

~~~text
Types / declarations     PascalCase
functions / values       camelCase
module segments          lowercase_snake_case
~~~

Keywords cannot be used as ordinary identifiers unless a future escaping syntax explicitly permits it.

# Keywords

The core language reserves at least:

~~~text
module
import
as

model
controller
migration
middleware
policy
event
listener
notification
mail

function
async
await
public
inject

const
let

if
else
for
in
while
return
break
continue

true
false
null

bool
int
int64
uint64
float
double
decimal
string
void
~~~

Framework-specific grammar may reserve additional contextual words inside specific declarations.

# Whitespace

Whitespace separates tokens where necessary and is otherwise insignificant outside string literals.

Conceptually:

~~~text
Whitespace
  := ' '
   | '\t'
   | '\r'
   | '\n'
~~~

Formatting is not semantically significant.

# Comments

Line comments:

~~~gnr
// this is a comment
~~~

Block comments:

~~~gnr
/*
    this is a block comment
*/
~~~

Conceptually:

~~~text
lineComment
  := '//' charactersUntilLineEnd

blockComment
  := '/*' charactersUntilMatchingClose '*/'
~~~

Comments are lexical trivia and must not affect parser structure.

Nested block comments are not required for the initial language.

# Literals

~~~text
literal
  := nullLiteral
   | booleanLiteral
   | integerLiteral
   | decimalLiteral
   | stringLiteral
   | listLiteral
   | objectLiteral
~~~

# Null literal

~~~text
nullLiteral
  := 'null'
~~~

# Boolean literals

~~~text
booleanLiteral
  := 'true'
   | 'false'
~~~

# Integer literals

Conceptually:

~~~text
integerLiteral
  := Digit+
~~~

The initial grammar may support only decimal integer notation.

Hexadecimal, binary, digit separators, and numeric suffixes should not be added until their semantics are explicitly specified.

# Decimal / floating literals

Conceptually:

~~~text
decimalLiteral
  := Digit+ '.' Digit+
~~~

Exponent syntax may be added later when lexical and type semantics are explicitly defined.

# String literals

Both single and double quotes represent Gungnir strings:

~~~text
stringLiteral
  := singleQuotedString
   | doubleQuotedString
~~~

Examples:

~~~gnr
'users'
"users"
~~~

Single quotes do **not** represent a C++ character literal.

There is no normal application-level `char` literal in the initial Gungnir language.

# String escapes

At minimum, string literals should support predictable escapes such as:

~~~text
\\
\'
\"
\n
\r
\t
~~~

The lexer owns escape decoding.

Invalid or unterminated escapes must produce lexical diagnostics rather than leaking into generated C++ unchanged.

# Types

~~~text
type
  := primaryType optionalMarker?

primaryType
  := builtinType
   | qualifiedType
   | genericType

optionalMarker
  := '?'
~~~

# Built-in scalar types

~~~text
builtinType
  := 'bool'
   | 'int'
   | 'int64'
   | 'uint64'
   | 'float'
   | 'double'
   | 'decimal'
   | 'string'
   | 'void'
~~~

Framework types such as `Request`, `Response`, `Next`, `datetime`, and application-defined model/event types are parsed through `qualifiedType`.

# Named / qualified types

~~~text
qualifiedType
  := Identifier ('::' Identifier)*
~~~

Examples:

~~~text
User
Request
Billing::InvoiceService
~~~

Module aliases may participate in type qualification.

# Generic types

~~~text
genericType
  := qualifiedType '<' typeArgumentList '>'

typeArgumentList
  := type (',' type)*
~~~

Examples:

~~~text
List<string>
Map<string, int>
Collection<User>
Query<User>
Result<User, Error>
~~~

This syntax represents Gungnir semantic generic types, not raw C++ template syntax.

User-defined generic declarations are not part of the initial grammar.

# Optional types

~~~text
optionalType
  := primaryType '?'
~~~

Examples:

~~~text
User?
string?
int?
~~~

Nullability is a type-level feature.

# Function declarations

~~~text
functionDeclaration
  := asyncModifier?
     'function'
     type
     Identifier
     '(' parameterList? ')'
     block

asyncModifier
  := 'async'
~~~

Examples:

~~~gnr
function string fullName(string first, string last) {
    return first + ' ' + last;
}
~~~

~~~gnr
async function User loadUser(int id) {
    return await repository.find(id);
}
~~~

The declared return type is the logical Gungnir result type. Native coroutine wrappers are not written in source.

# Parameters

~~~text
parameterList
  := parameter (',' parameter)*

parameter
  := type Identifier defaultValue?

defaultValue
  := '=' expression
~~~

Example:

~~~gnr
function int pageSize(int size = 25) {
    return size;
}
~~~

Required parameters must precede parameters with defaults.

# Framework action parameters

Framework actions use the same `parameterList` production unless their declaration contract further restricts required parameter types or positions.

Examples include:

~~~text
public show(User user)
public handle(Request request, Next next)
~~~

# Blocks

~~~text
block
  := '{' statement* '}'
~~~

Blocks establish lexical scope.

# Statements

~~~text
statement
  := block
   | constBindingStatement
   | letBindingStatement
   | assignmentStatement
   | expressionStatement
   | ifStatement
   | forStatement
   | whileStatement
   | returnStatement
   | breakStatement
   | continueStatement
~~~

# Const bindings

~~~text
constBindingStatement
  := 'const' explicitType? Identifier '=' expression ';'

explicitType
  := type
~~~

Examples:

~~~gnr
const user = User::findOrFail(id);
const int limit = 25;
~~~

`const` bindings are immutable.

# Mutable bindings

~~~text
letBindingStatement
  := 'let' explicitType? Identifier '=' expression ';'
~~~

Examples:

~~~gnr
let page = 1;
let int attempts = 0;
~~~

Bare first assignment is not the canonical declaration syntax.

# Assignment statements

~~~text
assignmentStatement
  := assignmentTarget assignmentOperator expression ';'

assignmentTarget
  := nameExpression
   | memberExpression
   | subscriptExpression

assignmentOperator
  := '='
   | '+='
   | '-='
   | '*='
   | '/='
~~~

Simple `=` assignment is required.

Compound assignment may be implemented after the basic assignment semantics are stable, but it is reserved by this grammar.

Assignment is a statement, not a value-producing expression.

# Expression statements

~~~text
expressionStatement
  := expression ';'
~~~

Typical expression statements are calls with intentional side effects:

~~~gnr
user.save();
logger.info('saved');
~~~

# If statements

~~~text
ifStatement
  := 'if' '(' expression ')' block elseClause?

elseClause
  := 'else' (ifStatement | block)
~~~

Example:

~~~gnr
if (user == null) {
    return response(null, 404);
} else {
    return json(user);
}
~~~

Conditions must be boolean-compatible semantically.

# For statements

The canonical application loop is `for ... in`:

~~~text
forStatement
  := 'for'
     '('
     'const'
     Identifier
     'in'
     expression
     ')'
     block
~~~

Example:

~~~gnr
for (const user in users) {
    logger.info(user.email);
}
~~~

A C-style `for (init; condition; update)` loop is not part of the canonical initial language contract.

# While statements

~~~text
whileStatement
  := 'while' '(' expression ')' block
~~~

Example:

~~~gnr
while (attempts < 3) {
    attempts = attempts + 1;
}
~~~

# Return statements

~~~text
returnStatement
  := 'return' expression? ';'
~~~

Examples:

~~~gnr
return user;
return json(user);
return;
~~~

Semantic analysis validates the result against the enclosing function or framework-action contract.

# Break and continue

~~~text
breakStatement
  := 'break' ';'

continueStatement
  := 'continue' ';'
~~~

They are legal only inside loops.

# Expressions

~~~text
expression
  := logicalOrExpression
~~~

Assignment is intentionally excluded from the expression grammar.

# Logical OR

~~~text
logicalOrExpression
  := logicalAndExpression ('||' logicalAndExpression)*
~~~

# Logical AND

~~~text
logicalAndExpression
  := equalityExpression ('&&' equalityExpression)*
~~~

# Equality

~~~text
equalityExpression
  := comparisonExpression (('==' | '!=') comparisonExpression)*
~~~

# Comparison

~~~text
comparisonExpression
  := additiveExpression
     (('<' | '<=' | '>' | '>=') additiveExpression)*
~~~

# Addition

~~~text
additiveExpression
  := multiplicativeExpression
     (('+' | '-') multiplicativeExpression)*
~~~

# Multiplication

~~~text
multiplicativeExpression
  := unaryExpression
     (('*' | '/' | '%') unaryExpression)*
~~~

# Unary expressions

~~~text
unaryExpression
  := ('!' | '+' | '-' | 'await') unaryExpression
   | postfixExpression
~~~

`await` is semantically legal only in an async context.

# Postfix expressions

~~~text
postfixExpression
  := primaryExpression postfixSuffix*

postfixSuffix
  := memberSuffix
   | staticMemberSuffix
   | callSuffix
   | subscriptSuffix

memberSuffix
  := '.' Identifier

staticMemberSuffix
  := '::' Identifier

callSuffix
  := '(' argumentList? ')'

subscriptSuffix
  := '[' expression ']'
~~~

This grammar supports chains such as:

~~~gnr
User::where('active', true)
    .orderBy('name')
    .get()
    .pluck('name')
~~~

Each suffix is represented structurally in the AST.

# Primary expressions

~~~text
primaryExpression
  := literal
   | Identifier
   | groupedExpression
   | lambdaExpression
~~~

# Grouped expressions

~~~text
groupedExpression
  := '(' expression ')'
~~~

# Call arguments

~~~text
argumentList
  := argument (',' argument)*

argument
  := namedArgument
   | expression

namedArgument
  := Identifier ':' expression
~~~

Syntactic rule:

- positional arguments must precede named arguments.

Semantic rules validate argument names and defaults against the resolved callable.

# List literals

~~~text
listLiteral
  := '[' listElements? ']'

listElements
  := expression (',' expression)* ','?
~~~

Examples:

~~~gnr
[]
[1, 2, 3]
['mail', 'database']
~~~

A trailing comma is allowed in multiline-friendly collection syntax.

# Object literals

~~~text
objectLiteral
  := '{' objectEntries? '}'

objectEntries
  := objectEntry (',' objectEntry)* ','?

objectEntry
  := objectKey ':' expression

objectKey
  := stringLiteral
~~~

Canonical example:

~~~gnr
{
    'name': user.name,
    'active': true,
}
~~~

Identifier-style object keys are not required by the initial grammar.

String keys avoid ambiguity with blocks and named arguments.

# Lambda expressions

~~~text
lambdaExpression
  := '(' lambdaParameterList? ')' '=>' block

lambdaParameterList
  := lambdaParameter (',' lambdaParameter)*

lambdaParameter
  := Identifier
   | type Identifier
~~~

Examples:

~~~gnr
(user) => {
    return user.name;
}
~~~

~~~gnr
(User user) => {
    return user.name;
}
~~~

Lambda parameter/result types may be inferred from contextual callable types.

# Async lambdas

Async lambda syntax is reserved for later stabilization.

A possible form:

~~~text
async (item) => { ... }
~~~

must not be treated as stable until async callback typing is fully implemented.

# Operator precedence

Highest to lowest:

| Level | Syntax |
| --- | --- |
| 1 | grouping, member, static member, call, subscript |
| 2 | unary `!`, `+`, `-`, `await` |
| 3 | `*`, `/`, `%` |
| 4 | `+`, `-` |
| 5 | `<`, `<=`, `>`, `>=` |
| 6 | `==`, `!=` |
| 7 | `&&` |
| 8 | `||` |

Assignment remains outside the expression precedence table.

# Model declarations

A model contains persistence mapping metadata and relationship declarations only.

~~~text
modelDeclaration
  := 'model' Identifier '{' modelMember* '}'

modelMember
  := modelConfiguration
   | relationshipDeclaration
~~~

Models do not declare schema fields.

Schema fields belong to migrations.

# Model configuration

Canonical model configuration keys include:

~~~text
table
connection
primaryKey
incrementing
keyType
fillable
hidden
casts
timestamps
softDeletes
~~~

Grammar:

~~~text
modelConfiguration
  := modelConfigurationName '=' expression ';'

modelConfigurationName
  := 'table'
   | 'connection'
   | 'primaryKey'
   | 'incrementing'
   | 'keyType'
   | 'fillable'
   | 'hidden'
   | 'casts'
   | 'timestamps'
   | 'softDeletes'
~~~

Semantic analysis validates the expected value type for each key.

Example:

~~~gnr
model User {
    table = 'users';
    primaryKey = 'id';

    fillable = [
        'name',
        'email',
    ];

    timestamps = true;
}
~~~

# Model relationships

~~~text
relationshipDeclaration
  := Identifier '(' ')' block
~~~

The body is syntactically a block, but model semantic validation restricts it to a relationship declaration form.

Canonical example:

~~~gnr
posts() {
    return hasMany('posts');
}
~~~

The parser should represent relationship members as dedicated model AST nodes rather than generic arbitrary methods.

# Controller declarations

~~~text
controllerDeclaration
  := 'controller' Identifier '{' controllerMember* '}'

controllerMember
  := injectDeclaration
   | controllerAction

controllerAction
  := 'public' 'async'? Identifier
     '(' parameterList? ')'
     block
~~~

Example:

~~~gnr
controller UserController {
    inject UserService users;

    public async show(int id) {
        const user = await users.find(id);

        return json(user);
    }
}
~~~

Controller actions do not write an explicit return type.

Their semantic result contract is `Response`.

# Injection declarations

~~~text
injectDeclaration
  := 'inject' type Identifier ';'
~~~

Example:

~~~gnr
inject UserService users;
~~~

# Middleware declarations

~~~text
middlewareDeclaration
  := 'middleware' Identifier '{' middlewareMember* '}'

middlewareMember
  := injectDeclaration
   | middlewareHandle

middlewareHandle
  := 'public' 'async'? 'handle'
     '(' parameterList ')'
     block
~~~

Semantic validation requires the canonical parameters to be compatible with:

~~~text
Request request
Next next
~~~

and requires exactly one `handle`.

# Migration declarations

~~~text
migrationDeclaration
  := 'migration' Identifier '{' migrationMember* '}'

migrationMember
  := migrationUp
   | migrationDown

migrationUp
  := 'up' '(' ')' block

migrationDown
  := 'down' '(' ')' block
~~~

A migration should define one `up()` and one `down()`.

The bodies contain ordinary statements and framework schema operations, which semantic analysis recognizes as migration/schema operations.

Example:

~~~gnr
migration CreateUsersTable {
    up() {
        Table::create('users', (table) => {
            table.id();
            table.string('name');
            table.timestamps();
        });
    }

    down() {
        Table::dropIfExists('users');
    }
}
~~~

# Policy declarations

~~~text
policyDeclaration
  := 'policy' Identifier '{' policyMember* '}'

policyMember
  := injectDeclaration
   | policyAction

policyAction
  := 'public' Identifier
     '(' parameterList? ')'
     block
~~~

Policy actions have an implicit authorization-decision result contract.

Example:

~~~gnr
policy PostPolicy {
    public update(User user, Post post) {
        return user.id == post.user_id;
    }
}
~~~

# Event declarations

Events are data-only declarations.

~~~text
eventDeclaration
  := 'event' Identifier '{' eventField* '}'

eventField
  := type Identifier ';'
~~~

Example:

~~~gnr
event UserRegistered {
    User user;
    datetime registeredAt;
}
~~~

Methods are not part of event grammar.

# Listener declarations

~~~text
listenerDeclaration
  := 'listener' Identifier '{' listenerMember* '}'

listenerMember
  := injectDeclaration
   | listenerHandle

listenerHandle
  := 'public' 'async'? 'handle'
     '(' parameterList ')'
     block
~~~

Semantic validation requires exactly one event parameter and one handle method in the initial contract.

Example:

~~~gnr
listener SendWelcomeNotification {
    public handle(UserRegistered event) {
        Notification::send(
            event.user,
            WelcomeNotification()
        );
    }
}
~~~

# Notification declarations

~~~text
notificationDeclaration
  := 'notification' Identifier '{' notificationMember* '}'

notificationMember
  := notificationField
   | injectDeclaration
   | notificationAction

notificationField
  := type Identifier ';'

notificationAction
  := 'public' Identifier
     '(' parameterList? ')'
     block
~~~

Recognized semantic actions include:

~~~text
via
mail
database
sms
push
~~~

The grammar does not hard-code every possible notification channel. Semantic analysis resolves supported channel methods.

# Mail declarations

~~~text
mailDeclaration
  := 'mail' Identifier '{' mailMember* '}'

mailMember
  := mailField
   | injectDeclaration
   | mailAction

mailField
  := type Identifier ';'

mailAction
  := 'public' Identifier
     '(' parameterList? ')'
     block
~~~

Recognized semantic mail actions include:

~~~text
subject
content
text
from
replyTo
attachments
headers
~~~

# Route statements

Route declarations use normal expression syntax but are classified semantically as route declarations when they begin with the framework `Route` symbol.

Syntactically:

~~~text
routeStatement
  := routeExpression ';'

routeExpression
  := routeBase routeModifier*

routeBase
  := 'Route' '::' Identifier
     '(' argumentList? ')'

routeModifier
  := '.' Identifier '(' argumentList? ')'
~~~

Examples:

~~~gnr
Route::get('/users', UserController::index);
~~~

~~~gnr
Route::get('/users/{user}', UserController::show)
    .middleware(AuthMiddleware)
    .whereNumber('user')
    .name('users.show');
~~~

Route groups may accept lambda expressions:

~~~gnr
Route::prefix('/admin')
    .middleware(AuthMiddleware)
    .group(() => {
        Route::get('/users', AdminUserController::index);
    });
~~~

The parser should preserve the call chain structurally.

The semantic routing pass validates methods, handlers, route names, middleware, constraints, resources, groups, and model binding.

# Static action references

Expressions such as:

~~~gnr
UserController::index
~~~

are valid static/member-reference expressions when used as route handlers.

They are not ordinary C++ member pointers at the source-language level.

Semantic resolution identifies:

~~~text
controller = UserController
action     = index
~~~

and lowering chooses the native representation.

# Framework configuration lambdas

Framework APIs may use lambdas:

~~~gnr
Table::create('users', (table) => {
    table.id();
    table.string('name');
});
~~~

The callback parameter type is supplied by the resolved framework API.

# Named construction / data construction

Several newer framework documents use construction syntax such as:

~~~gnr
WelcomeMail(
    user: user
)
~~~

or:

~~~gnr
UserRegistered(
    user: user
)
~~~

Syntactically these are ordinary call expressions with named arguments.

Semantic analysis determines whether the callee is:

- a function;
- a constructible event;
- a notification;
- a mail value;
- another declared application type.

No separate `new` keyword is required.

# Framework helper calls

Calls such as:

~~~gnr
json(user)
view('users/show', {'user': user})
redirect('/login')
response(null, 204)
event(UserRegistered(user: user))
authorize('update', post)
~~~

are ordinary call expressions syntactically.

Their framework-specific behavior is attached during symbol and semantic resolution.

# Unsupported C++ syntax

The canonical Gungnir grammar does not include normal application syntax for:

~~~text
class
struct
namespace
template
typename

public:
private:
protected:

T*
T&
T&&

new
delete

co_await
co_return

#include
#define
#if

goto
switch

reinterpret_cast
static_cast
dynamic_cast
const_cast
~~~

Native interoperability may use explicit compiler escape/binding mechanisms, but unsupported C++ syntax is not part of the normal `.gnr` grammar.

# Reserved future constructs

The following concepts may be reserved for future Gungnir syntax:

~~~text
interface
enum
match
try
catch
throw
defer
queue
job
trait
~~~

Reservation does not mean these constructs are currently valid.

# Error recovery

The parser should recover from common syntax errors so one mistake does not suppress the rest of the file.

Useful synchronization tokens include:

~~~text
;
}
top-level declaration keywords
public
inject
~~~

Examples of recoverable errors:

- missing semicolon;
- missing closing parenthesis;
- missing closing brace;
- missing declaration name;
- malformed parameter;
- malformed argument list;
- malformed collection entry.

Parser recovery must preserve source spans and avoid generating fabricated semantic facts that later phases treat as valid.

# Source spans

Every syntax node should retain:

~~~text
file
start offset / line / column
end offset / line / column
~~~

Diagnostics and generated C++ source mapping depend on precise spans.

# Syntax AST boundary

Parsing answers:

~~~text
What syntax did the developer write?
~~~

It should not prematurely answer every framework semantic question.

Examples:

~~~text
User::findOrFail(id)
Route::get(...)
hasMany('posts')
request.validate(...)
~~~

are parsed structurally first.

Later phases resolve:

- symbols;
- callable identities;
- types;
- framework meaning;
- authorization/ORM/routing contracts.

# No raw-source rediscovery

Once supported syntax has been parsed into AST nodes, later phases must not rediscover the same structure by scanning source text.

This applies especially to:

- framework declarations;
- controller actions;
- model relationships;
- route declarations;
- validation calls;
- async/await;
- ORM calls;
- injection declarations;
- view data;
- event/listener links.

Raw/source-preserving nodes should be restricted to explicit native interoperability or temporarily unsupported syntax, not used as the normal representation.

# Parser architecture

Recommended parser layers:

~~~text
parseSourceFile
  parseModuleDeclaration?
  parseImportDeclaration*
  parseTopLevelDeclaration*

parseTopLevelDeclaration
  parseModel
  parseController
  parseMigration
  parseMiddleware
  parsePolicy
  parseEvent
  parseListener
  parseNotification
  parseMail
  parseFunction
  parseRouteStatement

parseBlock
  parseStatement*

parseStatement
  parseBinding
  parseAssignment
  parseIf
  parseFor
  parseWhile
  parseReturn
  parseBreak
  parseContinue
  parseExpressionStatement

parseExpression
  precedence / Pratt parser
~~~

A Pratt parser or equivalent precedence parser is appropriate for expression parsing.

# Lexer/parser boundary

The lexer owns:

- keywords;
- identifiers;
- numbers;
- strings;
- punctuation;
- operators;
- comments/trivia;
- source positions.

The parser owns:

- declaration structure;
- types;
- parameter lists;
- blocks;
- statements;
- expression precedence;
- calls/chains;
- literals/collections;
- framework declaration bodies.

The lexer must not decide whether `User` is a model or whether `get` is an ORM method.

Those are semantic questions.

# Parser/semantic boundary

The parser may know that:

~~~text
controller UserController { ... }
~~~

is a controller declaration because `controller` is grammar.

The parser should not need database knowledge to decide whether:

~~~gnr
User::where('active', true)
~~~

is valid.

That belongs to symbol/type/framework semantic analysis.

# Complete example

~~~gnr
module app.controllers.user_controller;

import app.models.user;
import app.services.user_service;

controller UserController {
    inject UserService users;

    public index(Request request) {
        const active = request.boolean('active');

        const results = User::where('active', active)
            .orderBy('name')
            .paginate(25);

        return json(results);
    }

    public async show(User user) {
        const profile = await users.profile(user.id);

        if (profile == null) {
            return response(null, 404);
        }

        return json({
            'user': user,
            'profile': profile,
        });
    }
}

Route::get('/users', UserController::index)
    .name('users.index');

Route::get('/users/{user}', UserController::show)
    .name('users.show');
~~~

The parser should be able to represent every supported construct above without source rescanning.

# Canonical grammar summary

~~~text
sourceFile
  module?
  imports*
  declarations*

declaration
  model
  controller
  migration
  middleware
  policy
  event
  listener
  notification
  mail
  function
  route

block
  statements*

statement
  const
  let
  assignment
  expression
  if
  for-in
  while
  return
  break
  continue

expression precedence
  ||
  &&
  == !=
  < <= > >=
  + -
  * / %
  ! + - await
  postfix/member/call/subscript
  primary

primary
  literal
  identifier
  grouping
  lambda

literal
  null
  bool
  integer
  decimal
  string
  list
  object
~~~

# Relationship to specialized specifications

This file defines syntax.

Specialized documents define semantic contracts:

~~~text
language-types.md  type semantics
expressions.md     expression semantics
statements.md      statement/control-flow semantics
functions.md       function/callable semantics
async.md           async/await semantics
modules.md         module resolution
model.md           model semantics
migration.md       schema semantics
controller.md      controller semantics
routing.md         routing semantics
middleware.md      middleware semantics
request.md         request API
response.md        response API
validation.md      validation semantics
collection.md      collection semantics
authentication.md  authentication semantics
policy.md          policy semantics
event.md           event semantics
listener.md        listener semantics
notification.md    notification semantics
mail.md            mail semantics
view.md            template/view semantics
~~~

# Design rule

The grammar must remain smaller than C++ and more specific to web application development.

The compiler architecture should follow:

~~~text
source
  -> lexer
  -> parser
  -> syntax AST
  -> symbol resolution
  -> semantic/type analysis
  -> validated AST
  -> framework lowering
  -> C++23
~~~

The parser defines the language structure once.

Later phases operate on that structure rather than interpreting source text again.
