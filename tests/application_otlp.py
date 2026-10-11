"""Installed .gnr HTTP/SQLite/Redis worker telemetry through the official Collector."""
from concurrent.futures import ThreadPoolExecutor
import hashlib
import http.client
import json
import os
from pathlib import Path
import signal
import socket
import sqlite3
import subprocess
import sys
import tempfile
import time

build, cli_name = Path(sys.argv[1]).resolve(), Path(sys.argv[2]).name
env = os.environ.copy()
env['CMAKE_BUILD_PARALLEL_LEVEL'] = '2'
env.setdefault('CURL_ROOT', '/usr')
collector = Path(env['GUNGNIR_OTELCOL']).resolve()
assert collector.is_file(), 'GUNGNIR_OTELCOL must name the official otelcol-contrib binary'

def free_port():
    with socket.socket() as reservation:
        reservation.bind(('127.0.0.1', 0))
        return reservation.getsockname()[1]

def wait_port(process, port, log):
    deadline = time.monotonic() + 20
    while time.monotonic() < deadline:
        assert process.poll() is None, log.read_text()
        try:
            with socket.create_connection(('127.0.0.1', port), timeout=0.1): return
        except OSError: time.sleep(0.025)
    raise AssertionError(log.read_text())

with tempfile.TemporaryDirectory(prefix='gungnir-application-otlp-') as temporary:
    root = Path(temporary)
    stage, project = root / 'sdk', root / 'project'
    trace_file, metric_file = root / 'traces.jsonl', root / 'metrics.jsonl'
    collector_port = free_port()
    config = root / 'collector.yaml'
    config.write_text(f'''receivers:
  otlp:
    protocols:
      http:
        endpoint: 127.0.0.1:{collector_port}
exporters:
  file/traces:
    path: {trace_file}
    format: json
  file/metrics:
    path: {metric_file}
    format: json
service:
  telemetry:
    logs:
      level: error
    metrics:
      level: none
  pipelines:
    traces:
      receivers: [otlp]
      exporters: [file/traces]
    metrics:
      receivers: [otlp]
      exporters: [file/metrics]
''')
    collector_log = root / 'collector.log'
    processes = []
    with collector_log.open('w') as collector_output:
        collector_process = subprocess.Popen([str(collector), '--config', str(config)], stdout=collector_output, stderr=collector_output)
        processes.append(collector_process)
        try:
            wait_port(collector_process, collector_port, collector_log)
            env['TEST_OTLP_ENDPOINT'] = f'http://127.0.0.1:{collector_port}'
            subprocess.run(['cmake', '--install', str(build), '--prefix', str(stage)], check=True, stdout=subprocess.DEVNULL)
            cli = stage / 'bin' / cli_name
            env['GUNGNIR_CMAKE_PREFIX'] = str(stage)
            subprocess.run([str(cli), 'new', 'ExternalObservability', str(project)], env=env, check=True, stdout=subprocess.DEVNULL)

            def source(path, text):
                target = project / path
                target.parent.mkdir(parents=True, exist_ok=True)
                target.write_text(text, encoding='utf-8')

            def run(*arguments, role='command'):
                result = subprocess.run([str(cli), *arguments], cwd=project, env=dict(env, TEST_ROLE=role), text=True, capture_output=True, timeout=240)
                assert result.returncode == 0, result.stdout + result.stderr
                return result.stdout

            source('app/models/Observation.gnr', '''model Observation {
                table = 'observations'; timestamps = false;
                fillable = ['label', 'secret']; hidden = ['secret'];
                string label; string secret;
            }''')
            source('database/migrations/CreateObservations.gnr', '''migration CreateObservations {
                up() { Table::create('observations', (table) => {table.id(); table.string('label'); table.string('secret');}); }
                down() { Table::dropIfExists('observations'); }
            }''')
            source('app/jobs/Record.gnr', '''import app.models.Observation;
            job Record {
                inject Logger logger; inject Telemetry telemetry;
                string marker;
                handle() {
                    const span = telemetry.span('worker.custom', {'marker':marker, 'api_key':'worker-sensitive'});
                    logger.info('Worker record', {'marker':marker, 'token':'worker-sensitive'});
                    telemetry.counter('demo.jobs', 1, {'operation':'record'});
                    telemetry.histogram('demo.tiny', 1.23456789012345e-12);
                    if (marker == 'fail') { span.error('Intentional worker failure'); span.end(); throw 'Intentional worker failure'; }
                    Observation::create({'label':marker + ' worker', 'secret':'db-sensitive'});
                    span.end();
                }
            }''')
            source('app/controllers/Observed.gnr', '''import app.jobs.Record; import app.models.Observation;
            controller Observed {
                inject Queue queue; inject Logger logger; inject Telemetry telemetry;
                index() { return text('ready'); }
                work(Request request, string marker) {
                    const span = telemetry.span('web.custom', {'marker':marker, 'password':'web-sensitive'});
                    logger.info('Producer record', {'marker':marker, 'request_id':request.header('X-Request-ID'), 'authorization':request.header('Authorization'), 'cookie':request.header('Cookie')});
                    Observation::create({'label':marker, 'secret':'db-sensitive'});
                    telemetry.counter('demo.requests', 1, {'operation':'enqueue', 'access_token':'web-sensitive'});
                    telemetry.gauge('demo.active', 1);
                    const job = queue.dispatch(Record(marker), 1);
                    const trace = span.traceId(); span.end();
                    return json({'job':job, 'trace':trace});
                }
                fail() { throw 'Intentional HTTP failure'; }
            }''')
            source('app/middleware/Resume.gnr', '''middleware Resume {
                inject Logger logger; inject Telemetry telemetry;
                async handle(Request request, Next next) {
                    let response = await next(request);
                    const resumed = telemetry.span('web.resume');
                    logger.info('Resumed request', {'request_id':request.header('X-Request-ID')});
                    response.header('X-Trace', resumed.traceId()); resumed.end();
                    return response;
                }
            }''')
            source('routes/web.gnr', '''Route::get('/', Observed::index);
            Route::get('/work/{marker}', Observed::work).middleware('Resume');
            Route::get('/error', Observed::fail);''')
            prefix = 'gungnir:observability:' + str(time.time_ns()) + ':'
            source('bootstrap/app.hpp', r'''#pragma once
#include <gungnir/core/services.hpp>
#include <gungnir/core/timer.hpp>
#include <gungnir/queue/redis_driver.hpp>
#include <gungnir/observability/otlp_http_exporter.hpp>
#include <gungnir/logging/json_stream_sink.hpp>
#include <cstdlib>
#include <iostream>
namespace bootstrap {
inline void configure(gungnir::Application& app) {
    using namespace gungnir;
#ifndef GNR_ADAPTER_OTLP
#error Generated apps must link the installed OTLP adapter
#endif
    const String role = std::getenv("TEST_ROLE") ? std::getenv("TEST_ROLE") : "producer";
    observability::OtlpHttpSettings settings; settings.endpoint = std::getenv("TEST_OTLP_ENDPOINT");
    settings.resource.service_name = "observed-application"; settings.resource.deployment_environment = "acceptance";
    settings.resource.attributes = {{"service.instance.id", role}};
    settings.headers = {{"Authorization", "Bearer exporter-sensitive"}};
    // Delay is intentionally long: graceful process exit must drain pending data.
    settings.batch_delay = std::chrono::hours{1}; settings.max_batch_size = 128; settings.max_queue_size = 1024;
    settings.request_timeout = std::chrono::milliseconds{200};
    observability::OtlpHttpPolicy policy; policy.export_timeout = std::chrono::milliseconds{600}; policy.max_attempts = 2;
    policy.flush_timeout = std::chrono::seconds{2}; policy.shutdown_timeout = std::chrono::milliseconds{600};
    auto exporter = std::make_shared<observability::OtlpHttpExporter>(settings, policy);
    app.on_shutdown([exporter, role](Application& app) {
        std::cout << Json::object({{"kind","exporter-summary"}, {"role",role}, {"dropped",static_cast<UInt64>(exporter->dropped())},
            {"error",exporter->last_error()}, {"shutdown_errors",static_cast<UInt64>(app.shutdown_errors().size())}}).dump() << '\n';
    });
    ServiceOptions options; options.tracer = std::make_shared<observability::Tracer>(exporter);
    options.meter = std::make_shared<observability::Meter>(exporter);
    options.logger = std::make_shared<logging::Logger>(); options.logger->sink(std::make_shared<logging::JsonStreamSink>(std::cout));
    queue::RedisSettings queue; queue.host = std::getenv("GUNGNIR_REDIS_HOST") ? std::getenv("GUNGNIR_REDIS_HOST") : "127.0.0.1";
    queue.port = std::getenv("GUNGNIR_REDIS_PORT") ? static_cast<std::uint16_t>(std::stoi(std::getenv("GUNGNIR_REDIS_PORT"))) : 6379;
    queue.database = 15; queue.prefix = QUEUE_PREFIX;
    options.queue = std::make_shared<queue::RedisDriver>(queue);
    app.provider<ServicesProvider>(options);
    // Exercise a timer suspension before generated middleware/controller execution.
    app.router().use([](http::Request& request, http::Next next) -> Task<http::Response> {
        co_await sleep_for(std::chrono::milliseconds{5}); co_return co_await next(request);
    });
}
inline void boot(gungnir::Application&) {}
}
'''.replace('QUEUE_PREFIX', json.dumps(prefix)))
            source('.env', f'APP_HOST=127.0.0.1\nDB_CONNECTION=sqlite\nDB_DATABASE={project / "observations.sqlite"}\n')
            run('build'); run('migrate')
            binary = project / '.gungnir/build/app'
            app_logs, command_logs = [], []

            def request(port, path, traceparent='', request_id=''):
                connection = http.client.HTTPConnection('127.0.0.1', port, timeout=5)
                headers = {'Authorization':'Bearer web-sensitive', 'Cookie':'private=web-sensitive', 'X-Request-ID':request_id}
                if traceparent: headers['traceparent'] = traceparent
                connection.request('GET', path, headers=headers)
                response = connection.getresponse(); body = response.read().decode()
                result = response.status, body, {key.lower():value for key, value in response.getheaders()}
                connection.close(); return result

            def start(role):
                port = free_port(); log_path = root / f'{role}.log'; app_logs.append(log_path)
                output = log_path.open('w')
                process = subprocess.Popen([str(binary)], cwd=project, env=dict(env, APP_PORT=str(port), TEST_ROLE=role), stdout=output, stderr=output)
                processes.append(process); output.close()
                wait_port(process, port, log_path)
                assert request(port, '/')[1] == 'ready', log_path.read_text()
                return process, port

            def stop(process):
                process.send_signal(signal.SIGTERM)
                assert process.wait(timeout=10) == 0

            first, port = start('producer')
            expectations = {}
            def emit(marker):
                trace = hashlib.sha256(('trace:' + marker).encode()).hexdigest()[:32]
                parent = hashlib.sha256(('parent:' + marker).encode()).hexdigest()[:16]
                flags = '00' if marker == 'ok-5' else '01'
                status, body, headers = request(port, '/work/' + marker, f'00-{trace}-{parent}-{flags}', 'request-' + marker)
                assert status == 200, (status, body)
                result = json.loads(body)
                assert result['trace'] == trace and headers['x-trace'] == trace, (marker, result, headers)
                return marker, trace, parent, result['job']
            with ThreadPoolExecutor(max_workers=4) as executor:
                for marker, trace, parent, job in executor.map(emit, ['ok-' + str(index) for index in range(6)] + ['fail']):
                    expectations[marker] = trace, parent, job
            # Malformed and absent headers must start independent roots, without
            # inheriting a previous request on the same execution thread.
            invalid = request(port, '/work/invalid', '00-' + '0' * 32 + '-' + '1' * 16 + '-01', 'request-invalid')
            fresh = request(port, '/work/fresh', request_id='request-fresh')
            assert invalid[0] == fresh[0] == 200
            for marker, response in (('invalid', invalid), ('fresh', fresh)):
                result = json.loads(response[1]); assert len(result['trace']) == 32
                assert result['trace'] not in [item[0] for item in expectations.values()]
                expectations[marker] = result['trace'], '', result['job']
            assert request(port, '/error')[0] == 500
            stop(first)
            # New worker processes reconstruct queued context after the producer exits.
            for index in range(len(expectations)):
                output = run('queue:work', '--once', role=f'worker-{index}')
                assert '1 job(s) processed' in output, output
                command_logs.append(output)
            output = run('queue:work', '--once', role='worker-empty')
            assert '0 job(s) processed' in output; command_logs.append(output)
            stop(collector_process)

            def lines(path):
                return [json.loads(line) for line in path.read_text().splitlines() if line.strip()]
            traces, metrics = [], []
            for envelope in lines(trace_file):
                for resource in envelope['resourceSpans']:
                    attributes = {item['key']:item['value']['stringValue'] for item in resource['resource']['attributes']}
                    assert attributes['service.name'] == 'observed-application'
                    assert attributes['service.version'] == '1.0.0' and attributes['deployment.environment.name'] == 'acceptance'
                    for scope in resource['scopeSpans']:
                        assert scope['scope']['name'] == 'gungnir' and scope['scope']['version'] == '1.0.0'
                        traces.extend((attributes, span) for span in scope['spans'])
            for envelope in lines(metric_file):
                for resource in envelope['resourceMetrics']:
                    for scope in resource['scopeMetrics']: metrics.extend(scope['metrics'])
            assert traces and metrics, collector_log.read_text()
            def attrs(span): return {item['key']:item['value']['stringValue'] for item in span.get('attributes', [])}
            for marker, (trace, parent, job) in expectations.items():
                related = [(resource, span) for resource, span in traces if span['traceId'] == trace]
                http_spans = [span for _, span in related if span['name'] == 'http.server.request']
                assert len(http_spans) == 1 and http_spans[0].get('parentSpanId', '') == parent, (marker, http_spans)
                http_span = http_spans[0]; assert int(http_span['kind']) == 2
                assert attrs(http_span)['http.response.status_code'] == '200'
                jobs = [(resource, span) for resource, span in related if span['name'] == 'queue.job']
                assert len(jobs) == 1, (marker, related)
                resource, queue_span = jobs[0]
                assert resource['service.instance.id'].startswith('worker-')
                assert queue_span['parentSpanId'] == http_span['spanId'] and int(queue_span['kind']) == 5
                assert attrs(queue_span)['messaging.message.id'] == job
                custom = [span for _, span in related if span['name'] == 'worker.custom']
                assert len(custom) == 1 and custom[0]['parentSpanId'] == queue_span['spanId']
                assert attrs(custom[0])['api_key'] == '[redacted]'
                web = [span for _, span in related if span['name'] == 'web.custom']
                assert len(web) == 1 and web[0]['parentSpanId'] == http_span['spanId']
                assert attrs(web[0])['password'] == '[redacted]'
                resumed = [span for _, span in related if span['name'] == 'web.resume']
                assert len(resumed) == 1 and resumed[0]['parentSpanId'] == http_span['spanId']
                database = [span for _, span in related if span['name'] == 'database.query']
                assert any(span['parentSpanId'] == http_span['spanId'] for span in database)
                if marker == 'fail':
                    assert int(queue_span['status']['code']) == int(custom[0]['status']['code']) == 2
                else:
                    assert int(queue_span['status']['code']) == 1
                    assert any(span['parentSpanId'] == queue_span['spanId'] for span in database)
                for _, span in related:
                    assert int(span['endTimeUnixNano']) >= int(span['startTimeUnixNano']) > 0
                    assert int(span['flags']) == 1
            errors = [span for _, span in traces if span['name'] == 'http.server.request' and attrs(span).get('url.path') == '/error']
            assert len(errors) == 1 and int(errors[0]['status']['code']) == 2
            names = {metric['name'] for metric in metrics}
            assert {'demo.requests', 'demo.jobs', 'demo.active', 'demo.tiny', 'http.server.request.count',
                    'db.client.operation.duration', 'messaging.process.duration'} <= names, names
            tiny = [point for metric in metrics if metric['name'] == 'demo.tiny' for point in metric['histogram']['dataPoints']]
            assert len(tiny) == len(expectations) and all(point['sum'] == 1.23456789012345e-12 for point in tiny)
            exemplars = [exemplar for metric in metrics for kind in ('sum', 'gauge', 'histogram')
                         for point in metric.get(kind, {}).get('dataPoints', []) for exemplar in point.get('exemplars', [])]
            known = {(span['traceId'], span['spanId']) for _, span in traces}
            assert exemplars and all((exemplar['traceId'], exemplar['spanId']) in known for exemplar in exemplars)
            for trace, _, _ in expectations.values(): assert any(item['traceId'] == trace for item in exemplars)
            with sqlite3.connect(project / 'observations.sqlite') as connection:
                labels = {row[0] for row in connection.execute('SELECT label FROM observations')}
                for marker in expectations:
                    assert marker in labels
                    if marker != 'fail': assert marker + ' worker' in labels

            def log_records(text):
                return [json.loads(line) for line in text.splitlines() if line.startswith('{')]
            all_log_text = '\n'.join(path.read_text() for path in app_logs) + '\n' + '\n'.join(command_logs)
            logs = log_records(all_log_text)
            summaries = [record for record in logs if record.get('kind') == 'exporter-summary']
            assert len(summaries) == len(expectations) + 2, summaries
            assert all(record['dropped'] == record['shutdown_errors'] == 0 and record['error'] == '' for record in summaries), summaries
            for marker, (trace, _, _) in expectations.items():
                producer_logs = [record for record in logs if record.get('message') == 'Producer record' and record['context']['marker'] == marker]
                worker_logs = [record for record in logs if record.get('message') == 'Worker record' and record['context']['marker'] == marker]
                assert len(producer_logs) == len(worker_logs) == 1
                assert producer_logs[0]['context']['trace_id'] == worker_logs[0]['context']['trace_id'] == trace
                assert producer_logs[0]['context']['authorization'] == worker_logs[0]['context']['token'] == '[redacted]'
            # Exporter credentials and application secret fields must not reach logs
            # or any collector resource/span/metric attributes.
            exported = trace_file.read_text() + metric_file.read_text() + all_log_text
            for secret in ('exporter-sensitive', 'web-sensitive', 'worker-sensitive', 'db-sensitive'): assert secret not in exported, secret

            # Collector outage remains observable while requests/SQLite/queue work.
            outage, port = start('outage')
            response = request(port, '/work/outage', request_id='request-outage')
            assert response[0] == 200, response
            stop(outage)
            outage_records = log_records(app_logs[-1].read_text())
            summary = [record for record in outage_records if record.get('kind') == 'exporter-summary']
            assert len(summary) == 1 and summary[0]['dropped'] > 0 and summary[0]['error'] and summary[0]['shutdown_errors'] == 0, summary
            assert '1 job(s) processed' in run('queue:work', '--once', role='worker-outage')
            with sqlite3.connect(project / 'observations.sqlite') as connection:
                assert connection.execute("SELECT count(*) FROM observations WHERE label IN ('outage','outage worker')").fetchone()[0] == 2
            print(f'Official Collector accepted HTTP/SQLite/Redis worker traces, metrics, exemplars and redaction for {len(expectations)} independent traces; graceful drain and outage isolation passed')
        finally:
            for process in reversed(processes):
                if process.poll() is None:
                    process.terminate()
                    try: process.wait(timeout=10)
                    except subprocess.TimeoutExpired: process.kill(); process.wait(timeout=5)
