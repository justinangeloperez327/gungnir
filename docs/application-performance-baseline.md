# Application Load Baseline

This is a local observation of the Group 7 fixture, recorded on 2026-10-11T19:45:37.518202+00:00.
The framework was built from main `6ac1f8c28f7ef5ce92011ab850cfab60dfcca752`; the fixture and generator were from the Group 7 working tree (`source_dirty=true`).

## Environment and policy

- Platform: Linux-6.18.44-x86_64-with-glibc2.39; x86_64.
- CPU: INTEL(R) XEON(R) PLATINUM 8573C; 9 CPUs in affinity; cgroup quota `800000 100000` (8 CPU equivalents).
- Compiler: c++ (Ubuntu 13.3.0-6ubuntu2~24.04) 13.3.0; Release `-O3 -DNDEBUG`; Python 3.12.14.
- SDK: v1.0.0 custom profile, SQLite 3.45.1 enabled, other optional adapters disabled; local static SQLite dependency.
- Database: one connection, local temporary file, WAL/FULL, 4,096 seeded rows; one server without TLS/proxy.
- Each profile: two 2-second measured runs at 1/8/32/64 closed-loop keep-alive clients, 0.25-second warmup, no retries.
- Additional read soak: 15 seconds at 64 clients. Shutdown barrier: 32 admitted requests.
- Fixture SHA-256: `90904327ee22989785b0f4c67fab52a78455246c8baee1315c6ddec419059442`.
- Generator SHA-256: `46c51eb4ccb5059578f82f770aff056e88fe69207e81c731cd41dc3d1e9a6fe9`.
- Application SHA-256: `89f7c00bf3fee82a27f31db22e155d8d3ace3d22f6c155a7a1b7fc9b53cc677a`.
- Full local report SHA-256: `b939da047f5a84dccb0fb4c1d53b7257386c6d17fc16f2718b616443c59200d3`.

## Observed results

| Workload | Clients | Runs | Median successful req/s | Median p50 ms | Median p95 ms | Median p99 ms | Peak server RSS MiB | Median DB p95 ms |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| json | 1 | 2 | 7871.1 | 0.08 | 0.31 | 0.67 | 4.84 | 0.00 |
| json | 8 | 2 | 16384.6 | 0.32 | 1.22 | 2.72 | 4.90 | 0.00 |
| json | 32 | 2 | 13678.4 | 1.58 | 6.37 | 13.21 | 5.03 | 0.00 |
| json | 64 | 2 | 23285.3 | 2.50 | 5.57 | 9.42 | 5.32 | 0.00 |
| orm-read | 1 | 2 | 4846.8 | 0.12 | 0.57 | 1.35 | 5.45 | 0.04 |
| orm-read | 8 | 2 | 13227.6 | 0.33 | 2.12 | 5.17 | 5.45 | 0.03 |
| orm-read | 32 | 2 | 13519.6 | 1.99 | 5.33 | 8.45 | 5.45 | 0.03 |
| orm-read | 64 | 2 | 14111.6 | 3.91 | 9.54 | 13.93 | 5.71 | 0.02 |
| orm-write | 1 | 2 | 1913.8 | 0.31 | 1.57 | 2.76 | 5.72 | 0.04 |
| orm-write | 8 | 2 | 5507.1 | 1.14 | 3.62 | 5.93 | 5.72 | 0.03 |
| orm-write | 32 | 2 | 11206.4 | 2.41 | 6.24 | 9.39 | 5.72 | 0.02 |
| orm-write | 64 | 2 | 13018.5 | 4.30 | 9.42 | 14.59 | 5.73 | 0.02 |

Throughput and percentiles are medians across the two runs. Percentiles are 0.01 ms histogram upper bounds; server RSS is the maximum sampled value across runs. No HTTP or database histogram overflow occurred.

| Profile at 64 clients | Median generator CPU (% of one core) | Median server CPU (% of one core) |
| --- | ---: | ---: |
| json | 99.2 | 72.5 |
| orm-read | 82.1 | 78.0 |
| orm-write | 60.6 | 97.8 |

All 877,864 measured/soak requests were correct, with zero failed attempts and zero query errors. The defined query counts were zero per JSON response, one per read and three per write. All final write values were persisted. Every phase ended with zero active requests and zero database leases.

The read soak returned 323,179 correct responses; sampled server RSS remained 5.73 MiB at its start and end. A short soak is only an observation of that run.

SIGTERM stopped listener admission before releasing the barrier, drained 32/32 requests, and exited cleanly in 0.026 seconds. Its final record contained zero active requests and zero leased connections.

The JSON generator was near one full CPU core at 64 clients, so its throughput is not evidence of the server's maximum capacity. Timing varied across short runs on this shared sandbox. These figures describe the stated fixture and machine; they are not a regression threshold or a comparison with another framework.

The [complete local report](../benchmarks/results/application-load-baseline.json)
retains every measured run and the CPU/RSS samples behind this summary.
Performance Baseline CI publishes independent per-run JSON, Markdown and logs
as downloadable workflow artifacts. See the [methodology and run commands](application-performance.md)
to reproduce the workload.
