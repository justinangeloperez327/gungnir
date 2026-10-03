# Sessions

> **Status: Development.** This guide describes the current implementation and documented limits. The 1.0 compatibility contract is not frozen yet.

## Current behavior

Session supplies server-side request state, identity regeneration, invalidation and flash state. StartSession middleware loads/persists state using a Store and configures the session cookie. Memory and optional Redis store implementations are available.

Access attached state through `Request::session`; check `has_session` when middleware may be absent. Authentication and CSRF must be configured alongside session middleware.

## Limits and planned work

Memory state is not shared across processes. Distributed storage does not by itself prevent concurrent request updates or provide application transaction semantics. Configure cookie security, expiry and store failure handling for the deployment.

## Implementation references

- [include/gungnir/session/session.hpp](../include/gungnir/session/session.hpp)
- [include/gungnir/session/middleware.hpp](../include/gungnir/session/middleware.hpp)
- [include/gungnir/session/store.hpp](../include/gungnir/session/store.hpp)
- [include/gungnir/session/redis_store.hpp](../include/gungnir/session/redis_store.hpp)

See the [documentation index](README.md), [getting started](getting-started.md), and [target design](design/session.md).
