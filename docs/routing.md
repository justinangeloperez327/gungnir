# Routing

Gungnir routing maps HTTP methods and paths to synchronous or asynchronous handlers and controller actions.

The router supports GET, POST, PUT, PATCH, DELETE, OPTIONS and HEAD routes, global middleware, route middleware, path groups, named routes, regular-expression parameter constraints, numeric and UUID constraints, reverse URL generation, and application fallback handlers.

Named route registration rejects duplicate names. Reverse URL generation requires all route parameters to be supplied.

Route groups share path prefixes and middleware. Group support covers the primary HTTP verbs so grouped APIs do not need to escape back to the root router.

The Route facade resolves controller instances through the Gungnir container. Controller methods may return Response or Task<Response>; coroutine mechanics remain framework plumbing.

BindingRegistry is the foundation for typed/model route binding. ORM integration will supply model lookup semantics in the ORM relationship/data milestones rather than coupling the router directly to a database implementation.

Future route compilation/cache work should compile route metadata into an efficient immutable dispatch representation for production without changing the application-facing route API.
