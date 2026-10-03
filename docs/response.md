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

## Downloads and streams

Gungnir supports downloadable responses and streaming bodies for content that should not be buffered as a single response string.

## WebSockets

The HTTP runtime can upgrade supported requests to WebSocket connections through the response/runtime WebSocket API.
