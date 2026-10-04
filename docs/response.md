# Responses

A `Response` describes the HTTP response returned by a controller, middleware, exception handler, or route.

## Text

```gnr
return text("Hello from Gungnir");
```

## JSON

```gnr
return json({
    "status": "ok"
});
```

A status code can be supplied when creating the response:

```gnr
return json(user, 201);
```

## Views and HTML

```gnr
return view("users/index", {
    "users": users
});
```

Applications can also return explicit HTML responses.

## Redirects

```gnr
return redirect("/dashboard");
```

Named routes can be used to avoid coupling redirects to hard-coded application paths.

## No content

Use a no-content response for successful operations without a response body.

```gnr
return noContent();
```

## Headers

```gnr
return text("ok")
    .header("Cache-Control", "no-store");
```

Headers, status, and body setters return a response for chaining. Use a mutable binding when modifying an existing response; getters do not modify it.

```gnr
let result = text("queued");
result.status(202);
result.header("X-Result", "accepted");
const status = result.status();
const body = result.body();
const header = result.header("X-Result");
const headers = result.headers();
return result;
```

Header names are case insensitive. Header values reject CR/LF characters to prevent injected response headers. Response values and getter results remain valid after a temporary chain finishes.

## Cookies

Responses can attach and expire cookies through the cookie APIs.

```gnr
return text("saved").cookie("theme", "silver", {
    "path": "/",
    "secure": true,
    "httpOnly": true,
    "sameSite": "Strict",
    "maxAge": 300
});
```

Cookie options also accept a `domain`. The defaults are path `/`, HTTP-only, `SameSite=Lax`, and a session lifetime. Set `secure` for HTTPS cookies. `SameSite=None` and `__Secure-` cookies require secure transport; `__Host-` cookies also require path `/` and no domain. Invalid names, values, domains, or attributes are rejected before the cookie is attached.

```gnr
return text("expired").withoutCookie("theme");
```

Use `withoutCookie(name, path, options)` with the same path and domain as the original cookie. Expiration sets a zero lifetime; secure cookie prefixes retain their secure requirement.

## Downloads and streams

Gungnir supports downloadable responses and streaming bodies for content that should not be buffered as a single response string.

```gnr
return download("name,total\nFreya,42\n", "report.csv", "text/csv");
```

`download(body, filename, contentType, status)` sends a buffered attachment. The content type defaults to `application/octet-stream` and the status defaults to 200. Use a streaming body for large or incrementally produced content.

## WebSockets

The HTTP runtime can upgrade supported requests to WebSocket connections through the response/runtime WebSocket API.
