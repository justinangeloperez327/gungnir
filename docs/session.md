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

## Flash data

Flash values are available for the next request and are useful for validation messages, status notices, and redirect workflows.

## Session identifiers

Authentication and other privilege changes rotate the session identifier to protect against session fixation.

## Authentication

Session authentication stores the authenticated identity in the session and restores it on later requests.

See [Authentication](authentication.md).

## Drivers

Applications can configure session persistence appropriate to their deployment. Process-local storage is suitable for development and tests; shared production deployments should use a shared session backend.

## Cookies

The session identifier is transported using a configured cookie with appropriate security, same-site, and lifetime settings.
