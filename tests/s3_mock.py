"""Exercise the S3 adapter against a local, signed-request-aware HTTP fixture."""
import http.server
import subprocess
import sys
import threading
import urllib.parse

objects = {}
failures = []

class Handler(http.server.BaseHTTPRequestHandler):
    def log_message(self, *_):
        pass

    def reply(self, status, body=b""):
        self.send_response(status)
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        if self.command != "HEAD":
            self.wfile.write(body)

    def handle_request(self):
        auth = self.headers.get("Authorization", "")
        if not (auth.startswith("AWS4-HMAC-SHA256 Credential=test-key/")
                and "/us-east-1/s3/aws4_request" in auth
                and self.headers.get("X-Amz-Date")
                and self.headers.get("X-Amz-Security-Token") == "test-session"):
            failures.append("Request missing expected SigV4 credentials or session token")
            return self.reply(403)
        parsed = urllib.parse.urlsplit(self.path)
        path = urllib.parse.unquote(parsed.path)
        assert path.startswith("/test/")
        key = path[len("/test/"):]
        query = urllib.parse.parse_qs(parsed.query)
        if self.command == "GET" and "list-type" in query:
            assert query["encoding-type"] == ["url"]
            prefix = query.get("prefix", [""])[0]
            keys = sorted(k for k in objects if k.startswith(prefix))
            position = 1 if "continuation-token" in query else 0
            if position:
                assert query["continuation-token"] == ["next&token"]
            truncated = prefix == "repeat/" or position + 1 < len(keys)
            body = "<ListBucketResult><IsTruncated>" + str(truncated).lower() + "</IsTruncated>"
            for item in keys[position:position+1]:
                body += "<Contents><Key>" + urllib.parse.quote(item, safe="") + "</Key></Contents>"
            if truncated:
                body += "<NextContinuationToken>next&amp;token</NextContinuationToken>"
            return self.reply(200, (body + "</ListBucketResult>").encode())
        if key == "error":
            return self.reply(503)
        if key == "large":
            return self.reply(200, b"x" * 2048)
        if self.command == "PUT":
            objects[key] = self.rfile.read(int(self.headers.get("Content-Length", 0)))
            return self.reply(200)
        if self.command == "DELETE":
            objects.pop(key, None)
            return self.reply(204)
        return self.reply(200, objects[key]) if key in objects else self.reply(404)

    do_GET = do_HEAD = do_PUT = do_DELETE = handle_request

with http.server.ThreadingHTTPServer(("127.0.0.1", 0), Handler) as server:
    thread = threading.Thread(target=server.serve_forever, daemon=True)
    thread.start()
    try:
        result = subprocess.run([sys.argv[1], f"http://127.0.0.1:{server.server_port}"], timeout=25)
        if failures:
            raise AssertionError(failures)
        sys.exit(result.returncode)
    finally:
        server.shutdown()
        thread.join()
