# Linux production deployment example

This example runs an installed `.gnr` application on one Linux host with Nginx
TLS, two HTTP processes, a queue worker, a scheduler, SQLite and persistent
Redis. Supervisor manages the processes; the rendered systemd unit starts
Supervisor at boot.

Read the [deployment guide](../../docs/production-deployment.md) before using
the example. The installed copy is under
`share/gungnir/examples/production-deployment` in the SDK; the guide is in the
repository documentation.

`deploy.py` builds immutable releases, renders private configuration, reports
process status, pauses/resumes application work, creates verified coordinated
backups, restores offline data, and promotes or rolls back a release:

```sh
python3 deploy.py --root /srv/gungnir --help
```

The application accepts an operator-issued bearer token. It demonstrates
shared notes, Redis cache and secure server-side sessions. `Finalize` processes
queued notes idempotently; `Heartbeat` writes a file through the scheduler
and queue. The sample is a deployment reference, not a user account application.

The `gungnir.production_deployment` acceptance test installs the SDK and this
example, compiles its HTTP/migration/background executables, and exercises real
Nginx HTTPS, Supervisor restarts, Redis persistence, graceful request draining,
coordinated backups, restore, release promotion and failed-candidate recovery.
It uses temporary certificates, secrets, directories and ports.
