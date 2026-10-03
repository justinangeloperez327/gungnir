# Request

> **Gungnir 1.x design specification.** This document describes intended long-term architecture beyond the frozen `1.0.0-rc.1` contract. Examples and requirements may exceed the current implementation. See the [current implementation guide](../request.md) before using an API.


## 1.0 RC alignment

The shipped baseline is `1.0.0-rc.1`. The 1.0 contracts are frozen; this design document may describe additive 1.x evolution or future-major work, but current implementation guides and executable tests remain authoritative.

A Gungnir `Request` represents the current HTTP request.

Controllers and middleware receive the request through framework injection:

```gnr
public store(Request request) {
    // ...
}
```

Application code does not construct the current request manually.

The request API provides access to:

- input data;
- query-string values;
- route parameters;
- headers;
- cookies;
- uploaded files;
- JSON and body content;
- HTTP method and URI information;
- client/request metadata;
- validation.

Gungnir follows familiar Laravel-style request naming where the behavior maps cleanly to the framework.

# Request injection

A controller action may request the current request:

```gnr
controller UserController {
    public store(Request request) {
        const name = request.input('name');

        return json({
            'name': name
        });
    }
}
```

Middleware receives the same request object:

```gnr
middleware AuthMiddleware {
    public handle(Request request, Next next) {
        // inspect request

        return next(request);
    }
}
```

`Request` is a framework-provided request-scoped value.

# All input

Use `all()` to retrieve all parsed application input:

```gnr
const data = request.all();
```

This may include parsed body input and query input according to the request parsing contract.

Uploaded files remain file values rather than being flattened into arbitrary strings.

# Input values

Use `input()` to retrieve application input:

```gnr
const name = request.input('name');
```

A default value may be supplied:

```gnr
const status = request.input('status', 'active');
```

Nested input may use dotted keys:

```gnr
const city = request.input('address.city');
```

The exact source precedence between body and query data must be deterministic and documented by the runtime. Applications that need a specific source should use `query()`, `json()`, or another source-specific accessor.

# Query-string input

Use `query()` for URL query parameters.

For:

```text
/users?page=2&active=true
```

access values with:

```gnr
const page = request.query('page');
const active = request.query('active');
```

A default may be supplied:

```gnr
const page = request.query('page', 1);
```

Calling `query()` without a key returns the parsed query collection:

```gnr
const query = request.query();
```

Query parameters do not participate in route-path matching.

# Route parameters

Use `route()` to access a route parameter directly:

```gnr
const id = request.route('id');
```

For:

```gnr
Route::get('/users/{id}', UserController::show);
```

the route value is available as:

```gnr
request.route('id');
```

Normally controller action parameters are preferable when the route parameter is part of the action contract:

```gnr
public show(int id) {
    // ...
}
```

For model-bound routes:

```gnr
Route::get('/users/{user}', UserController::show);
```

prefer:

```gnr
public show(User user) {
    return json(user);
}
```

rather than manually re-resolving the model through the request.

# Headers

Use `header()` to read request headers:

```gnr
const authorization = request.header('authorization');
```

A default may be supplied:

```gnr
const version = request.header('x-api-version', '1');
```

Header lookup should be case-insensitive according to HTTP semantics.

Check for a header with:

```gnr
if (request.hasHeader('x-request-id')) {
    // ...
}
```

# Bearer token

Use a dedicated helper for bearer authentication tokens:

```gnr
const token = request.bearerToken();
```

Authentication semantics belong in `authentication.md`; the request only exposes the parsed credential value.

# Cookies

Use `cookie()` to retrieve a request cookie:

```gnr
const locale = request.cookie('locale');
```

A default may be supplied:

```gnr
const locale = request.cookie('locale', 'en');
```

Cookie signing, encryption, and response-cookie creation belong to the HTTP/session response layers.

# Uploaded files

Use `file()` to retrieve an uploaded file:

```gnr
const avatar = request.file('avatar');
```

Check whether a file was supplied:

```gnr
if (request.hasFile('avatar')) {
    const avatar = request.file('avatar');
}
```

File uploads should expose structured file metadata and temporary-storage access rather than raw multipart internals.

A file value may provide concepts such as:

```text
name
originalName
contentType
size
temporaryPath
extension
```

The exact uploaded-file API belongs in dedicated file/storage documentation.

# JSON requests

Use `json()` when the request body is JSON.

Retrieve the parsed JSON body:

```gnr
const data = request.json();
```

Retrieve one value:

```gnr
const name = request.json('name');
```

Nested values may use dotted keys:

```gnr
const city = request.json('address.city');
```

Malformed JSON should produce a clear request/body parsing error rather than silently becoming an empty object.

# Raw request body

Use `body()` when raw request content is genuinely required:

```gnr
const body = request.body();
```

This is useful for cases such as:

- webhook signature verification;
- custom media types;
- raw payload processing.

Normal form or JSON requests should use structured accessors instead.

# Content type

Inspect the request content type when needed:

```gnr
const contentType = request.contentType();
```

Convenience checks may include:

```gnr
request.isJson();
request.acceptsJson();
request.expectsJson();
```

These helpers describe HTTP content negotiation and parsing expectations, not application validation.

# HTTP method

Retrieve the incoming HTTP method:

```gnr
const method = request.method();
```

Check a method:

```gnr
if (request.isMethod('post')) {
    // ...
}
```

The method value should use a normalized representation.

# URI and path

Retrieve the request path:

```gnr
const path = request.path();
```

Retrieve the full request URL:

```gnr
const url = request.url();
```

Retrieve a URL including the query string:

```gnr
const fullUrl = request.fullUrl();
```

Route matching itself belongs in `routing.md`.

# Host and scheme

Where required:

```gnr
const host = request.host();
const scheme = request.scheme();
```

Trusted-proxy handling must be applied before values such as client IP, host, or scheme are trusted from forwarded headers.

# Client IP

Retrieve the resolved client address:

```gnr
const ip = request.ip();
```

The runtime must apply configured trusted-proxy rules rather than blindly trusting forwarding headers.

# User agent

```gnr
const userAgent = request.userAgent();
```

The user-agent value is untrusted request input.

# Input presence

## has

Check whether an input key exists:

```gnr
if (request.has('email')) {
    // ...
}
```

Multiple keys:

```gnr
if (request.has([
    'name',
    'email'
])) {
    // ...
}
```

## missing

```gnr
if (request.missing('email')) {
    // ...
}
```

## filled

Check whether a key exists and contains a non-empty value:

```gnr
if (request.filled('name')) {
    // ...
}
```

## anyFilled

```gnr
if (request.anyFilled([
    'phone',
    'mobile'
])) {
    // ...
}
```

Presence helpers must distinguish missing values from valid false-like values such as `false` or `0`.

# Selecting input

## only

Retrieve only selected input:

```gnr
const data = request.only([
    'name',
    'email'
]);
```

This is useful for explicit update surfaces:

```gnr
user.update(request.only([
    'name',
    'email'
]));
```

`only()` does not replace model `fillable` protection. The controller controls which request data is selected; the model independently controls which attributes may be mass assigned.

## except

Retrieve all input except selected keys:

```gnr
const data = request.except([
    'password_confirmation'
]);
```

For security-sensitive operations, `only()` is generally clearer than broad exclusion.

# String input

Where a caller explicitly expects textual input, a convenience accessor may be used:

```gnr
const name = request.string('name');
```

This should produce a predictable string representation or a typed request error when conversion is not valid.

# Integer input

```gnr
const page = request.integer('page', 1);
```

Typed input helpers should perform explicit conversion rules rather than relying on C++ implicit conversions.

# Boolean input

```gnr
const active = request.boolean('active');
```

The framework should define accepted boolean representations consistently, such as:

```text
true
false
1
0
"1"
"0"
"true"
"false"
```

Exact accepted textual forms belong to the request conversion contract and must remain deterministic.

# Array/list input

```gnr
const tags = request.array('tags');
```

If the value is not structurally an array/list, the typed accessor should not silently reinterpret unrelated data.

# Validation

A request may validate input directly:

```gnr
const data = request.validate({
    'name': 'required|string',
    'email': 'required|email',
    'age': 'nullable|integer'
});
```

The result contains validated data suitable for application use:

```gnr
const user = User::create(data);
```

Validation failure should stop normal action execution and enter the framework validation-error response flow.

The complete rule language, nested validation, custom messages, database-backed rules, and error representation belong in `validation.md`.

# Validated request example

```gnr
controller UserController {
    public store(Request request) {
        const data = request.validate({
            'name': 'required|string',
            'email': 'required|email',
            'password': 'required|string'
        });

        const user = User::create(data);

        return json(user, 201);
    }
}
```

# Request data and model fillable

Request filtering and model mass-assignment protection are separate concerns.

Controller:

```gnr
const data = request.only([
    'name',
    'email'
]);
```

Model:

```gnr
model User {
    fillable = [
        'name',
        'email'
    ];
}
```

Both may apply.

The request decides which incoming values the action chooses to use.

The model decides which values may be mass assigned to persistence.

# Request immutability

The incoming request should be treated as immutable application input.

Accessors such as:

```gnr
request.input(...)
request.query(...)
request.header(...)
request.route(...)
```

read request state.

Normal application code should not mutate the original HTTP request body, headers, or route parameters in place.

Middleware that needs derived request context should use explicitly supported request-context/attribute mechanisms rather than arbitrary mutation.

# Request-scoped attributes

Framework middleware may need to attach request-scoped application context such as authentication or tracing information.

A structured attribute API may expose:

```gnr
request.attribute('requestId');
```

and framework-controlled setting through the middleware/runtime layer.

Application code should prefer specialized framework APIs such as `Auth::user()` when a domain-specific abstraction exists.

Arbitrary request attributes must not become a substitute for typed dependency injection.

# Sessions

If session middleware is active, request-scoped session access may be exposed through:

```gnr
const session = request.session();
```

Session semantics, persistence, flash data, and regeneration belong in dedicated session documentation.

A request without the required session middleware should not silently fabricate a persistent session.

# Authentication context

The request may expose framework authentication context indirectly, but authentication behavior belongs in `authentication.md`.

Preferred application usage should remain explicit:

```gnr
const user = Auth::user();
```

rather than making every authentication operation a generic request accessor.

# Locale

Where localization support is enabled:

```gnr
const locale = request.locale();
```

Locale resolution may depend on application middleware and configuration.

# Request IDs and tracing

The runtime may expose request-scoped observability metadata:

```gnr
const requestId = request.id();
```

Generation and propagation belong to the HTTP/observability runtime.

Incoming request identifiers are untrusted unless accepted through configured forwarding/tracing rules.

# Async safety

A `Request` may be used inside an async controller action:

```gnr
public async store(Request request) {
    const data = request.validate({
        'name': 'required|string'
    });

    const result = await service.create(data);

    return json(result);
}
```

The runtime must guarantee that request data referenced across coroutine suspension remains valid for the lifetime of the action.

Application syntax should not expose native pointer/reference lifetime management.

# Body limits

Request parsing must honor configured limits for:

- total request body size;
- multipart/form-data size;
- individual uploaded-file size;
- header size/count;
- parsed field count where applicable.

Limit configuration belongs in application/server configuration.

Exceeding a configured limit should produce a defined HTTP/request error rather than unbounded allocation.

# Multipart requests

Multipart parsing should expose normal input fields and structured uploaded files.

For example:

```gnr
const title = request.input('title');
const file = request.file('document');
```

Controllers should not parse multipart boundaries manually.

# Security rules

Request data is untrusted input.

The request API must not imply that values are safe merely because they were parsed successfully.

Important distinctions:

```text
parsing      -> converts HTTP representation into request values
validation   -> verifies application requirements
authorization -> verifies whether the actor may perform the action
escaping     -> depends on the output/context
ORM binding  -> protects values used in normal database queries
```

These responsibilities must remain separate.

# What does not belong in Request

`Request` should not become a general application service container.

Do not place unrelated responsibilities on it such as:

```text
database queries
ORM persistence
mail sending
business workflows
authorization policy implementation
event dispatch logic
application-service location
global mutable state
```

The request represents incoming HTTP state and request-scoped HTTP context.

# Complete controller example

```gnr
controller UserController {
    public index(Request request) {
        const active = request.boolean('active');

        const users = User::query()
            .when(active, (query) => {
                query.where('active', true);
            })
            .orderBy('name')
            .paginate(25);

        return json(users);
    }

    public store(Request request) {
        const data = request.validate({
            'name': 'required|string',
            'email': 'required|email',
            'password': 'required|string'
        });

        const user = User::create(data);

        return json(user, 201);
    }

    public update(Request request, User user) {
        const data = request.validate({
            'name': 'required|string',
            'email': 'required|email'
        });

        user.update(data);

        return json(user);
    }
}
```

# Request API reference

The intended core request surface includes:

| API | Purpose |
| --- | --- |
| `all()` | All parsed application input |
| `input()` | Body/query application input |
| `query()` | Query-string input |
| `route()` | Route parameter |
| `header()` | Request header |
| `hasHeader()` | Header presence |
| `bearerToken()` | Bearer credential |
| `cookie()` | Request cookie |
| `file()` | Uploaded file |
| `hasFile()` | Uploaded-file presence |
| `json()` | JSON body/value |
| `body()` | Raw request body |
| `contentType()` | Content type |
| `isJson()` | JSON content check |
| `acceptsJson()` | Accept-header JSON check |
| `expectsJson()` | Framework JSON-response expectation |
| `method()` | HTTP method |
| `isMethod()` | HTTP method check |
| `path()` | Request path |
| `url()` | URL |
| `fullUrl()` | URL including query string |
| `host()` | Resolved host |
| `scheme()` | Resolved scheme |
| `ip()` | Resolved client IP |
| `userAgent()` | User-Agent header |
| `has()` | Input-key presence |
| `missing()` | Missing input check |
| `filled()` | Non-empty input check |
| `anyFilled()` | Any non-empty selected input |
| `only()` | Select input keys |
| `except()` | Exclude input keys |
| `string()` | Typed string input |
| `integer()` | Typed integer input |
| `boolean()` | Typed boolean input |
| `array()` | Typed array/list input |
| `validate()` | Validate request data |
| `session()` | Request session when enabled |
| `attribute()` | Request-scoped framework attribute |
| `locale()` | Resolved locale |
| `id()` | Request/trace identifier |

# Request type model

The compiler should treat `Request` as a framework-provided type.

Conceptually:

```text
Request
  input
  query
  route parameters
  headers
  cookies
  files
  body
  request metadata
  request-scoped context
```

Request accessors should expose typed results rather than force application code to work with native C++ HTTP parser objects.

# Semantic validation

The semantic analyzer should validate at least:

- `Request` parameters are valid framework action/middleware parameters;
- request methods resolve to known request APIs;
- typed request accessor arguments are structurally valid;
- `only()` and `except()` receive valid key collections;
- validation calls use supported validation structures;
- file-specific APIs return file-compatible values;
- request values used across async suspension remain within the framework lifetime contract;
- native request implementation details are not required in application code.

Some checks, such as whether a particular input key exists at runtime, remain runtime concerns.

# Compiler contract

This source:

```gnr
public store(Request request) {
    const data = request.validate({
        'name': 'required|string',
        'email': 'required|email'
    });

    const user = User::create(data);

    return json(user, 201);
}
```

should conceptually pass through:

```text
source
  -> lexer
  -> parser
  -> ControllerAction AST
  -> Request parameter resolution
  -> request method call AST
  -> validation expression analysis
  -> typed/validated AST
  -> HTTP/request lowering
  -> C++23 generation
```

Before native generation, the compiler should already know:

```text
parameter          request
parameter type     Request

call               request.validate(...)
receiver type      Request
result             validated input object

call               User::create(data)
receiver           ModelSymbol(User)

return             json(...)
response contract  Response
```

The transpiler must not rediscover request operations by scanning raw source text.

# Generated C++ boundary

Gungnir source:

```gnr
const email = request.input('email');
```

may lower to native request accessors, parsed-value structures, optional values, or framework-specific C++23 types.

Those details are not part of the application language contract.

Application code should not need to manage:

```text
HTTP parser buffers
native header maps
multipart parser state
request-body ownership
string_view lifetimes
pointer/reference validity
coroutine request lifetimes
```

The runtime and generated C++ layer own those concerns.

# Naming convention

Public request APIs use camelCase where multiple words are required:

```text
hasHeader
bearerToken
hasFile
contentType
isJson
acceptsJson
expectsJson
isMethod
fullUrl
userAgent
anyFilled
```

Simple APIs remain single words:

```text
all
input
query
route
header
cookie
file
json
body
method
path
url
host
scheme
ip
has
missing
filled
only
except
string
integer
boolean
array
validate
session
locale
```

Native C++ naming is an implementation detail.

# Design rule

The request contract is intentionally focused:

```text
Request = incoming HTTP data + request-scoped HTTP context
```

Controllers coordinate request handling.

Validation determines whether input satisfies application rules.

Authentication identifies the actor.

Authorization determines what the actor may do.

The ORM handles persistence.

Those responsibilities remain separate.

