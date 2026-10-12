# Application Load and Performance

**Availability:** The installed HTTP/ORM load fixture was merged in PR #192
after the published v1.0.0 core SDK. Build a current-source SDK with SQLite
support to run it. The October 5 release downloads do not contain this
post-release fixture. See [Release Status](development-status.md).

See the [recorded local baseline](application-performance-baseline.md) for its
environment, results and resource observations.

Measure a running native Gungnir application through real HTTP/1.1 connections.
The installed SDK compiles the fixture's ordinary `.gnr` model, controller,
routes and migration in Release mode. No generated C++ is patched.

## Workloads

| Profile | Request | Application work |
| --- | --- | --- |
| `json` | `GET /json` | Host validation, bearer authentication, routing and JSON serialization |
| `orm-read` | `GET /items/{item}` | Authentication, model route binding, a real SQLite lookup and model JSON |
| `orm-write` | `PUT /items/{item}` | Authentication, model binding, input validation, lookup, model save and model JSON |

The coordinator creates a temporary database through the installed CLI's real
migration. It seeds 4,096 rows offline, then starts one compiled HTTP process.
Each seed body has a row reference and 64 payload characters. Read clients walk
deterministic IDs above 64; write clients each own a row in 1–64 and send a
distinct sequence value. Every response is checked against its expected ID and
body. The coordinator separately verifies the last write for each client in the
persisted database. Requests that fail validation or return an unexpected status
remain failed attempts, without automatic retries.

SQLite uses one connection, WAL journaling and `synchronous=FULL`. Reads use
SQLite's normal warm page cache after warmup. Writes use the real durable save
path; this is not an in-memory database or a SQL construction benchmark. The
write profile includes its binding lookup and explicit controller lookup.
The measured profiles require zero queries per JSON response, one per read and
three per write, so a changed workload or unexpected extra queries fail the
contract instead of silently changing the benchmark.

## Build and run

On Linux, install a C++23 compiler, CMake 3.25+, Ninja, Python 3.10+ and SQLite
development headers. The script only uses Python's standard library.

```bash
cmake -S . -B build-load -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DGUNGNIR_BUILD_TESTS=OFF \
  -DGUNGNIR_WITH_SQLITE=ON
cmake --build build-load --parallel 2 \
  --target gungnir-cli gungnirc gungnir-orm
cmake --install build-load --prefix "$PWD/load-sdk"

python3 load-sdk/share/gungnir/benchmarks/application_load.py \
  --sdk "$PWD/load-sdk" \
  --output application-load.json --summary application-load.md
```

An application SDK with bundled SQLite can also run this fixture. Other SDK
adapters keep their documented native dependency requirements. The installed
script and fixture live under `share/gungnir/benchmarks`. Output files are JSON,
Markdown and a sibling `.log` containing build/application output. The temporary
application is removed after the run, and a random fixture credential stays out
of reports and logs. All fixture endpoints, including measurement controls,
require that credential; the listener only binds to loopback.

Defaults are three measured runs of three seconds per workload at 1, 8 and 32
clients, 0.25 seconds of warmup per phase and a 30-second ORM read soak at the
highest concurrency. Each load process opens its connections and warms the
application before the coordinator resets native statistics and starts the
measurement. Warmup is excluded from measured response counts and DB latency.

For a short contract check:

```bash
python3 load-sdk/share/gungnir/benchmarks/application_load.py \
  --sdk "$PWD/load-sdk" --smoke
```

For a longer local measurement:

```bash
python3 load-sdk/share/gungnir/benchmarks/application_load.py \
  --sdk "$PWD/load-sdk" \
  --concurrency 1,8,32,64 --duration 10 --samples 5 \
  --warmup 1 --soak-duration 120
```

The fixture bounds connections at 128, active dispatches at 64, requests at
16 KiB and headers at 8 KiB. Its regular 100-request keep-alive policy is
retained. The generator honors `Connection: close` and includes subsequent
connection establishment in request latency. Concurrency must be 1–64; this
suite does not intentionally drive admission shedding above the dispatch limit.
Existing HTTP overload tests cover that contract.

## Read the report

- **Throughput:** successful validated responses divided by measured elapsed
  time, including completion of requests already started at the duration limit.
  Attempt count, failure count and HTTP status counts are retained separately.
- **HTTP latency:** time from starting a request to receiving its complete
  response and applying its connection-close policy, including request I/O and
  reconnects. Validation costs contribute to
  generator CPU and throughput. Clients issue one request at a time.
- **Database latency:** the runtime's `db.client.operation.duration` metric from
  real database operations, including driver work and its tracing overhead.
  Pool acquisition and waiting before the connection's execute lock are outside
  this metric. HTTP latency includes application scheduling and pool acquisition.
- **Percentiles:** fixed histogram bucket upper bounds at 0.01 ms resolution.
  HTTP buckets reach 655.35 ms; DB buckets reach 40.95 ms. Overflow counts are
  explicit; a percentile in the overflow bucket uses the maximum observed value
  as its upper bound. No per-request sample list grows with throughput.
- **Memory:** the native server's resident set from Linux `/proc`, sampled every
  50 ms. JSON retains baseline, peak, final and the time series. RSS excludes
  other processes and general filesystem page cache. Short peaks between
  samples can be missed; a short soak does not prove absence of memory leaks.
- **CPU:** server and generator process CPU seconds and percent of one core.
  A busy Python generator can limit observed throughput; the report makes that
  visible. The coordinator and generator run on the same host as the server.
  The server's observation window starts just before releasing the prepared
  generator and ends after receiving its result. Reconnect counts during warmup
  and measurement are recorded separately.

The Markdown table gives median throughput and the median of each run's
percentile upper bound, plus the maximum observed server RSS across runs. These
are not pooled-request percentiles. JSON retains every measured run and its
native metrics, resource samples and generator CPU.

Metadata records platform, CPU model/affinity/quota, compiler and Release flags,
SDK capabilities, source commit when available, dirty state, fixture/generator/
binary hashes, timestamp, runtime limits, database version, durability, row
count, warmup, repetitions and client policy. A script copied out of a Git
checkout can have an empty commit field outside CI; retain the source revision
alongside its output when comparing local SDKs.

## Shutdown during traffic

The coordinator opens one request per client, up to 32 clients, to a coroutine
endpoint that waits at an admission barrier. The cap leaves admission capacity
for observing the barrier. It verifies all requests are active before sending
SIGTERM. The barrier remains closed until the listener stops accepting new
connections and the process is still alive. It then releases admitted work and
requires every response to arrive, exit code zero, zero active requests and zero
leased database connections in the application's shutdown record.

The report records actual drain time and the configured five-second shutdown
budget. This exercises cooperative request draining on the generated native
server. Slow synchronous user code, deadline cancellation, background workers
and exporter cleanup have their own runtime/deployment acceptance coverage.

## CI and comparisons

Performance Baseline installs its optimized SDK and runs two two-second samples
at 1, 8 and 32 clients plus a ten-second read soak. Response correctness,
persisted writes, zero query errors, completion counts, request/lease cleanup and
admitted-request shutdown are required. Timing and memory values are reported
without a shared-runner regression threshold. JSON, Markdown and logs are
uploaded with the microbenchmark artifacts, including a failure report when the
application run fails.

Compare raw reports only on equivalent hardware, CPU quotas, compilers, flags,
SDK features, database durability, fixture and generator hashes, concurrency and sample policy.
Retain all samples and account for generator CPU before interpreting a plateau.
The microbenchmark comparison command operates on its own schema and does not
accept this application report.

These measurements establish a reproducible application workload, not universal
production capacity or a speed comparison with Laravel. The fixture uses
loopback HTTP without TLS or a proxy, local SQLite, a small payload and one
server process. Measure your real endpoints, deployment topology, remote
databases, cache/session/queue traffic and realistic request mix separately.
The [Linux deployment reference](production-deployment.md) supplies a tested
TLS/proxy/shared-state topology for that follow-on measurement.

Sources: [runner](../benchmarks/application_load.py),
[controller](../benchmarks/application/app/controllers/LoadController.gnr),
[bootstrap](../benchmarks/application/bootstrap/app.hpp),
[performance workflow](../.github/workflows/benchmark.yml).
