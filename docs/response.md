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

## Headers

```gnr
return text("ok")
    .header("Cache-Control", "no-store");
```

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

## WebSockets

The HTTP runtime can upgrade supported requests to WebSocket connections through the response/runtime WebSocket API.
