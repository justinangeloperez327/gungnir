# Response

A Gungnir response represents the HTTP result returned by a controller action, middleware, or inline route handler.

Controller actions and middleware have an implicit response contract, so application code normally returns response helpers directly:

```gnr
return json(user);
```

```gnr
return view('users/show', {
    'user': user
});
```

```gnr
return redirect('/login');
```

Application code does not need to declare `Response` in every action signature.

# Response responsibility

A response defines:

```text
status code
body
content type
headers
cookies
redirect location
file/download metadata
```

The server runtime is responsible for encoding and sending the response over the network.

# Basic response

Use `response()` for a general response:

```gnr
return response('OK');
```

Specify a status:

```gnr
return response('Created', 201);
```

An empty response:

```gnr
return response(null, 204);
```

# Text responses

Use `text()` for plain text:

```gnr
return text('Hello');
```

With a status:

```gnr
return text('Not Found', 404);
```

The framework should set an appropriate text content type automatically.

# JSON responses

Use `json()` for JSON:

```gnr
return json(user);
```

With an explicit status:

```gnr
return json(user, 201);
```

Object response:

```gnr
return json({
    'status': 'ok',
    'user': user
});
```

Collections and loaded relationships should serialize according to the model/ORM serialization contract.

# View responses

Use `view()` to render a template:

```gnr
return view('users/index', {
    'users': users
});
```

A view may omit data:

```gnr
return view('home');
```

Template syntax and rendering belong in `view.md`.

# Redirect responses

Redirect to a URL:

```gnr
return redirect('/login');
```

Redirect to a named route:

```gnr
return redirect()
    .route('users.show', {
        'user': user.id
    });
```

Redirect back:

```gnr
return redirect().back();
```

Redirect semantics, status codes, and URL generation should follow the routing contract.

# Response status

Set or replace the status code:

```gnr
return json(data)
    .status(202);
```

Common explicit status examples:

```gnr
return response(null, 204);
return json(error, 400);
return json(error, 401);
return json(error, 403);
return json(error, 404);
return json(result, 201);
```

Gungnir should not require symbolic HTTP status enums in normal application code, though the runtime may expose them internally.

# Headers

Add a response header:

```gnr
return json(data)
    .header('X-Request-ID', request.id());
```

Multiple headers:

```gnr
return json(data)
    .headers({
        'Cache-Control': 'no-store',
        'X-Frame-Options': 'DENY'
    });
```

Header names should be handled according to HTTP case-insensitive semantics.

The framework must reject invalid header names or values that could create response-splitting vulnerabilities.

# Cookies

Attach a cookie:

```gnr
return response('OK')
    .cookie('locale', 'en');
```

Cookie options should be structured:

```gnr
return response('OK')
    .cookie('session', token, {
        'httpOnly': true,
        'secure': true,
        'sameSite': 'lax'
    });
```

Cookie deletion:

```gnr
return response('OK')
    .withoutCookie('session');
```

Cookie signing, encryption, expiration, path, domain, and SameSite rules belong to the HTTP/session layer.

# File responses

Return a file:

```gnr
return file('/path/to/report.pdf');
```

The runtime should determine or validate the content type safely.

# Downloads

Force a file download:

```gnr
return download('/path/to/report.pdf');
```

Provide a download name:

```gnr
return download(
    '/path/to/report.pdf',
    'monthly-report.pdf'
);
```

Application code should not manually construct `Content-Disposition` headers when the download helper can do so safely.

# No-content response

Use an explicit empty response for operations with no body:

```gnr
return response(null, 204);
```

A dedicated helper may also be supported:

```gnr
return noContent();
```

If `noContent()` is implemented, it should be equivalent to a valid HTTP 204 response and must not emit a body.

# Created response

For a newly created resource:

```gnr
return json(user, 201);
```

A dedicated helper may later be added if it improves readability without duplicating the core response API.

# Response mutation

Response helpers return response values that may be refined before return:

```gnr
const response = json(data);

response.status(202);
response.header('X-Job-ID', job.id);

return response;
```

Or fluently:

```gnr
return json(data)
    .status(202)
    .header('X-Job-ID', job.id);
```

The response API should define whether response values are mutable handles or persistent builder values internally. Application syntax should remain stable either way.

# Middleware response handling

Middleware may inspect or modify a downstream response:

```gnr
middleware SecurityHeadersMiddleware {
    public handle(Request request, Next next) {
        const response = next(request);

        response.header(
            'X-Content-Type-Options',
            'nosniff'
        );

        return response;
    }
}
```

Async middleware:

```gnr
public async handle(Request request, Next next) {
    const response = await next(request);

    response.header('X-Request-ID', request.id());

    return response;
}
```

# Controller response contract

Controller actions do not write an explicit return type:

```gnr
public show(User user) {
    return json(user);
}
```

The compiler knows the action is response-producing.

This is invalid unless raw-value auto-conversion is explicitly supported:

```gnr
public show(User user) {
    return user;
}
```

Gungnir should prefer explicit response intent:

```gnr
return json(user);
return text('OK');
return view('users/show', {'user': user});
return redirect('/users');
```

# Inline route responses

Inline route handlers use the same response contract:

```gnr
Route::get('/health', () => {
    return json({
        'status': 'ok'
    });
});
```

# Response serialization

`json()` should serialize supported values such as:

```text
null
bool
numbers
strings
lists
objects/maps
models
collections
pagination results
supported DTO/application values
```

Model serialization must honor model metadata such as:

```text
hidden
casts
loaded relationships
```

The ORM/model documentation defines those semantics.

# JSON errors

JSON serialization failures should become structured framework errors.

The runtime must not emit partially generated JSON responses silently.

# Content negotiation

Helpers should set their expected content type automatically:

```text
text()      -> text response
json()      -> JSON response
view()      -> HTML response
file()      -> file content type
download()  -> download/file response
```

Applications may still override headers when appropriate, but invalid combinations should be diagnosed where possible.

# Streaming

Streaming responses may be added as a separate advanced contract later.

If added, they should use an explicit API rather than overloading normal string/body responses.

Possible future concept:

```gnr
return stream((writer) => {
    // ...
});
```

Streaming must account for backpressure, cancellation, and coroutine lifetime correctly before being treated as stable.

# Response security

Response helpers should handle common protocol details safely.

Examples include:

- header validation;
- safe JSON encoding;
- safe cookie serialization;
- valid redirect locations;
- correct content-length/chunking behavior;
- correct download disposition formatting.

Output escaping for HTML belongs to the template/view layer rather than the generic response object.

# What does not belong in Response

Response objects should not contain:

- database queries;
- business workflows;
- authentication logic;
- authorization policy logic;
- model persistence;
- application service discovery;
- arbitrary global state.

A response is the HTTP result of application work.

# Response API reference

| API | Purpose |
| --- | --- |
| `response()` | General HTTP response |
| `text()` | Plain-text response |
| `json()` | JSON response |
| `view()` | Rendered view response |
| `redirect()` | Redirect response |
| `file()` | File response |
| `download()` | Download response |
| `noContent()` | Optional 204 helper |
| `.status()` | Set status |
| `.header()` | Add/set one header |
| `.headers()` | Add/set multiple headers |
| `.cookie()` | Attach cookie |
| `.withoutCookie()` | Remove cookie |

# Response AST and typing

Response helpers should be resolved through normal expression/call AST nodes.

Conceptually:

```text
json(user)
  receiver/helper    json
  argument           user
  resultType         Response

view(...)
  resultType         Response

redirect(...)
  resultType         RedirectResponse
  compatibleWith     Response
```

Controller and middleware semantic analysis should require every reachable return path to be response-compatible.

# Semantic validation

The compiler should validate at least:

- controller and middleware returns are response-compatible;
- response helper calls use valid argument shapes;
- status codes are valid integer values where compile-time known;
- header calls use valid argument types;
- cookie options use supported keys and value types;
- route redirects use valid route parameters where statically resolvable;
- file/download helper arguments are structurally valid;
- async actions do not accidentally return unresolved async values instead of responses.

Runtime-only values remain runtime validated where static proof is not possible.

# Compiler contract

This source:

```gnr
public store(Request request) {
    const data = request.validate({
        'name': 'required|string'
    });

    const user = User::create(data);

    return json(user, 201)
        .header('X-Resource', 'user');
}
```

should conceptually pass through:

```text
source
  -> parser
  -> response call expressions
  -> helper resolution
  -> response type checking
  -> validated AST
  -> response lowering
  -> C++23 generation
```

Before native generation, the compiler should already know:

```text
helper          json
payload         User
status          201
result          Response
header          X-Resource
```

The transpiler must not detect response helpers through raw source-string rewriting.

# Generated C++ boundary

A source expression such as:

```gnr
return json(user, 201);
```

may lower to native response builders, serializers, HTTP body types, or server response objects.

Application code should not need to manage:

```text
native HTTP response classes
header container internals
JSON serializer templates
buffer ownership
socket write state
content-length mechanics
coroutine transport state
```

# Naming convention

Public response APIs use familiar camelCase naming:

```text
noContent
withoutCookie
```

Most response helpers remain single-word:

```text
response
text
json
view
redirect
file
download
status
header
headers
cookie
```

# Design rule

The response contract is intentionally focused:

```text
Response = HTTP status + body + headers + cookies + response metadata
```

Controllers and middleware produce responses.

The server runtime sends them.

Those responsibilities remain separate.
