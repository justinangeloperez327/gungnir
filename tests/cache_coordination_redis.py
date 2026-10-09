"""Installed .gnr application: two live instances share Redis coordination."""
from concurrent.futures import ThreadPoolExecutor
from http.cookies import SimpleCookie
import json
import os
from pathlib import Path
import signal
import socket
import subprocess
import sys
import tempfile
import time
import urllib.error
import urllib.request
import uuid

build, cli_name = Path(sys.argv[1]).resolve(), Path(sys.argv[2]).name
env = os.environ.copy()
env["CMAKE_BUILD_PARALLEL_LEVEL"] = "2"
prefix = "gungnir:coordination-app:" + uuid.uuid4().hex + ":"


def redis_command(*parts):
    with socket.create_connection((env.get("GUNGNIR_REDIS_HOST", "127.0.0.1"), int(env.get("GUNGNIR_REDIS_PORT", "6379"))), timeout=5) as connection:
        data = [str(part).encode() for part in parts]
        connection.sendall(b"*" + str(len(data)).encode() + b"\r\n" + b"".join(b"$" + str(len(part)).encode() + b"\r\n" + part + b"\r\n" for part in data))
        stream = connection.makefile("rb")

        def reply():
            line = stream.readline()
            assert line.endswith(b"\r\n"), "Incomplete Redis reply"
            kind, body = line[:1], line[1:-2]
            if kind == b"-":
                raise AssertionError(body.decode())
            if kind in (b"+", b":"):
                return body.decode() if kind == b"+" else int(body)
            if kind == b"*":
                return [reply() for _ in range(int(body))]
            assert kind == b"$", line
            size = int(body)
            if size == -1:
                return None
            value = stream.read(size)
            assert stream.read(2) == b"\r\n"
            return value.decode()

        return reply()


with tempfile.TemporaryDirectory(prefix="gungnir-cache-coordination-") as temporary:
    root = Path(temporary)
    stage, project = root / "sdk", root / "project"
    subprocess.run(["cmake", "--install", str(build), "--prefix", str(stage)], check=True, stdout=subprocess.DEVNULL)
    env["GUNGNIR_CMAKE_PREFIX"] = str(stage)
    cli = stage / "bin" / cli_name
    subprocess.run([str(cli), "new", "CacheCoordination", str(project)], env=env, check=True, stdout=subprocess.DEVNULL)

    def source(path, content):
        target = project / path
        target.parent.mkdir(parents=True, exist_ok=True)
        target.write_text(content)

    source("app/controllers/Shared.gnr", """controller Shared {
        inject Cache cache;
        inject Storage storage;
        index() { return text('ready'); }
        put(Request request) { cache.put('payload', request.json(), 60); return noContent(); }
        get() { if (cache.has('payload')) { return json(cache.get('payload')); } return noContent(); }
        flush() { cache.flush(); return noContent(); }
        nullValue() { cache.put('null', null, 60); return json(cache.has('null')); }
        readNull() { return json({exists: cache.has('null'), value: cache.get('null')}); }
        remember() { return json(cache.rememberLocked('summary', 60, 5000, () => {
            storage.put('factory-entered', 'yes');
            while (!storage.exists('factory-release')) {}
            return {kind: 'shared', count: 42};
        })); }
        hold(string id) {
            storage.put('entered-' + id, 'yes');
            while (!storage.exists('release-' + id)) {}
            return text('released');
        }
        limited() { return text('admitted'); }
        sessionPut(Request request) { request.session().put('locale', 'en'); return text('saved'); }
        sessionGet(Request request) { return text(request.session().get('locale')); }
        rotate(Request request) { request.session().regenerate(); return text(request.session().id()); }
    }""")
    source("app/middleware/SharedLock.gnr", """middleware SharedLock {
        inject Cache cache;
        async handle(Request request, Next next) {
            const lock = cache.lock('critical', 1500);
            if (!lock.acquire()) { return text('busy', 409); }
            return await next(request);
        }
    }""")
    source("routes/web.gnr", """Route::get('/', Shared::index);
    Route::post('/cache', Shared::put);
    Route::get('/cache', Shared::get);
    Route::get('/flush', Shared::flush);
    Route::get('/null/write', Shared::nullValue);
    Route::get('/null/read', Shared::readNull);
    Route::get('/remember', Shared::remember);
    Route::get('/hold/{id}', Shared::hold).middleware('SharedLock');
    Route::get('/limited', Shared::limited);
    Route::get('/session/put', Shared::sessionPut);
    Route::get('/session/get', Shared::sessionGet);
    Route::get('/session/rotate', Shared::rotate);""")
    source(".env", "APP_HOST=127.0.0.1\nDB_CONNECTION=\n")
    source("bootstrap/app.hpp", r'''#pragma once
#include <gungnir/core/services.hpp>
#include <gungnir/cache/redis_store.hpp>
#include <gungnir/cache/redis_lock.hpp>
#include <gungnir/http/redis_rate_limit.hpp>
#include <gungnir/http/security.hpp>
#include <gungnir/session/middleware.hpp>
#include <gungnir/session/redis_store.hpp>
#include <gungnir/storage/local_disk.hpp>
#include <cstdlib>
namespace bootstrap {
inline void configure(gungnir::Application& app) {
    using namespace gungnir;
    cache::RedisSettings settings;
    settings.host = std::getenv("GUNGNIR_REDIS_HOST") ? std::getenv("GUNGNIR_REDIS_HOST") : "127.0.0.1";
    settings.port = std::getenv("GUNGNIR_REDIS_PORT") ? static_cast<std::uint16_t>(std::stoi(std::getenv("GUNGNIR_REDIS_PORT"))) : 6379;
    const std::string prefix = std::getenv("GUNGNIR_COORDINATION_PREFIX");
    settings.prefix = prefix + "values:";
    cache::RedisLockSettings lock_settings;
    lock_settings.host = settings.host; lock_settings.port = settings.port;
    lock_settings.prefix = prefix + "locks:";
    ServiceOptions options;
    options.cache = std::make_shared<cache::RedisStore>(settings);
    options.cache_locks = std::make_shared<cache::RedisLockStore>(lock_settings);
    options.storage = std::make_shared<storage::Manager>();
    options.storage->add("local", std::make_shared<storage::LocalDisk>(app.base_path() / "storage"));
    app.provider<ServicesProvider>(std::move(options));
    session::RedisSessionSettings sessions; sessions.redis = settings;
    sessions.redis.prefix = prefix + "sessions:"; sessions.lifetime = std::chrono::seconds{2};
    app.router().use(session::middleware(std::make_shared<session::RedisStore>(sessions), {.secure = false}));
    http::RedisRateLimitSettings rate;
    rate.client.nodes = {{settings.host, settings.port}}; rate.prefix = prefix + "rate:";
    auto limiter = http::rate_limit({.requests = 3, .window = std::chrono::seconds{5}},
        std::make_shared<http::RedisRateLimitStore>(rate), "shared-api");
    app.router().use([limiter](Request& request, Next next) -> Task<Response> {
        if (request.path() == "/limited") co_return co_await limiter(request, std::move(next));
        co_return co_await next(request);
    });
}
inline void boot(gungnir::Application&) {}
}
''')
    built = subprocess.run([str(cli), "build"], cwd=project, env=env, text=True, capture_output=True, timeout=240)
    assert built.returncode == 0, built.stdout + built.stderr
    binary = project / ".gungnir/build/app"
    processes = []
    logs = []

    def request(port, path, payload=None, cookie=None):
        headers = {} if cookie is None else {"Cookie": "gungnir_session=" + cookie}
        if payload is not None:
            headers["Content-Type"] = "application/json"
        incoming = urllib.request.Request(f"http://127.0.0.1:{port}{path}",
            data=None if payload is None else json.dumps(payload, ensure_ascii=False).encode(), headers=headers)
        try:
            response = urllib.request.urlopen(incoming, timeout=8)
        except urllib.error.HTTPError as error:
            response = error
        with response:
            return response.status, response.read().decode(), {key.lower(): value for key, value in response.headers.items()}

    def start(namespace):
        with socket.socket() as reservation:
            reservation.bind(("127.0.0.1", 0)); port = reservation.getsockname()[1]
        process_env = dict(env, APP_PORT=str(port), GUNGNIR_COORDINATION_PREFIX=namespace)
        log = (root / f"instance-{len(logs)}.log").open("w")
        logs.append(log)
        process = subprocess.Popen([str(binary)], cwd=project, env=process_env, stdout=log, stderr=log)
        processes.append(process)
        deadline = time.monotonic() + 20
        while time.monotonic() < deadline:
            assert process.poll() is None, "Application exited during startup"
            try:
                if request(port, "/")[:2] == (200, "ready"):
                    return process, port
            except OSError:
                pass
            time.sleep(0.05)
        raise AssertionError("Application did not start")

    def entered(name):
        deadline = time.monotonic() + 5
        marker = project / "storage" / name
        while time.monotonic() < deadline:
            if marker.exists():
                return
            time.sleep(0.01)
        raise AssertionError("Operation did not reach " + name)

    def permit(name):
        source("storage/" + name, "yes")

    def cookie(headers):
        parsed = SimpleCookie(); parsed.load(headers["set-cookie"])
        return parsed["gungnir_session"].value

    def stop(process):
        if process.poll() is None:
            process.send_signal(signal.SIGTERM)
            process.wait(timeout=10)

    try:
        first, a = start(prefix)
        second, b = start(prefix)
        _, isolated = start(prefix + "isolated:")
        payload = {"title": "共享缓存 👋", "items": [1, None, True], "unsigned": 18446744073709551615, "ratio": 3.0}
        assert request(a, "/cache", payload)[0] == 204
        assert json.loads(request(b, "/cache")[1]) == payload
        assert request(isolated, "/cache")[0] == 204
        assert request(a, "/null/write")[:2] == (200, "true")
        assert json.loads(request(b, "/null/read")[1]) == {"exists": True, "value": None}
        stop(first)
        first, a = start(prefix)
        assert json.loads(request(a, "/cache")[1]) == payload

        with ThreadPoolExecutor(max_workers=1) as work:
            filling = work.submit(request, a, "/remember")
            try:
                entered("factory-entered")
                assert request(b, "/remember")[0] == 500
            finally:
                permit("factory-release")
            expected = {"kind": "shared", "count": 42}
            assert json.loads(filling.result()[1]) == expected
            assert json.loads(request(b, "/remember")[1]) == expected

        with ThreadPoolExecutor(max_workers=2) as work:
            holding = work.submit(request, a, "/hold/normal")
            try:
                entered("entered-normal")
                assert request(b, "/hold/blocked")[0] == 409
                assert request(b, "/flush")[0] == 204
                assert request(b, "/hold/blocked")[0] == 409
            finally:
                permit("release-normal")
            assert holding.result()[:2] == (200, "released")

            # Expired original resumes while its replacement is still held.
            old = work.submit(request, a, "/hold/old")
            replacement = None
            try:
                entered("entered-old")
                time.sleep(1.7)
                replacement = work.submit(request, b, "/hold/new")
                entered("entered-new")
                permit("release-old")
                assert old.result()[0] == 200
                assert request(a, "/hold/stale-release")[0] == 409
            finally:
                permit("release-old"); permit("release-new")
            assert replacement.result()[0] == 200

            abandoned = work.submit(request, a, "/hold/crash")
            try:
                entered("entered-crash")
                first.kill(); first.wait(timeout=5)
                assert request(b, "/hold/blocked")[0] == 409
                time.sleep(1.7)
                permit("release-recovered")
                assert request(b, "/hold/recovered")[:2] == (200, "released")
            finally:
                permit("release-crash")
                try:
                    abandoned.result()
                except (OSError, TimeoutError):
                    pass

        first, a = start(prefix)
        attempts = [request(a, "/limited"), request(b, "/limited"), request(a, "/limited"), request(b, "/limited")]
        assert [item[0] for item in attempts] == [200, 200, 200, 429]
        assert [item[2]["x-ratelimit-remaining"] for item in attempts] == ["2", "1", "0", "0"]
        assert 1 <= int(attempts[-1][2]["retry-after"]) <= 5
        assert request(isolated, "/limited")[0] == 200
        stop(second)
        second, b = start(prefix)
        assert request(b, "/limited")[0] == 429
        time.sleep(5.1)
        assert request(b, "/limited")[0] == 200

        session_id = cookie(request(a, "/session/put")[2])
        assert request(b, "/session/get", cookie=session_id)[:2] == (200, "en")
        rotated = request(b, "/session/rotate", cookie=session_id)
        new_id = cookie(rotated[2])
        assert new_id != session_id and request(a, "/session/get", cookie=new_id)[1] == "en"
        assert request(a, "/session/get", cookie=session_id)[1] == ""
        time.sleep(2.1)
        assert request(b, "/session/get", cookie=new_id)[1] == ""
    except BaseException:
        for log in logs:
            log.flush()
            print(Path(log.name).read_text(), file=sys.stderr)
        raise
    finally:
        # Permit any fixture wait before graceful shutdown; kill only if needed.
        for name in ("normal", "old", "new", "crash", "blocked", "stale-release", "recovered"):
            permit("release-" + name)
        permit("factory-release")
        for process in processes:
            try:
                stop(process)
            except subprocess.TimeoutExpired:
                process.kill(); process.wait(timeout=5)
        for log in logs:
            log.close()
        cursor = "0"
        while True:
            cursor, keys = redis_command("SCAN", cursor, "COUNT", 256)
            for key in keys:
                if key.startswith(prefix):
                    redis_command("DEL", key)
            if cursor == "0":
                break
print("Installed Redis applications: shared JSON, owned locks, crash recovery, global quotas and session lifecycle passed")
