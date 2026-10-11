#!/usr/bin/env python3
"""Installed .gnr application load measurements; Python standard library only.

The coordinator, native server and asyncio load generator are separate
processes. This deliberately bounded, closed-loop workload is not a saturation
or open-loop capacity claim. See docs/application-performance.md.
"""
import argparse
from array import array
import asyncio
from concurrent.futures import ThreadPoolExecutor
from datetime import datetime, timezone
import hashlib
import http.client
import json
import math
import os
from pathlib import Path
import platform
import resource
import secrets
import select
import shutil
import signal
import socket
import sqlite3
import statistics
import subprocess
import sys
import tempfile
import threading
import time
import urllib.parse


def require(condition, message):
    if not condition:
        raise RuntimeError(message)


class PhaseFailure(RuntimeError):
    def __init__(self, message, result):
        super().__init__(message)
        self.result = result


class Histogram:
    """Bounded 0.01 ms buckets, with an explicit overflow count."""
    def __init__(self):
        self.buckets = array('Q', [0]) * 65536
        self.count = self.overflow = 0
        self.total = self.maximum = 0.0

    def record(self, value):
        index = min(len(self.buckets) - 1, int(value / 0.01))
        self.buckets[index] += 1
        self.count += 1
        self.overflow += value >= (len(self.buckets) - 1) * 0.01
        self.total += value
        self.maximum = max(self.maximum, value)

    def snapshot(self):
        def percentile(fraction):
            if not self.count:
                return 0.0
            rank, cumulative = math.ceil(fraction * self.count), 0
            for index, count in enumerate(self.buckets):
                cumulative += count
                if cumulative >= rank:
                    return self.maximum if index == len(self.buckets) - 1 else (index + 1) * 0.01
            return self.maximum
        return dict(count=self.count, overflow=self.overflow, bucket_width_ms=0.01,
                    mean_ms=self.total / self.count if self.count else 0.0, max_ms=self.maximum,
                    p50_upper_ms=percentile(0.50), p95_upper_ms=percentile(0.95),
                    p99_upper_ms=percentile(0.99))


class Client:
    def __init__(self, port, token, timeout):
        self.port, self.token, self.timeout = port, token, timeout
        self.reader = self.writer = None
        self.connections = 0

    async def close(self):
        if self.writer:
            self.writer.close()
            try:
                await self.writer.wait_closed()
            except OSError:
                pass
        self.reader = self.writer = None

    async def request(self, method, path, body=b''):
        async def exchange():
            if self.writer is None:
                self.reader, self.writer = await asyncio.open_connection('127.0.0.1', self.port, limit=16384)
                self.connections += 1
            headers = (f'{method} {path} HTTP/1.1\r\nHost: localhost\r\n'
                       f'Authorization: Bearer {self.token}\r\nConnection: keep-alive\r\n'
                       f'Content-Type: application/x-www-form-urlencoded\r\nContent-Length: {len(body)}\r\n\r\n')
            self.writer.write(headers.encode('ascii') + body)
            await self.writer.drain()
            raw_headers = await self.reader.readuntil(b'\r\n\r\n')
            lines = raw_headers.decode('ascii').split('\r\n')
            protocol, status, _ = lines[0].split(' ', 2)
            require(protocol == 'HTTP/1.1', 'Unexpected HTTP version')
            fields = {}
            for line in lines[1:-2]:
                key, value = line.split(':', 1)
                key = key.lower()
                require(key not in fields, 'Duplicate response header: ' + key)
                fields[key] = value.strip()
            require('transfer-encoding' not in fields, 'Fixture unexpectedly returned a streamed response')
            length = int(fields['content-length'])
            require(0 <= length <= 1024 * 1024, 'Unbounded response')
            payload = await self.reader.readexactly(length)
            if fields.get('connection', '').lower() == 'close':
                await self.close()
            return int(status), payload
        try:
            return await asyncio.wait_for(exchange(), self.timeout)
        except Exception:
            await self.close()
            raise


def seed_body(identifier):
    return f'seed-{identifier}-' + 'x' * 64


def specification(case, worker, sequence, rows):
    if case == 'json':
        return 'GET', '/json', b'', {'message': 'Gungnir application load'}
    if case == 'orm-read':
        # Reads never overlap the first 64 rows reserved for write clients.
        identifier = 65 + (worker + sequence * 17) % (rows - 64)
        return 'GET', f'/items/{identifier}', b'', {'id': identifier, 'body': seed_body(identifier)}
    identifier, body = worker + 1, f'worker-{worker}-{sequence}'
    return 'PUT', f'/items/{identifier}', urllib.parse.urlencode({'body': body}).encode(), {
        'id': identifier, 'body': body}


async def generate(args):
    histogram, errors, statuses, last_writes = Histogram(), [], {}, {}
    clients = [Client(args.port, os.environ['GUNGNIR_LOAD_TOKEN'], args.timeout)
               for _ in range(args.clients)]
    sequences = [0] * args.clients
    success = 0

    async def attempt(index, measured):
        nonlocal success
        method, path, body, expected = specification(args.case, index, sequences[index], args.rows)
        sequences[index] += 1
        started = time.perf_counter()
        try:
            status, payload = await clients[index].request(method, path, body)
            elapsed = (time.perf_counter() - started) * 1000
            if measured:
                histogram.record(elapsed)
                statuses[str(status)] = statuses.get(str(status), 0) + 1
            require(status == 200, f'{args.case}: HTTP {status}')
            result = json.loads(payload)
            require(result == expected, f'{args.case}: incorrect response for {path}')
            if args.case == 'orm-write':
                last_writes[str(index + 1)] = expected['body']
            if measured:
                success += 1
        except Exception as error:
            if not measured:
                raise
            # No retries: failed requests stay in the attempted-request latency
            # histogram and reduce successful throughput.
            if 'elapsed' not in locals():
                histogram.record((time.perf_counter() - started) * 1000)
            if len(errors) < 3:
                errors.append(str(error) or type(error).__name__)

    async def warm(index):
        deadline = time.perf_counter() + args.warmup
        while True:
            await attempt(index, False)
            if time.perf_counter() >= deadline:
                return

    try:
        await asyncio.gather(*(warm(index) for index in range(args.clients)))
        print(json.dumps({'ready': True}), flush=True)
        require(await asyncio.to_thread(sys.stdin.readline) == 'start\n', 'Missing coordinator start signal')
        warmup_connections = sum(client.connections for client in clients)
        for client in clients:
            client.connections = 0
        before = resource.getrusage(resource.RUSAGE_SELF)
        started = time.perf_counter()
        deadline = started + args.duration

        async def worker(index):
            while time.perf_counter() < deadline:
                await attempt(index, True)
        await asyncio.gather(*(worker(index) for index in range(args.clients)))
        elapsed = time.perf_counter() - started
        after = resource.getrusage(resource.RUSAGE_SELF)
        cpu = after.ru_utime + after.ru_stime - before.ru_utime - before.ru_stime
        return dict(attempted=histogram.count, successful=success, failed=histogram.count - success,
                    seconds=elapsed, successful_requests_per_second=success / elapsed,
                    attempted_requests_per_second=histogram.count / elapsed,
                    latency=histogram.snapshot(), status_counts=statuses, errors=errors,
                    last_writes=last_writes, connections_opened=sum(c.connections for c in clients),
                    warmup_connections_opened=warmup_connections,
                    generator_cpu_seconds=cpu, generator_cpu_percent_one_core=100 * cpu / elapsed)
    finally:
        await asyncio.gather(*(client.close() for client in clients))


def request(port, token, path, method='GET', body=None, expected=200):
    connection = http.client.HTTPConnection('127.0.0.1', port, timeout=5)
    try:
        connection.request(method, path, body, {'Host': 'localhost', 'Authorization': 'Bearer ' + token})
        response = connection.getresponse()
        payload = response.read()
        require(response.status == expected, f'{path}: expected HTTP {expected}, got {response.status}')
        return json.loads(payload) if expected == 200 else payload
    finally:
        connection.close()


def process_resources(pid):
    # Linux RSS excludes the generator, coordinator and filesystem page cache.
    fields = Path(f'/proc/{pid}/stat').read_text().rsplit(')', 1)[1].split()
    pages = int(Path(f'/proc/{pid}/statm').read_text().split()[1])
    return pages * os.sysconf('SC_PAGE_SIZE'), (int(fields[11]) + int(fields[12])) / os.sysconf('SC_CLK_TCK')


class Resources:
    def __init__(self, pid):
        self.pid, self.samples, self.stop = pid, [], threading.Event()
        self.started = time.perf_counter()
        self.baseline, self.cpu_before = process_resources(pid)
        self.thread = threading.Thread(target=self.collect, daemon=True)
        self.thread.start()

    def collect(self):
        while True:
            try:
                rss, _ = process_resources(self.pid)
                self.samples.append({'seconds': time.perf_counter() - self.started, 'rss_bytes': rss})
            except OSError:
                return
            if self.stop.wait(0.05):
                return

    def finish(self):
        self.stop.set()
        self.thread.join(timeout=2)
        rss, cpu = process_resources(self.pid)
        elapsed = time.perf_counter() - self.started
        require(bool(self.samples), 'No server memory measurements')
        return dict(baseline_rss_bytes=self.baseline, final_rss_bytes=rss,
                    peak_rss_bytes=max([rss, self.baseline] + [s['rss_bytes'] for s in self.samples]),
                    cpu_seconds=cpu - self.cpu_before, cpu_percent_one_core=100 * (cpu - self.cpu_before) / elapsed,
                    observed_seconds=elapsed,
                    sample_interval_seconds=0.05, samples=self.samples)


def line_with_timeout(process, timeout):
    require(bool(select.select([process.stdout], [], [], timeout)[0]), 'Load generator did not become ready')
    line = process.stdout.readline()
    require(bool(line), 'Load generator exited before becoming ready')
    return json.loads(line)


def measure(args, case, concurrency, duration, process, project, environment, token, port, sample):
    command = [sys.executable, str(Path(__file__).resolve()), '--client', '--case', case,
               '--port', str(port), '--clients', str(concurrency), '--duration', str(duration),
               '--warmup', str(args.warmup), '--rows', str(args.rows), '--timeout', str(args.timeout)]
    with subprocess.Popen(command, env=environment, stdin=subprocess.PIPE, stdout=subprocess.PIPE,
                          stderr=subprocess.PIPE, text=True) as client:
        resources = None
        result = None
        try:
            require(line_with_timeout(client, args.timeout + args.warmup + 15).get('ready') is True,
                    'Invalid load generator handshake')
            require(request(port, token, '/bench/reset', 'POST') == {'reset': True}, 'Unable to reset measurements')
            resources = Resources(request(port, token, '/bench/stats')['proc_id'])
            client.stdin.write('start\n')
            client.stdin.flush()
            output, error = client.communicate(timeout=duration + args.timeout + 20)
            measured_resources = resources.finish()
            resources = None
            require(client.returncode == 0, 'Load generator failed: ' + error[-4000:])
            result = json.loads(output)
            require('client_error' not in result, 'Load generator failed: ' + str(result))
            stats = request(port, token, '/bench/stats')
            result.update(case=case, concurrency=concurrency, sample=sample,
                          server=measured_resources, native=stats)
            require(result['attempted'] > 0 and result['failed'] == 0, 'Load request errors: ' + str(result['errors']))
            require(stats['active'] == stats['leased'] == 0, 'Request or database lease survived a phase')
            require(stats['completed'] == result['successful'], 'Client/server completion counts differ')
            require(stats['peak_active'] <= 64 and stats['database']['errors'] == 0, 'Native resource/query failure')
            if case == 'json':
                require(stats['database']['queries'] == 0, 'JSON profile unexpectedly touched the database')
            else:
                expected_queries = 1 if case == 'orm-read' else 3
                require(stats['database']['queries'] == expected_queries * result['successful'],
                        'Unexpected query count for the defined workload')
            if case == 'orm-write':
                with sqlite3.connect(project / 'load.sqlite') as database:
                    for identifier, body in result['last_writes'].items():
                        persisted = database.execute('SELECT body FROM load_items WHERE id=?', (int(identifier),)).fetchone()
                        require(persisted == (body,), f'Write was not persisted for row {identifier}')
            result.pop('last_writes')
            return result
        except Exception as error:
            if result is not None:
                raise PhaseFailure(str(error), result) from error
            raise
        finally:
            if resources:
                resources.stop.set()
                resources.thread.join(timeout=2)
            if client.poll() is None:
                client.kill()
                client.wait(timeout=5)


def shutdown_under_load(process, project, port, token, concurrency):
    request(port, token, '/bench/reset', 'POST')
    with ThreadPoolExecutor(max_workers=concurrency) as clients:
        futures = [clients.submit(request, port, token, f'/bench/drain?client={index}')
                   for index in range(concurrency)]
        try:
            deadline = time.monotonic() + 5
            while time.monotonic() < deadline:
                stats = request(port, token, '/bench/stats')
                if stats['admitted_drain'] == stats['active'] == concurrency:
                    break
                time.sleep(0.01)
            else:
                raise RuntimeError('Not all drain requests were admitted')
            started = time.perf_counter()
            process.send_signal(signal.SIGTERM)
            # Keep all admitted handlers suspended until the listener stops.
            # This proves SIGTERM was handled with requests still active.
            deadline = time.monotonic() + 3
            stopped_accepting = False
            while time.monotonic() < deadline:
                with socket.socket() as probe:
                    probe.settimeout(0.1)
                    if probe.connect_ex(('127.0.0.1', port)) != 0:
                        stopped_accepting = True
                        break
                time.sleep(0.01)
            require(stopped_accepting and process.poll() is None, 'Server did not drain admitted requests after SIGTERM')
            (project / 'drain.release').touch()
            for index, future in enumerate(futures):
                require(future.result(timeout=5) == {'drained': True, 'client': str(index)}, 'Lost admitted drain response')
            code = process.wait(timeout=6)
            elapsed = time.perf_counter() - started
            stopped = json.loads((project / 'stopped.json').read_text())
            require(code == 0 and stopped == {'active': 0, 'completed': concurrency, 'leased': 0},
                    'Unclean shutdown: ' + str(stopped))
            return dict(signal='SIGTERM', admitted=concurrency, completed=concurrency,
                        listener_closed_before_release=True, seconds=elapsed, exit_code=code,
                        shutdown_budget_seconds=5, final=stopped)
        finally:
            (project / 'drain.release').touch()


def command(arguments, environment, log, cwd=None):
    with log.open('a') as output:
        result = subprocess.run(arguments, cwd=cwd, env=environment, stdout=output,
                                stderr=subprocess.STDOUT, timeout=300)
    require(result.returncode == 0, 'Command failed: ' + ' '.join(arguments) + '\n' + log.read_text()[-6000:])


def environment_metadata(project, sdk):
    cache = {}
    for line in (project / '.gungnir/build/CMakeCache.txt').read_text().splitlines():
        if '=' in line and ':' in line.split('=', 1)[0] and not line.startswith('//'):
            key, value = line.split('=', 1)
            cache[key.split(':', 1)[0]] = value
    require(cache.get('CMAKE_BUILD_TYPE') == 'Release', 'Application was not built in Release mode')
    compiler = subprocess.check_output([cache['CMAKE_CXX_COMPILER'], '--version'], text=True, timeout=10).splitlines()[0]
    fixture = Path(__file__).parent / 'application'
    digest = hashlib.sha256()
    for path in sorted(p for p in fixture.rglob('*') if p.is_file()):
        digest.update(str(path.relative_to(fixture)).encode() + b'\0' + path.read_bytes())
    source = Path(__file__).resolve().parent
    revision = subprocess.run(['git', '-C', str(source), 'rev-parse', 'HEAD'], capture_output=True, text=True, timeout=10)
    dirty = subprocess.run(['git', '-C', str(source), 'status', '--porcelain'], capture_output=True, text=True, timeout=10)
    cpu_model = next((line.split(':', 1)[1].strip() for line in Path('/proc/cpuinfo').read_text().splitlines()
                      if line.startswith('model name')), platform.processor())
    quota = Path('/sys/fs/cgroup/cpu.max')
    return dict(platform=platform.platform(), machine=platform.machine(), cpu_model=cpu_model,
                available_cpus=len(os.sched_getaffinity(0)), cpu_quota=quota.read_text().strip() if quota.exists() else '',
                python=platform.python_version(), compiler=compiler, compiler_path=cache['CMAKE_CXX_COMPILER'],
                build_type=cache['CMAKE_BUILD_TYPE'], release_flags=cache.get('CMAKE_CXX_FLAGS_RELEASE', ''),
                sdk=dict(line.split('=', 1) for line in (sdk / 'share/gungnir/sdk.txt').read_text().splitlines()),
                commit=os.environ.get('GITHUB_SHA') or revision.stdout.strip(),
                source_dirty=bool(dirty.stdout) if dirty.returncode == 0 else None,
                fixture_sha256=digest.hexdigest(), generator_sha256=hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),
                application_sha256=hashlib.sha256((project / '.gungnir/build/app').read_bytes()).hexdigest())


def markdown(payload):
    lines = ['# Gungnir application load results', '',
             f"Status: **{payload['status']}**. Commit: `{payload.get('environment', {}).get('commit') or 'unknown'}`.", '',
             'HTTP/1.1 loopback, one Release server, authenticated requests, SQLite WAL/FULL, one database connection.',
             'Separate asyncio generator; closed-loop clients, one request in flight per client, keep-alive with normal reconnects.', '',
             '| Workload | Clients | Runs | Median successful req/s | Median p50 ms | Median p95 ms | Median p99 ms | Peak server RSS MiB | Median DB p95 ms |',
             '| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: |']
    groups = {}
    for result in payload['results']:
        groups.setdefault((result['case'], result['concurrency']), []).append(result)
    for (case, concurrency), results in groups.items():
        med = lambda values: statistics.median(values)
        lines.append(f"| {case} | {concurrency} | {len(results)} | "
                     f"{med(r['successful_requests_per_second'] for r in results):.1f} | "
                     f"{med(r['latency']['p50_upper_ms'] for r in results):.2f} | "
                     f"{med(r['latency']['p95_upper_ms'] for r in results):.2f} | "
                     f"{med(r['latency']['p99_upper_ms'] for r in results):.2f} | "
                     f"{max(r['server']['peak_rss_bytes'] for r in results) / 1048576:.2f} | "
                     f"{med(r['native']['database']['p95_upper_ms'] for r in results):.2f} |")
    if 'soak' in payload:
        soak = payload['soak']
        lines += ['', f"ORM read soak: {soak['seconds']:.2f}s, {soak['concurrency']} clients, "
                  f"{soak['successful']} correct responses, "
                  f"RSS {soak['server']['baseline_rss_bytes'] / 1048576:.2f} → "
                  f"{soak['server']['final_rss_bytes'] / 1048576:.2f} MiB."]
    if 'shutdown' in payload:
        shutdown = payload['shutdown']
        lines += ['', f"SIGTERM: {shutdown['completed']}/{shutdown['admitted']} admitted requests drained, "
                  f"exit {shutdown['exit_code']}, {shutdown['seconds']:.3f}s, zero active requests/leases."]
    if 'error' in payload:
        lines += ['', 'Failure: ' + payload['error']]
    lines += ['', 'Percentiles are histogram upper bounds (0.01 ms buckets); table percentiles are medians across runs.',
              'JSON retains each run, overflow counts, errors, HTTP status counts, DB metrics, CPU and RSS samples.',
              'Shared-runner timing is observational. This fixture excludes TLS, proxy, remote database, cache, sessions and queues.',
              'Use like-for-like machines and methodology for comparisons; no Laravel comparison or production capacity claim.', '']
    return '\n'.join(lines)


def run_application(args, payload):
    sdk = Path(args.sdk).resolve()
    cli = sdk / 'bin/gungnir'
    require(cli.is_file(), 'No installed gungnir CLI at ' + str(cli))
    settings = dict(line.split('=', 1) for line in (sdk / 'share/gungnir/sdk.txt').read_text().splitlines())
    require(settings.get('sqlite') == 'ON', 'The installed SDK needs SQLite enabled')
    environment = {key: value for key, value in os.environ.items()
                   if not key.startswith(('APP_', 'DB_', 'GUNGNIR_LOAD_'))}
    environment.update(GUNGNIR_CMAKE_PREFIX=str(sdk), CMAKE_BUILD_PARALLEL_LEVEL='2',
                       GUNGNIR_LOAD_TOKEN=secrets.token_hex(32))
    token = environment['GUNGNIR_LOAD_TOKEN']
    with tempfile.TemporaryDirectory(prefix='gungnir-application-load-') as temporary:
        project = Path(temporary) / 'application'
        shutil.copytree(Path(__file__).parent / 'application', project)
        log = Path(temporary) / 'application-load.log'
        log.touch()
        process = None
        try:
            with socket.socket() as available:
                available.bind(('127.0.0.1', 0))
                port = available.getsockname()[1]
            configuration = dict(APP_ENV='production', APP_DEBUG='false', APP_HOST='127.0.0.1', APP_PORT=str(port),
                                 DB_CONNECTION='sqlite', DB_DATABASE=str(project / 'load.sqlite'),
                                 DB_POOL_SIZE='1', DB_POOL_ACQUIRE_TIMEOUT_MS='2000')
            (project / '.env').write_text(''.join(f'{key}={value}\n' for key, value in configuration.items()))
            command([str(cli), 'build', '--release'], environment, log, cwd=project)
            command([str(cli), 'migrate', '--release'], environment, log, cwd=project)
            with sqlite3.connect(project / 'load.sqlite') as database:
                database.executemany('INSERT INTO load_items(id, body) VALUES(?, ?)',
                                     ((identifier, seed_body(identifier)) for identifier in range(1, args.rows + 1)))
            payload['environment'] = environment_metadata(project, sdk)
            with log.open('a') as output:
                process = subprocess.Popen([str(project / '.gungnir/build/app')], cwd=project, env=environment,
                                           stdout=output, stderr=subprocess.STDOUT)
            deadline = time.monotonic() + 20
            while time.monotonic() < deadline:
                require(process.poll() is None, 'Application exited before readiness: ' + log.read_text()[-6000:])
                try:
                    stats = request(port, token, '/bench/stats')
                    payload['policy']['sqlite_version'] = stats['sqlite_version']
                    break
                except OSError:
                    time.sleep(0.05)
            else:
                raise RuntimeError('Application did not start')
            request(port, 'incorrect', '/json', expected=401)
            for case in ('json', 'orm-read', 'orm-write'):
                for concurrency in args.concurrency:
                    for sample in range(1, args.samples + 1):
                        result = measure(args, case, concurrency, args.duration, process, project,
                                         environment, token, port, sample)
                        payload['results'].append(result)
                        print(f"{case} clients={concurrency} sample={sample}: "
                              f"{result['successful_requests_per_second']:.1f} correct req/s, "
                              f"p95={result['latency']['p95_upper_ms']:.2f}ms", flush=True)
            payload['soak'] = measure(args, 'orm-read', max(args.concurrency), args.soak_duration, process,
                                      project, environment, token, port, 1)
            # Leave admission capacity for the observation request while the
            # coroutine barrier is held. At most 32 drain clients are needed.
            payload['shutdown'] = shutdown_under_load(process, project, port, token, min(max(args.concurrency), 32))
        finally:
            if process is not None and process.poll() is None:
                process.terminate()
                try:
                    process.wait(timeout=7)
                except subprocess.TimeoutExpired:
                    process.kill()
                    process.wait(timeout=5)
            require(token not in log.read_text(), 'Credential appeared in application logs')
            shutil.copyfile(log, Path(args.output).with_suffix('.log'))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--sdk', help='Installed Release SDK with SQLite and tools enabled')
    parser.add_argument('--output', default='application-load.json')
    parser.add_argument('--summary', default='application-load.md')
    parser.add_argument('--concurrency', default='1,8,32')
    parser.add_argument('--duration', type=float, default=3)
    parser.add_argument('--samples', type=int, default=3)
    parser.add_argument('--warmup', type=float, default=0.25)
    parser.add_argument('--soak-duration', type=float, default=30)
    parser.add_argument('--rows', type=int, default=4096)
    parser.add_argument('--timeout', type=float, default=5)
    parser.add_argument('--smoke', action='store_true')
    parser.add_argument('--client', action='store_true', help=argparse.SUPPRESS)
    parser.add_argument('--case', choices=('json', 'orm-read', 'orm-write'), help=argparse.SUPPRESS)
    parser.add_argument('--clients', type=int, help=argparse.SUPPRESS)
    parser.add_argument('--port', type=int, help=argparse.SUPPRESS)
    args = parser.parse_args()
    if platform.system() != 'Linux':
        parser.error('This reference measures Linux /proc resources and POSIX SIGTERM')
    if args.smoke:
        args.duration, args.samples, args.concurrency, args.soak_duration = 0.25, 1, '1,8', 0.5
    try:
        args.concurrency = sorted(set(int(value) for value in args.concurrency.split(',')))
    except ValueError:
        parser.error('--concurrency requires comma-separated integers')
    if not args.concurrency or not all(1 <= value <= 64 for value in args.concurrency):
        parser.error('--concurrency must stay between 1 and the fixture dispatch limit of 64')
    if not (0.1 <= args.duration <= 120 and 0.1 <= args.soak_duration <= 120 and
            0 <= args.warmup <= 10 and 1 <= args.samples <= 10 and 128 <= args.rows <= 100000 and
            0.1 <= args.timeout <= 30):
        parser.error('Use duration/soak 0.1..120s, warmup 0..10s, samples 1..10, rows 128..100000, timeout 0.1..30s')
    if args.client:
        require(args.case and args.clients and args.port, 'Incomplete client arguments')
        try:
            result = asyncio.run(generate(args))
            print(json.dumps(result), flush=True)
            return 0 if result['failed'] == 0 else 1
        except Exception as error:
            print(json.dumps({'client_error': str(error)}), flush=True)
            return 1
    if not args.sdk:
        parser.error('--sdk is required')
    for target in (args.output, args.summary):
        Path(target).resolve().parent.mkdir(parents=True, exist_ok=True)
    payload = dict(schema=1, suite='application-load', status='running', smoke=args.smoke,
                   measured_at=datetime.now(timezone.utc).isoformat(), results=[],
                   policy=dict(concurrency=args.concurrency, duration_seconds=args.duration, samples=args.samples,
                               warmup_seconds=args.warmup, soak_seconds=args.soak_duration, seeded_rows=args.rows,
                               timeout_seconds=args.timeout, protocol='HTTP/1.1', tls=False, proxy=False,
                               database='sqlite', journal_mode='WAL', synchronous='FULL', database_pool_size=1,
                               max_connections=128, max_active_dispatches=64, max_requests_per_connection=100,
                               shutdown_concurrency=min(max(args.concurrency), 32),
                               client_model='closed-loop, one keep-alive connection per client, no retries',
                               server_processes=1, histogram_percentiles='bucket upper bounds',
                               timing_gate=False))
    try:
        run_application(args, payload)
        payload['status'] = 'passed'
    except Exception as error:
        payload['status'], payload['error'] = 'failed', str(error)
        if isinstance(error, PhaseFailure):
            payload['failed_phase'] = error.result
    Path(args.output).write_text(json.dumps(payload, indent=2, sort_keys=True) + '\n')
    summary = markdown(payload)
    Path(args.summary).write_text(summary)
    print(summary)
    return 0 if payload['status'] == 'passed' else 1


if __name__ == '__main__':
    raise SystemExit(main())
