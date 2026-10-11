"""Exercise the installed Linux deployment through real TLS and process supervision."""
from concurrent.futures import ThreadPoolExecutor
import hashlib
import http.client
from http.cookies import SimpleCookie
import json
import os
from pathlib import Path
import secrets
import shutil
import signal
import socket
import sqlite3
import ssl
import subprocess
import sys
import tempfile
import time
import urllib.parse


def run(command, *, success=True, **options):
    result = subprocess.run(command, text=True, capture_output=True, timeout=240, **options)
    if success:
        assert result.returncode == 0, result.stdout + result.stderr
    else:
        assert result.returncode != 0, "Operation unexpectedly succeeded: " + command[-1]
    return result


def eventually(predicate, description, timeout=25):
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        try:
            if predicate():
                return
        except (OSError, ValueError, http.client.HTTPException, AssertionError):
            pass
        time.sleep(0.1)
    raise AssertionError("Timed out: " + description)


def ports():
    listeners = [socket.socket() for _ in range(5)]
    try:
        for listener in listeners:
            listener.bind(("127.0.0.1", 0))
        return [listener.getsockname()[1] for listener in listeners]
    finally:
        for listener in listeners:
            listener.close()


build = Path(sys.argv[1]).resolve()
environment = dict(os.environ, CMAKE_BUILD_PARALLEL_LEVEL="2")
with tempfile.TemporaryDirectory(prefix="gungnir-deployment-") as temporary:
    fixture = Path(temporary)
    stage, root, tls = fixture / "sdk", fixture / "deployment", fixture / "tls"
    run(["cmake", "--install", str(build), "--prefix", str(stage)], env=environment)
    example = stage / "share/gungnir/examples/production-deployment"
    assert (example / "project/.gungnir-project").is_file()
    script = example / "deploy.py"
    assert script.is_file() and (stage / "bin" / Path(sys.argv[2]).name).is_file()
    tls.mkdir(mode=0o700)
    run(["openssl", "req", "-x509", "-newkey", "rsa:2048", "-nodes", "-days", "1",
         "-subj", "/CN=Deployment test CA", "-keyout", str(tls / "ca.key"), "-out", str(tls / "ca.pem")])
    run(["openssl", "req", "-new", "-newkey", "rsa:2048", "-nodes", "-subj", "/CN=localhost",
         "-keyout", str(tls / "server.key"), "-out", str(tls / "server.csr")])
    (tls / "extensions").write_text("subjectAltName=DNS:localhost\nextendedKeyUsage=serverAuth\n")
    run(["openssl", "x509", "-req", "-in", str(tls / "server.csr"), "-CA", str(tls / "ca.pem"),
         "-CAkey", str(tls / "ca.key"), "-CAcreateserial", "-days", "1",
         "-extfile", str(tls / "extensions"), "-out", str(tls / "server.pem")])
    http_port, https_port, web_a, web_b, redis_port = ports()
    token, password = secrets.token_hex(32), secrets.token_hex(32)
    values = {
        "APP_ENV": "production", "APP_DEBUG": "false", "APP_HOST": "127.0.0.1",
        "APP_PUBLIC_HOST": "localhost", "APP_API_TOKEN": token, "APP_NAMESPACE": "acceptance",
        "DB_CONNECTION": "sqlite", "DB_DATABASE": str(root / "data/application.sqlite"),
        "DB_POOL_SIZE": "1", "DB_POOL_ACQUIRE_TIMEOUT_MS": "1000",
        "DEPLOY_DATA_DIR": str(root / "data"), "REDIS_PORT": str(redis_port),
        "REDIS_PASSWORD": password, "REQUEST_WORK_DELAY_MS": "750",
    }
    env_file = fixture / "deployment.env"
    env_file.write_text("".join(key + "=" + value + "\n" for key, value in values.items()))
    env_file.chmod(0o600)

    def operations(*arguments, success=True):
        return run([sys.executable, str(script), "--root", str(root), *arguments],
                   env=environment, success=success)

    operations("build", "--sdk", str(stage), "--revision", "first")
    operations("configure", "--revision", "first", "--env-file", str(env_file),
               "--certificate", str(tls / "server.pem"), "--private-key", str(tls / "server.key"),
               "--http-port", str(http_port), "--https-port", str(https_port),
               "--web-a-port", str(web_a), "--web-b-port", str(web_b))
    for private in ("deployment.env", "redis.conf", "supervisord.conf"):
        assert (root / "config" / private).stat().st_mode & 0o077 == 0
    metadata = json.loads((root / "deployment.json").read_text())
    tools = metadata["tools"]
    if shutil.which("systemd-analyze"):
        run(["systemd-analyze", "verify", str(root / "config/gungnir-example.service")], env=environment)
    # Some isolated runners prohibit AF_UNIX bind. This explicit local-only
    # fixture override retains real Supervisor and authenticated loopback RPC;
    # CI uses the shipped private Unix control socket without this override.
    tcp_control = os.environ.get("GUNGNIR_DEPLOYMENT_TEST_TCP_CONTROL") == "1"
    if tcp_control:
        listener = socket.socket()
        listener.bind(("127.0.0.1", 0))
        control_port = listener.getsockname()[1]
        listener.close()
        control_password = secrets.token_hex(32)
        configuration = root / "config/supervisord.conf"
        text = configuration.read_text()
        text = text.replace("[unix_http_server]\nfile=" + str(root / "run/supervisor.sock") + "\nchmod=0600",
                            "[inet_http_server]\nport=127.0.0.1:" + str(control_port) +
                            "\nusername=fixture\npassword=" + control_password)
        text = text.replace("serverurl=unix://" + str(root / "run/supervisor.sock"),
                            "serverurl=http://127.0.0.1:" + str(control_port) +
                            "\nusername=fixture\npassword=" + control_password)
        configuration.write_text(text)
        print("Local fixture override: authenticated loopback Supervisor control; CI tests private Unix control",
              flush=True)

    def control(*arguments):
        return run([tools["supervisorctl"], "-c", str(root / "config/supervisord.conf"), *arguments],
                   env=environment).stdout.strip()

    def pid(name):
        return int(control("pid", name))

    def redis(*arguments):
        return run([tools["redis-cli"], "-h", "127.0.0.1", "-p", str(redis_port), "--raw", *arguments],
                   env=dict(environment, REDISCLI_AUTH=password)).stdout.strip()

    trusted = ssl.create_default_context(cafile=str(tls / "ca.pem"))

    def request(path, data=None, *, port=None, authenticated=True, headers=None, cookie="", raw=None):
        connection = (http.client.HTTPConnection("127.0.0.1", port, timeout=12) if port else
                      http.client.HTTPSConnection("localhost", https_port, timeout=12, context=trusted))
        fields = {"Host": "localhost", "Content-Type": "application/x-www-form-urlencoded"}
        if port:
            fields["X-Forwarded-Proto"] = "https"
        if authenticated:
            fields["Authorization"] = "Bearer " + token
        if cookie:
            fields["Cookie"] = cookie
        fields.update(headers or {})
        body = raw if raw is not None else urllib.parse.urlencode(data) if data is not None else None
        connection.request("POST" if body is not None else "GET", path, body=body, headers=fields)
        response = connection.getresponse()
        result = response.status, response.read().decode(), dict((key.lower(), value) for key, value in response.getheaders())
        connection.close()
        return result

    def ready(port):
        response = request("/health/ready", port=port, authenticated=False)
        return response[0] == 200 and json.loads(response[1]) == {"ready": True}

    def processed(reference):
        response = request("/notes/" + reference)
        return response[0] == 200 and json.loads(response[1])["processed"] is True

    def submit(reference, body="private-note-body"):
        response = request("/notes", {"reference": reference, "body": body})
        assert response[0] == 202, response
        return response

    output = (fixture / "supervisor-output.log").open("w")
    supervisor = subprocess.Popen([tools["supervisord"], "-n", "-c", str(root / "config/supervisord.conf")],
                                  env=environment, stdout=output, stderr=subprocess.STDOUT)
    try:
        # First startup exposes liveness but never admits traffic before explicit schema setup.
        eventually(lambda: request("/health/live", port=web_a, authenticated=False)[0] == 200, "initial liveness")
        for port in (web_a, web_b):
            eventually(lambda: request("/health/ready", port=port, authenticated=False)[0] == 503, "missing schema readiness")
        run([str(root / "current/.gungnir/build/migrations"), "migrate"], cwd=root / "current",
            env=dict(environment, GUNGNIR_DEPLOY_ENV=str(root / "config/deployment.env")))
        operations("resume")
        if not tcp_control:
            assert (root / "run/supervisor.sock").stat().st_mode & 0o077 == 0
        print("Explicit migration and independent process readiness passed", flush=True)

        # TLS is authenticated by a CA, unknown SNI is rejected, and client proxy headers are replaced.
        assert request("/health/live", authenticated=False)[0] == 200
        for version in (ssl.TLSVersion.TLSv1_2, ssl.TLSVersion.TLSv1_3):
            context = ssl.create_default_context(cafile=str(tls / "ca.pem"))
            context.minimum_version = context.maximum_version = version
            with socket.create_connection(("127.0.0.1", https_port), timeout=5) as raw:
                with context.wrap_socket(raw, server_hostname="localhost") as secure:
                    assert secure.version() in ("TLSv1.2", "TLSv1.3")
        for context, hostname in ((ssl.create_default_context(), "localhost"), (trusted, "wrong.example")):
            try:
                with socket.create_connection(("127.0.0.1", https_port), timeout=5) as raw:
                    with context.wrap_socket(raw, server_hostname=hostname):
                        raise AssertionError("Untrusted TLS peer unexpectedly accepted")
            except ssl.SSLError:
                pass
        connection = http.client.HTTPConnection("127.0.0.1", http_port, timeout=5)
        connection.request("GET", "/notes", headers={"Host": "localhost"})
        redirect = connection.getresponse()
        assert redirect.status == 308 and redirect.getheader("Location") == f"https://localhost:{https_port}/notes"
        redirect.read()
        connection.close()
        assert request("/context", headers={"Host": "wrong.example"})[0] == 421
        assert request("/notes/shared", authenticated=False)[0] == 401
        assert request("/notes", {"reference": ""})[0] == 422
        assert request("/notes", raw="x=" + "a" * (33 * 1024))[0] == 413
        contexts = []
        for _ in range(8):
            response = request("/context", headers={"X-Forwarded-For": "203.0.113.8", "X-Forwarded-Proto": "http",
                                                   "Forwarded": "for=203.0.113.8;proto=http"})
            assert response[0] == 200 and "strict-transport-security" in response[2], response
            context = json.loads(response[1])
            assert context["secure"] is True and context["client_ip"] == "127.0.0.1", context
            contexts.append(context)
        assert {item["instance"] for item in contexts} == {"web-a", "web-b"}, contexts
        assert request("/context", port=web_a, headers={"X-Forwarded-Proto": "http"})[0] == 400
        print("Verified HTTPS, proxy trust, host validation, admission and authentication", flush=True)

        # SQL, Redis cache and secure server-side sessions survive across independent web processes.
        submit("shared")
        eventually(lambda: processed("shared"), "queued note completion")
        for port in (web_a, web_b):
            response = request("/notes/shared", port=port)
            assert response[0] == 200 and json.loads(response[1])["body"] == "private-note-body"
            assert json.loads(request("/cache/shared", port=port)[1]) == "private-note-body"
        response = request("/session", {}, port=web_a)
        assert response[0] == 200
        jar = SimpleCookie()
        jar.load(response[2]["set-cookie"])
        session = next(iter(jar.values()))
        assert session["secure"] and session["httponly"] and session["samesite"].lower() == "lax"
        cookie = session.key + "=" + session.value
        assert json.loads(request("/session", port=web_b, cookie=cookie)[1]) == "shared"
        heartbeat = root / "data/storage/scheduler-heartbeat.txt"
        eventually(lambda: heartbeat.exists(), "scheduled job storage effect")

        # Supervisor restarts actual native processes after a crash; queued work remains durable.
        control("stop", "application:worker")
        submit("pending")
        assert json.loads(request("/notes/pending")[1])["processed"] is False
        control("start", "application:worker")
        eventually(lambda: processed("pending"), "queue resumed after worker restart")
        for name in ("application:web-a", "application:worker", "application:scheduler"):
            old = pid(name)
            assert old > 0
            os.kill(old, signal.SIGKILL)
            eventually(lambda: pid(name) > 0 and pid(name) != old, "supervised crash recovery " + name)
        operations("resume")
        heartbeat.unlink()
        eventually(lambda: heartbeat.exists(), "scheduler recovery creates a new heartbeat")
        submit("after-crash")
        eventually(lambda: processed("after-crash"), "worker crash recovery processes new work")
        assert json.loads(request("/session", port=web_a, cookie=cookie)[1]) == "shared"

        # Send SIGTERM only after a request is admitted, while its bounded async work is outstanding.
        log = root / "logs/web-a.jsonl"
        before = log.stat().st_size
        old = pid("application:web-a")
        with ThreadPoolExecutor(max_workers=1) as executor:
            pending = executor.submit(request, "/notes", {"reference": "drained", "body": "private-note-body"}, port=web_a)
            eventually(lambda: '"Request started"' in log.read_text()[before:] and
                       '"/notes"' in log.read_text()[before:], "request admission before shutdown")
            os.kill(old, signal.SIGTERM)
            assert pending.result(timeout=12)[0] == 202
        eventually(lambda: pid("application:web-a") > 0 and pid("application:web-a") != old, "graceful web restart")
        eventually(lambda: processed("drained"), "drained mutation retained")
        print("Shared SQL/cache/sessions, worker/scheduler recovery and graceful HTTP drain passed", flush=True)

        # Dependency failure removes readiness/admission without turning liveness into a restart loop.
        control("stop", "redis")
        for port in (web_a, web_b):
            assert request("/health/live", port=port, authenticated=False)[0] == 200
            assert request("/health/ready", port=port, authenticated=False)[0] == 503
            assert request("/notes/shared", port=port)[0] == 503
        control("start", "redis")
        operations("resume")
        assert json.loads(request("/cache/shared")[1]) == "private-note-body"
        assert json.loads(request("/session", cookie=cookie)[1]) == "shared"
        old = pid("redis")
        redis("SAVE")
        os.kill(old, signal.SIGKILL)
        eventually(lambda: pid("redis") > 0 and pid("redis") != old, "Redis crash restart")
        operations("resume")
        assert processed("shared") and json.loads(request("/cache/shared")[1]) == "private-note-body"
        print("Dependency outage and persistent Redis restart passed", flush=True)

        # Coordinated snapshots include pending jobs, sessions and local files. Restore rejects corruption first.
        backup = fixture / "snapshot"
        operations("backup", "--output", str(backup), success=False)
        control("stop", "application:worker")
        submit("restored-pending")
        (root / "data/storage/marker.txt").write_text("snapshot")
        operations("pause")
        operations("backup", "--output", str(backup))
        operations("verify-backup", "--input", str(backup))
        assert not (backup / "deployment.env").exists()
        operations("resume")
        eventually(lambda: processed("restored-pending"), "pending work after backup")
        submit("newer")
        eventually(lambda: processed("newer"), "post-snapshot mutation")
        (root / "data/storage/marker.txt").write_text("newer")
        operations("pause")
        control("stop", "redis")
        corrupt = fixture / "corrupt"
        shutil.copytree(backup, corrupt)
        with (corrupt / "application.sqlite").open("ab") as stream:
            stream.write(b"tampered")
        operations("restore", "--input", str(corrupt), success=False)
        with sqlite3.connect(root / "data/application.sqlite") as database:
            assert database.execute("SELECT count(*) FROM notes WHERE reference='newer'").fetchone()[0] == 1
        assert (root / "data/storage/marker.txt").read_text() == "newer"
        operations("restore", "--input", str(backup))
        # A reboot during maintenance must not start a worker against partially restored persistence.
        blocked = run([str(root / "current/.gungnir/build/background"), "queue:work", "--once"],
                      cwd=root / "current", env=dict(environment, GUNGNIR_DEPLOY_ENV=str(root / "config/deployment.env"),
                                                    GUNGNIR_DEPLOY_INSTANCE="worker"), success=False)
        assert "maintenance" in blocked.stderr
        operations("resume")
        eventually(lambda: processed("restored-pending"), "restored queued work")
        assert request("/notes/newer")[0] == 404
        assert json.loads(request("/cache/newer")[1]) is None
        assert json.loads(request("/session", cookie=cookie)[1]) == "shared"
        assert (root / "data/storage/marker.txt").read_text() == "snapshot"
        assert "appendonly\nyes" in redis("CONFIG", "GET", "appendonly")
        old = pid("redis")
        os.kill(old, signal.SIGKILL)
        eventually(lambda: pid("redis") > 0 and pid("redis") != old, "restored AOF survives another crash")
        operations("resume")
        assert request("/notes/newer")[0] == 404 and processed("restored-pending")
        assert json.loads(request("/cache/shared")[1]) == "private-note-body"
        print("Verified backup, corrupt-restore refusal, RDB recovery and renewed AOF persistence", flush=True)

        # Copies isolate operation testing from compilation. Both successful releases run the native app;
        # the deliberately broken candidate is only a startup failure injected for rollback verification.
        def candidate(revision, broken=False):
            destination = root / "releases" / revision
            shutil.copytree(root / "releases/first", destination)
            (destination / "REVISION").write_text(revision + "\n")
            if broken:
                executable = destination / ".gungnir/build/app"
                executable.write_text("#!/bin/sh\nexit 1\n")
                executable.chmod(0o700)
            manifest = json.loads((destination / "release.json").read_text())
            manifest["revision"] = revision
            for name in manifest["sha256"]:
                with (destination / name).open("rb") as stream:
                    manifest["sha256"][name] = hashlib.file_digest(stream, "sha256").hexdigest()
            (destination / "release.json").write_text(json.dumps(manifest))

        candidate("second")
        operations("promote", "--revision", "second")
        assert json.loads(request("/context")[1])["revision"] == "second" and processed("shared")
        operations("promote", "--revision", "first")
        assert json.loads(request("/context")[1])["revision"] == "first"
        candidate("broken", broken=True)
        failed = operations("promote", "--revision", "broken", success=False)
        assert "previous release was restored" in failed.stderr
        assert json.loads(request("/context")[1])["revision"] == "first" and processed("shared")
        print("Manual promotion, rollback and failed-candidate recovery passed", flush=True)

        for override in ({"APP_DEBUG": "true"}, {"APP_API_TOKEN": ""}, {"DB_DATABASE": "/tmp/outside.sqlite"},
                         {"APP_HOST": "0.0.0.0"}):
            result = run([str(root / "current/.gungnir/build/app")], cwd=root / "current", success=False,
                         env=dict(environment, GUNGNIR_DEPLOY_ENV=str(root / "config/deployment.env"), **override))
            assert token not in result.stdout + result.stderr and password not in result.stdout + result.stderr
        combined = "\n".join(path.read_text(errors="replace") for path in (root / "logs").rglob("*") if path.is_file())
        for secret in (token, password, "private-note-body", session.value):
            assert secret not in combined, "Sensitive application value leaked into process logs"
        print("Strict production settings and protected log redaction passed", flush=True)
    except BaseException:
        for path in [fixture / "supervisor-output.log", *(root / "logs").glob("*.error.log"),
                     root / "logs/nginx-error.log", root / "logs/redis.log"]:
            if path.is_file():
                tail = "\n".join(path.read_text(errors="replace").splitlines()[-25:])
                print(path.name + ":\n" + tail.replace(token, "[redacted]").replace(password, "[redacted]"),
                      file=sys.stderr, flush=True)
        raise
    finally:
        if supervisor.poll() is None:
            supervisor.terminate()
            try:
                supervisor.wait(timeout=100)
            except subprocess.TimeoutExpired:
                supervisor.kill()
                supervisor.wait(timeout=10)
                raise AssertionError("Supervisor failed to stop its process groups")
        output.close()

print("Installed production deployment acceptance passed")
