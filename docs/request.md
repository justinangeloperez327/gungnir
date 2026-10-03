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

## JSON

JSON requests can be read as structured values:

```gnr
const payload = request.json();
```

Structured values preserve arrays, objects, numbers, booleans, strings, and null values.

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

## Cookies and sessions

Cookies are available through the request cookie APIs. Session-enabled requests expose the current session through the session middleware.

See [Sessions](session.md).

## Authentication

Authenticated requests expose the resolved user and authentication state.

See [Authentication](authentication.md).

## Uploaded files

Multipart requests expose uploaded files through the request upload API. File validation should be applied before storing user-provided files.

## Bearer tokens

Bearer credentials can be read from the authorization header through the request authentication helpers.
