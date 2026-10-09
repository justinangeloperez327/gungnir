# Sessions

Sessions persist request-scoped user state across HTTP requests.

## Session middleware

Session middleware loads the incoming session before the route pipeline and persists session changes with the response.

## Reading and writing values

```gnr
const locale = request.session().get("locale");
request.session().put("locale", "en");
```

Sessions support retrieving, storing, removing, and clearing values.

`request.session()` returns a `Session` handle for the request's live session. Handles preserve ownership across function calls and awaited work. Session values are strings; `get` returns an empty string for an absent key, and `has` distinguishes absence from an empty stored value.

```gnr
const current = request.session();
const hasLocale = current.has("locale");
const values = current.values();
current.forget("temporary");
```

`values()` returns an owned `Map<string, string>`. Use `clear()` to remove stored values. A session handle represents runtime state; persist specific values or identifiers in models and job payloads.

## Flash data

Flash values are available for the next request and are useful for validation messages, status notices, and redirect workflows.

```gnr
request.session().flash("notice", "Saved");
const notice = request.session().flashed("notice");
```

The session middleware ages flash values between requests. A flashed value is visible on the following request and expires after that request.

## Session identifiers

Authentication and other privilege changes rotate the session identifier to protect against session fixation.

```gnr
const previousId = request.session().id();
request.session().regenerate();
const rotated = request.session().regenerated();
```

`regenerate()` chooses a new unpredictable identifier and preserves values. `invalidate()` clears values and flash data and chooses a new identifier. The middleware removes the old stored identifier when persisting a rotated session.

## Authentication

Session authentication stores the authenticated identity in the session and restores it on later requests.

See [Authentication](authentication.md).

## Drivers

Applications can configure session persistence appropriate to their deployment. Process-local storage is suitable for development and tests; shared production deployments should use a shared session backend.

The Redis store writes complete snapshots with server-side expiration. Concurrent requests use last-writer-wins semantics; rotation cleanup is separate from saving. Shared persistence does not serialize request updates or prevent an old in-flight snapshot from recreating a removed identifier. See [Distributed coordination](cache-coordination.md) for the explicit contract and multi-instance acceptance.

## Cookies

The session identifier is transported using a configured cookie with appropriate security, same-site, and lifetime settings.
