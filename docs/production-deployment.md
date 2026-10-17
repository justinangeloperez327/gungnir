# Deploy an application on Linux

The [production deployment example](../examples/production-deployment/README.md)
provides a tested reference for running a Gungnir application on one Linux host.
It uses Nginx for HTTPS, Supervisor for process supervision, SQLite for records,
Redis for persistent cache/sessions/queues/leases, and a local storage directory.
Two HTTP processes share the same data. A separate worker processes jobs and a
separate scheduler enqueues recurring work.

The commands below require Ubuntu 24.04 or a compatible Linux installation,
Python 3.11 or later, a C++23 compiler, CMake and Ninja. Use absolute, normalized
paths containing letters, numbers, `/`, `.`, `_` and `-`. All application
operations run as the dedicated `gungnir` service user.

## Build and install the SDK

The portable core SDK does not include Redis or these external daemons. Build a
custom SDK with SQLite and Redis enabled. OTLP is optional; enable it to use the
collector settings from [External Observability](external-observability.md).
These commands run from the Gungnir source checkout:

```sh
sudo apt-get update
sudo apt-get install -y build-essential cmake ninja-build python3 openssl \
  nginx supervisor redis-server redis-tools libsqlite3-dev libhiredis-dev \
  libcurl4-openssl-dev
cmake -S . -B build-deployment -G Ninja \
  -DCMAKE_BUILD_TYPE=Release -DGUNGNIR_BUILD_TESTS=OFF \
  -DGUNGNIR_WITH_SQLITE=ON -DGUNGNIR_WITH_REDIS=ON -DGUNGNIR_WITH_OTLP=ON
cmake --build build-deployment --parallel 2
sudo cmake --install build-deployment --prefix /opt/gungnir
```

Retain the SDK and its runtime adapter dependencies for building later
releases. This profile uses the host SQLite/hiredis/libcurl packages.
Package version remains `1.0.1`, native contract `1.0`, ABI epoch `1`.

## Provision the host

Create a service account and private deployment directory:

```sh
sudo useradd --system --create-home --shell /bin/bash gungnir
sudo install -d -m 0700 -o gungnir -g gungnir /srv/gungnir
sudo -iu gungnir
```

Provision a certificate and private key for your public DNS hostname. Use
your certificate provider's full chain and store the private key with mode
`0600`, readable by the service user. Certificate issuance, DNS, host firewall
rules and off-host backup storage belong to the host operator. Renew the
certificate through that provider and reload the managed Nginx process after
checking its configuration.

Copy the environment template, edit it privately, and generate independent
secrets with `openssl rand -hex 32`:

```sh
cp /opt/gungnir/share/gungnir/examples/production-deployment/deployment.env.example \
  /srv/gungnir/deployment.env
chmod 0600 /srv/gungnir/deployment.env
```

Set `APP_PUBLIC_HOST` to the certificate's hostname, `APP_API_TOKEN` and
`REDIS_PASSWORD` to independent 64-character lowercase hex values, and
`APP_NAMESPACE` to a stable application-specific name. Keep these settings:

```ini
APP_ENV=production
APP_DEBUG=false
APP_HOST=127.0.0.1
DB_CONNECTION=sqlite
DB_DATABASE=/srv/gungnir/data/application.sqlite
DB_POOL_SIZE=1
DEPLOY_DATA_DIR=/srv/gungnir/data
REQUEST_WORK_DELAY_MS=0
```

The file contains literal, unquoted `KEY=value` entries and whole-line comments;
it is never sourced as a shell script. Rendering copies it into protected
`config/deployment.env`. Process environment variables override file values, so
keep the service user's environment free of conflicting application settings.
Missing secrets, debug mode, a public application listener, and an inconsistent
database path fail startup. The API token persists in this external file across
release and process changes. Rotate it by updating that private file and
restarting the application processes.

This profile uses dedicated Redis on loopback port `6379`; change `REDIS_PORT`
if the host already uses that port. The five HTTP/TLS/backend/Redis ports must
be distinct. Supervisor's control socket is private and has no network listener.
Treat the service account and other host administrators as trusted: applications
trust proxy headers only from the local loopback connection.

## Build, configure and start

Build a named release from the installed sample project:

```sh
python3 /opt/gungnir/share/gungnir/examples/production-deployment/deploy.py \
  --root /srv/gungnir build --sdk /opt/gungnir --revision initial
python3 /opt/gungnir/share/gungnir/examples/production-deployment/deploy.py \
  --root /srv/gungnir configure --revision initial \
  --env-file /srv/gungnir/deployment.env \
  --certificate /srv/gungnir/tls/fullchain.pem \
  --private-key /srv/gungnir/tls/privkey.pem
supervisord -n -c /srv/gungnir/config/supervisord.conf
```

The reference listens publicly on HTTP `8080` and HTTPS `8443`, with application
listeners on `127.0.0.1:8001` and `127.0.0.1:8002`. Set ports with
`--http-port`, `--https-port`, `--web-a-port`, and `--web-b-port` when rendering.
Expose the public ports through your host firewall. Keep backend/Redis ports
private.

In another service-user shell, pause the application while applying the
explicit migration, then resume:

```sh
python3 /opt/gungnir/share/gungnir/examples/production-deployment/deploy.py \
  --root /srv/gungnir pause
cd /srv/gungnir/current
GUNGNIR_DEPLOY_ENV=/srv/gungnir/config/deployment.env \
  ./.gungnir/build/migrations migrate
python3 /opt/gungnir/share/gungnir/examples/production-deployment/deploy.py \
  --root /srv/gungnir resume
```

Web startup never migrates automatically. Before schema setup, liveness works
and readiness returns `503`. `resume` requires both HTTP instances to be ready
and all six supervised processes to be running. A persistent startup failure
reaches Supervisor's `FATAL` state after bounded retries; inspect the protected
logs, correct the cause, then run `resume`.

For boot startup, stop the foreground Supervisor cleanly, leave the
service-user shell, and install the rendered systemd unit:

```sh
sudo install -m 0644 /srv/gungnir/config/gungnir-example.service \
  /etc/systemd/system/gungnir-example.service
sudo systemctl daemon-reload
sudo systemctl enable --now gungnir-example.service
```

The unit runs Supervisor as `gungnir`. It restarts Supervisor on failure and
provides a 90-second stop budget for the process groups. The acceptance suite
tests Supervisor directly; boot and the installed systemd unit still need
verification on your actual machine.

## HTTPS, health and supervision

Nginx serves TLS 1.2/1.3 for the configured virtual host. Plain HTTP redirects
to the fixed public HTTPS hostname; unknown hosts/SNI are rejected. It overwrites
`Host`, client IP, forwarded protocol and request ID headers and clears
`Forwarded`, preventing a public caller from forging the application's proxy
context. Upstream retry is disabled so Nginx does not replay a mutation on
another application instance.

| Endpoint or operation | Meaning |
| --- | --- |
| `GET /health/live` | `200` while the HTTP process responds; no dependency probe |
| `GET /health/ready` | `200` only when lifecycle, schema/SQLite, Redis and maintenance checks pass; otherwise `503` |
| `deploy.py --root /srv/gungnir status` | Supervisor states for both web instances, worker, scheduler, Redis and proxy |
| `deploy.py --root /srv/gungnir resume` | Start stopped processes and wait for fleet readiness |
| `supervisorctl -c /srv/gungnir/config/supervisord.conf restart application:worker` | Restart a worker with a graceful stop first |

Health endpoints require no API token and report only overall state. Readiness
does not certify that a worker is consuming jobs or a scheduler has fired;
monitor those processes and workload signals separately. All business routes
require HTTPS and `Authorization: Bearer <APP_API_TOKEN>`. The `/context`
endpoint returns instance/revision and trusted transport context to an
authenticated operator.

An example mutation is `POST /notes` with URL-encoded `reference` and `body`
fields. Both web instances see its SQLite record, Redis cache and queued job.
Repeated submission of the same reference/body can retry an unsuccessful
enqueue; a different body for an existing reference returns `409`.
`Finalize` updates the record idempotently. SQLite creation and Redis enqueue
are separate operations, so this sample is not a transactional outbox. A
concurrent creation race can require a client retry. Jobs may run more than
once; keep real application handlers idempotent.

Native HTTP limits bound request/header sizes, connections, active dispatches
and read/write/request/idle timeouts. Nginx limits request bodies to 32 KiB.
SIGTERM closes HTTP admission and drains cooperative work with a five-second
HTTP deadline. Supervisor allows 45 seconds for application shutdown, including
worker cleanup and optional exporter flush, before force-stopping the process
group. Worker and scheduler lifetimes are independent of the HTTP server.

## Observe the deployment

`logs/web-a.jsonl`, `web-b.jsonl`, `worker.jsonl` and `scheduler.jsonl` contain
structured application logs. Nginx logs method, path, status and request ID;
it omits request bodies, query strings and authorization. Supervisor rotates
application logs at 20 MB with five backups; error and daemon logs have their
own 10 MB limits. Configuration, control sockets, logs and data stay private
to the service account.

Set `OTLP_ENDPOINT`, and optionally `OTLP_CA_FILE` and `OTLP_AUTHORIZATION`,
to reuse the installed optional exporter. Each process emits service,
environment, instance and release resource fields. Queue propagation, redaction,
bounded export and Collector delivery are covered by the
[external observability acceptance contract](external-observability.md).
The deployment suite checks that API/Redis secrets, note bodies and session
identifiers do not appear in process logs.

Monitor disk space, Redis persistence errors, queue depth/age/failures, worker
and scheduler activity, readiness failures and exporter loss. Set resource
budgets for your workload before choosing host capacity. This example supplies
an operational path; it does not establish a throughput target.

## Back up and restore

SQLite, Redis and local files are coordinated by pausing all application writers.
`pause` writes a maintenance marker before stopping both web processes, the
worker and the scheduler. Redis and Nginx remain running. Managed background startup
also refuses work while the marker exists, including after a supervisor restart.

```sh
python3 /opt/gungnir/share/gungnir/examples/production-deployment/deploy.py \
  --root /srv/gungnir pause
python3 /opt/gungnir/share/gungnir/examples/production-deployment/deploy.py \
  --root /srv/gungnir backup --output /srv/backups/gungnir-snapshot
python3 /opt/gungnir/share/gungnir/examples/production-deployment/deploy.py \
  --root /srv/gungnir verify-backup --input /srv/backups/gungnir-snapshot
python3 /opt/gungnir/share/gungnir/examples/production-deployment/deploy.py \
  --root /srv/gungnir resume
```

Provision the backup parent directory for the service user first. Each backup
uses a new destination. It contains an online-API SQLite snapshot, a synchronous
Redis RDB snapshot, local storage files and a SHA-256 manifest with namespace
and release identity. It excludes credentials, configuration, TLS keys and
logs. Verification checks the exact file set, rejects links/special files,
checks all hashes, runs SQLite integrity checks and validates the RDB. These
hashes detect corruption; protect backup storage against unauthorized changes.

Copy verified backups off the host and apply your retention/access policy.
Keep the matching release/SDK, private configuration and TLS material in a
separate protected recovery plan. Practice the restore before relying on it.

Restore requires an explicit pause and stopped Redis:

```sh
python3 /opt/gungnir/share/gungnir/examples/production-deployment/deploy.py \
  --root /srv/gungnir pause
supervisorctl -c /srv/gungnir/config/supervisord.conf stop redis
python3 /opt/gungnir/share/gungnir/examples/production-deployment/deploy.py \
  --root /srv/gungnir restore --input /srv/backups/gungnir-snapshot
python3 /opt/gungnir/share/gungnir/examples/production-deployment/deploy.py \
  --root /srv/gungnir resume
```

Restore validates the snapshot before replacing data and archives the previous
state under `recovery/<timestamp>`. It loads the restored RDB with AOF disabled,
then `resume` enables AOF and waits for a new base file before admitting
application work. This prevents an old AOF from overriding the restored
snapshot. If resume fails, keep maintenance in place and inspect Redis logs;
do not remove the `enable-aof` marker manually.

Restoring replaces the whole dataset, including queued jobs, sessions, cache and
leases. Replayed jobs must tolerate repeats. Use a backup compatible with the
selected release's schema. Do not write to shared directories from unmanaged
applications during maintenance. If the host stops between the two data-directory
renames, keep processes stopped and recover the preserved directory under
`recovery/` before retrying; restore is not a power-loss transaction across stores.

## Promote or roll back a release

The reference operations build from the installed sample project. For your own
application, maintain its source under version control and adapt that project
payload; include HTTP, migration and background executables in every release.
Build with a target-compatible toolchain. The release manifest records the SDK
version and hashes of all three executables plus `REVISION`; promotion verifies
those artifacts.

```sh
python3 /opt/gungnir/share/gungnir/examples/production-deployment/deploy.py \
  --root /srv/gungnir build --sdk /opt/gungnir --revision candidate
python3 /opt/gungnir/share/gungnir/examples/production-deployment/deploy.py \
  --root /srv/gungnir promote --revision candidate
python3 /opt/gungnir/share/gungnir/examples/production-deployment/deploy.py \
  --root /srv/gungnir promote --revision initial
```

Promotion pauses admission/background work, atomically switches the `current`
symlink and checks the restarted fleet. If a candidate fails startup/readiness,
it attempts to restore the previous release and its readiness. Shared data is
outside both releases. This is a brief maintenance deployment, with no promise
of uninterrupted availability.

Build/promotion never migrate or roll back the database automatically. Apply
reviewed schema changes explicitly during a pause. Keep changes compatible with
the previous executable if you need binary rollback; destructive schema changes
need a separate recovery decision. Rollback cannot undo completed jobs, external
effects or database mutations.

## Tested scope and limits

Enable `GUNGNIR_DEPLOYMENT_INTEGRATION_TESTS=ON` with tests/tools/SQLite/Redis in
a Linux build, build the SDK targets, and run:

```sh
ctest --test-dir build-deployment --output-on-failure --no-tests=error \
  -R '^gungnir\.production_deployment$'
```

CI installs this example from the SDK, compiles its `.gnr` project and tests
real certificate trust, forged proxy headers, authenticated routing, shared
records/cache/sessions, queue and scheduler side effects, HTTP/worker/scheduler
crash recovery, graceful HTTP drain, dependency outage, persistent Redis restart,
coordinated backup, corruption refusal, restore, renewed AOF persistence,
promotion, rollback and failed-candidate recovery. The promotion fixture uses
two copies of the compiled native binaries with distinct revision metadata;
only the deliberately failing candidate is replaced with a failure injector.

This is a single-host reference, with no failover or multi-host storage claim.
SQLite WAL files require local filesystem semantics; do not share them over a
network filesystem. Redis AOF uses `appendfsync everysec`, so recent writes can
be lost on a crash; process-restart tests do not establish power-loss durability.
Redis uses `noeviction`, with host memory capacity chosen by the operator.
Schedule backups according to your recovery point objective. Cooperative
cancellation and queue visibility/retry rules remain subject to the
[production](production.md) and [queue](queues.md) contracts.

Upstream operating contracts:
[Nginx proxy configuration](https://nginx.org/en/docs/http/ngx_http_proxy_module.html),
[Supervisor configuration](https://supervisord.org/configuration.html),
[Supervisor process states](https://supervisord.org/subprocess.html), and
[Redis persistence](https://redis.io/docs/latest/operate/oss_and_stack/management/persistence/).
