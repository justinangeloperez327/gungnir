# Application Lifecycle

Gungnir applications have an explicit lifecycle: created, registering, booting, ready, running, stopping, and stopped.

Providers organize framework and package bootstrap without forcing application code to manage construction order. A provider can register services, boot after registrations are complete, observe readiness, and release resources during shutdown.

Application hooks are available for boot, ready, and shutdown work. Shutdown hooks and providers execute in reverse provider order where appropriate so dependent resources can be released before their dependencies.

Application mode is derived from `APP_ENV` and normalized into development, testing, staging, or production. `is_production()` provides a semantic mode check rather than scattering string comparisons through framework code.

The intended bootstrap sequence is:

`Application::create -> environment/configuration -> provider registration -> database/framework boot -> boot hooks -> ready providers/hooks -> server runtime -> shutdown providers/hooks`.

Providers must be installed before boot. This keeps the dependency graph deterministic and prevents runtime mutation of framework bootstrap behavior.
