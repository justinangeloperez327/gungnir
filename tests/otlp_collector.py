#!/usr/bin/env python3
import json
import os
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer

output = os.environ.get("GUNGNIR_OTLP_CAPTURE", "otlp-capture.jsonl")
port = int(os.environ.get("GUNGNIR_OTLP_PORT", "4319"))

class Handler(BaseHTTPRequestHandler):
    def do_POST(self):
        length = int(self.headers.get("Content-Length", "0"))
        body = self.rfile.read(length).decode("utf-8")
        json.loads(body)

        if self.path not in ("/v1/traces", "/v1/metrics"):
            self.send_response(404)
            self.end_headers()
            return

        with open(output, "a", encoding="utf-8") as stream:
            stream.write(json.dumps({"path": self.path, "body": json.loads(body)}))
            stream.write("\n")

        self.send_response(200)
        self.send_header("Content-Type", "application/json")
        self.send_header("Content-Length", "2")
        self.end_headers()
        self.wfile.write(b"{}")

    def log_message(self, format, *args):
        pass

ThreadingHTTPServer(("127.0.0.1", port), Handler).serve_forever()
