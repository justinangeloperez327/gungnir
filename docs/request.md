# Requests

The `Request` object represents the incoming HTTP request and provides access to headers, route parameters, query values, cookies, body data, uploaded files, sessions, authentication, and validated input.

## Reading input

```gnr
const name = request.input("name");
const search = request.query("search");
const id = request.parameter("id");
const agent = request.header("User-Agent");
```

Use `all`, `only`, and `except` when working with groups of input values.

```gnr
const input = request.all();
const selected = request.only(["name", "email"]);
const publicInput = request.except(["password"]);
const query = request.query();
```

These methods return `Map<string, string>`. Body values take precedence over query values. Use structured JSON or validation when types and nested values matter. Missing string input, headers, cookies, and route parameters return an empty string; use `has` or `hasParameter` to check whether an input or route parameter exists.

## JSON

JSON requests can be read as structured values:

```gnr
const payload = request.json();
```

Structured values preserve arrays, objects, numbers, booleans, strings, and null values.

Use `get` to read an optional object member. A missing key returns `null` as `Json?`; a present JSON null remains a `Json` value whose `isNull()` is true.

```gnr
const payload = request.json();
const name = payload.get("name");
if (name != null) {
    if (name.isString()) {
        const value = name.string();
    }
}
```

`isObject`, `isArray`, `isBoolean`, `isInteger`, `isNumber`, `isString`, and `isNull` inspect a value. `asObject()` returns `Map<string, Json>` and `asArray()` returns `List<Json>`; they require the matching value kind. `dump()` serializes a value as JSON. Indexing an object with `payload["name"]` requires the key to exist.

Requests with malformed or empty JSON bodies return HTTP 400 when read through `json()`. The error response does not include the submitted body.

URL-encoded form data is available through `request.form()` as `Map<string, string>`.

## Validation

```gnr
const data = request.validate({
    "email": "required|email",
    "name": "required|string|max:100"
});
```

Validation returns the selected validated data or raises a validation exception handled by the HTTP exception layer.

See [Validation](validation.md).

## Request metadata

Requests expose the HTTP method, target, path, content type, client information, headers, and cancellation state.

```gnr
const method = request.method();
const target = request.target();
const path = request.path();
const contentType = request.contentType();
const clientIp = request.clientIp();
const headers = request.headers();
const parameters = request.parameters();
const cancelled = request.cancelled();
const secure = request.secure();
```

`method()` returns the uppercase HTTP method name. `isJson()` checks the request content type; `expectsJson()` and `accepts("application/json")` inspect accepted response media types. Client security and address information come from the HTTP server's configured trust policy.

## Cookies and sessions

Cookies are available through the request cookie APIs. Session-enabled requests expose the current session through the session middleware.

See [Sessions](session.md).

## Authentication

Authenticated requests expose the resolved user and authentication state.

```gnr
const signedIn = request.authenticated();
const anonymous = request.guest();
```

Without an authentication context, `authenticated()` is false and `guest()` is true. `hasAuth`, `hasSession`, and `hasServices` check the attached request contexts.

See [Authentication](authentication.md).

## Uploaded files

Multipart requests expose owning `UploadedFile` values. `file(name)` returns `UploadedFile?` when exactly one file matches; `hasFile(name)` tests whether any match. `files()` returns all uploads and `files(name)` returns matches in multipart order. The `[]` suffix is optional when retrieving a repeated field.

```gnr
const present = request.hasFile("avatar");
const avatar = request.file("avatar");
if (avatar != null) {
    const metadata = {"name": avatar.name(), "type": avatar.contentType(),
        "size": avatar.size(), "field": avatar.field()};
}
const photos = request.files("photos");
```

`bytes()` returns a binary-safe string. Uploads retain their bytes after the request ends or an action resumes from `await`. Client names lose directory components; names and Content-Type remain client metadata. Apply [file validation](validation.md#files) before storage and choose the storage path explicitly.

Multipart text fields appear in `form()` and take precedence over query fields in `all()`/`structuredInput()`. Uploads are accessed separately; selected upload metadata appears in validation results. Repeated scalar text fields use the last value. File/text collisions are rejected. Repeated files use an array field such as `photos[]`; a single `file` rule rejects a group of multiple uploads.

Malformed multipart bodies produce status 400. Parser defaults limit the body to 16 MiB, each file to 8 MiB, each text field to 1 MiB, part headers to 8 KiB, and parts to 128; exceeding a limit produces status 413. The server's body limit also applies. Native integrations can set `Request::multipart_limits(MultipartLimits)` before reading input.

## Bearer tokens

Bearer credentials can be read from the authorization header through the request authentication helpers.

```gnr
const token = request.bearerToken();
const authorization = request.authorization();
```

A bearer credential's presence does not authenticate the request. Authentication middleware verifies credentials and establishes the authentication context.
