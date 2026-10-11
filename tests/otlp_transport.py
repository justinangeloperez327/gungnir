"""Fault-injected OTLP/HTTP transport; not a substitute for Collector acceptance."""
from collections import Counter
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from email.utils import formatdate
import json
import os
from pathlib import Path
import ssl
import subprocess
import sys
import tempfile
import threading
import time

binary = sys.argv[1]

class Server(ThreadingHTTPServer):
    daemon_threads = True

class Handler(BaseHTTPRequestHandler):
    def do_POST(self):
        body = self.rfile.read(int(self.headers['Content-Length']))
        with self.server.lock:
            number = self.server.counts[self.path]
            self.server.counts[self.path] += 1
            self.server.received.append((self.path, body, dict(self.headers)))
        status, response = 200, b'{}'
        case = self.server.scenario
        if case == 'retry' and number == 0:
            status = 503
        if case == 'outage':
            status = 503
        if self.path == '/v1/traces':
            if case == 'rejected': status = 400
            if case == 'malformed': response = b'{bad JSON'
            if case == 'nested': response = b'[' * 40 + b'0' + b']' * 40
            if case == 'oversized': response = b'x' * 4096
            if case in ('retry_after', 'retry_date'): status = 429
            if case == 'redirect': status = 302
            if case == 'partial': response = b'{"partialSuccess":{"rejectedSpans":"1","errorMessage":"exporter-private"}}'
            if case == 'warning': response = b'{"partialSuccess":{"rejectedSpans":"0","errorMessage":"exporter-private"}}'
            if case == 'bad_partial': response = b'{"partialSuccess":{"rejectedSpans":"999"}}'
        elif case == 'partial':
            response = b'{"partialSuccess":{"rejectedDataPoints":"2"}}'
        if case in ('shutdown', 'flush_deadline'): time.sleep(2)
        if case == 'flush_snapshot': time.sleep(0.005)
        self.send_response(status)
        self.send_header('Content-Type', 'application/json')
        self.send_header('Content-Length', str(len(response)))
        if case == 'retry_after': self.send_header('Retry-After', '1')
        if case == 'retry_date': self.send_header('Retry-After', formatdate(time.time() + 30, usegmt=True))
        if case == 'redirect': self.send_header('Location', '/redirected')
        self.end_headers()
        try:
            self.wfile.write(response)
        except (BrokenPipeError, ConnectionResetError):
            pass
    def log_message(self, *arguments): pass

for case in ('success', 'retry', 'rejected', 'partial', 'warning', 'bad_partial', 'redirect', 'malformed', 'oversized', 'nested',
             'retry_after', 'retry_date', 'outage', 'overflow', 'invalid_records', 'shutdown', 'flush_deadline', 'flush_snapshot'):
    with Server(('127.0.0.1', 0), Handler) as server:
        server.scenario, server.lock, server.counts, server.received = case, threading.Lock(), Counter(), []
        thread = threading.Thread(target=server.serve_forever, daemon=True); thread.start()
        try:
            result = subprocess.run([binary, f'http://127.0.0.1:{server.server_port}', case], text=True, capture_output=True, timeout=10)
            assert result.returncode == 0, (case, result.stdout, result.stderr)
            assert 'exporter-private' not in result.stdout + result.stderr
            with server.lock:
                counts, received = server.counts.copy(), list(server.received)
            if case in ('success', 'rejected', 'partial', 'warning', 'bad_partial', 'redirect', 'malformed', 'oversized', 'nested', 'retry_after', 'retry_date'):
                assert counts == {'/v1/traces': 1, '/v1/metrics': 1}, (case, counts)
            if case == 'retry': assert counts == {'/v1/traces': 2, '/v1/metrics': 2}, counts
            if case == 'outage': assert counts == {'/v1/traces': 3, '/v1/metrics': 3}, counts
            if case == 'invalid_records': assert not counts, counts
            if case in ('shutdown', 'flush_deadline'): assert counts['/v1/traces'] == 1, counts
            for path, body, headers in received:
                assert headers['Content-Type'] == 'application/json'
                assert headers['Authorization'] == 'Bearer exporter-private'
                payload = json.loads(body)
                if path == '/v1/traces':
                    spans = payload['resourceSpans'][0]['scopeSpans'][0]['spans']
                    assert all(len(span['traceId']) == 32 and len(span['spanId']) == 16 for span in spans)
                elif path == '/v1/metrics':
                    point = payload['resourceMetrics'][0]['scopeMetrics'][0]['metrics'][0]['sum']['dataPoints'][0]
                    assert point['asDouble'] == 1.23456789012345e-12
            if case == 'retry':
                for path in ('/v1/traces', '/v1/metrics'):
                    bodies = [body for destination, body, _ in received if destination == path]
                    assert bodies[0] == bodies[1], (case, path)
            print(result.stdout.strip())
        finally:
            server.shutdown(); thread.join(timeout=5)

# A private root is supported explicitly. Verification failures are terminal:
# neither a bad certificate nor a mismatched hostname is retried or bypassed.
class TLSServer(Server):
    def get_request(self):
        self.accepts += 1
        return super().get_request()

with tempfile.TemporaryDirectory(prefix='gungnir-otlp-tls-') as temporary:
    certificate, key = Path(temporary) / 'certificate.pem', Path(temporary) / 'key.pem'
    subprocess.run(['openssl', 'req', '-x509', '-newkey', 'rsa:2048', '-nodes', '-days', '1',
                    '-keyout', str(key), '-out', str(certificate), '-subj', '/CN=localhost',
                    '-addext', 'subjectAltName=DNS:localhost'], check=True, capture_output=True)
    context = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER); context.load_cert_chain(certificate, key)
    for case in ('tls_untrusted', 'tls_trusted', 'tls_hostname'):
        with TLSServer(('127.0.0.1', 0), Handler) as server:
            server.socket = context.wrap_socket(server.socket, server_side=True)
            server.scenario, server.lock, server.counts, server.received, server.accepts = case, threading.Lock(), Counter(), [], 0
            thread = threading.Thread(target=server.serve_forever, daemon=True); thread.start()
            try:
                host = '127.0.0.1' if case == 'tls_hostname' else 'localhost'
                result = subprocess.run([binary, f'https://{host}:{server.server_port}', case],
                                        env=dict(os.environ, GUNGNIR_OTLP_TEST_CA=str(certificate)), text=True, capture_output=True, timeout=10)
                assert result.returncode == 0, (case, result.stdout, result.stderr)
                assert server.accepts == 2, (case, server.accepts)
                if case == 'tls_trusted':
                    assert server.counts == {'/v1/traces':1, '/v1/metrics':1}
                    assert all(headers['Authorization'] == 'Bearer exporter-private' for _, _, headers in server.received)
                else: assert not server.counts
                print(result.stdout.strip())
            finally:
                server.shutdown(); thread.join(timeout=5)
