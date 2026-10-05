"""Installed generated application across Redis producer/worker/scheduler processes."""
import json
import os
from pathlib import Path
import signal
import socket
import subprocess
import sys
import tempfile
import time
import urllib.request
build, cli_name = Path(sys.argv[1]).resolve(), Path(sys.argv[2]).name
env = os.environ.copy()
env["CMAKE_BUILD_PARALLEL_LEVEL"] = "2"
with tempfile.TemporaryDirectory(prefix="gungnir-background-redis-") as temporary:
    root = Path(temporary)
    stage, project = root / "sdk", root / "project"
    subprocess.run(["cmake", "--install", str(build), "--prefix", str(stage)], check=True, stdout=subprocess.DEVNULL)
    cli = stage / "bin" / cli_name
    env["GUNGNIR_CMAKE_PREFIX"] = str(stage)
    subprocess.run([str(cli), "new", "BackgroundRedis", str(project)], env=env, check=True, stdout=subprocess.DEVNULL)
    def run(*arguments):
        result = subprocess.run([str(cli), *arguments], cwd=project, env=env, text=True, capture_output=True, timeout=240)
        assert result.returncode == 0, result.stdout + result.stderr
        return result.stdout
    def source(path, text):
        target = project / path
        target.parent.mkdir(parents=True, exist_ok=True)
        target.write_text(text)
    source("app/jobs/Write.gnr", """job Write {
        inject Storage storage;
        string key;
        bool requirePermit;
        handle() {
            if (requirePermit && !storage.exists('permit.txt')) { throw 'retry me'; }
            storage.put(key, 'done');
        }
    }""")
    source("app/controllers/Producer.gnr", """import app.jobs.Write;
    controller Producer {
        inject Queue queue;
        index() { return text('ready'); }
        send() { return text(queue.dispatch(Write('job.txt', false), 2)); }
        fail() { return text(queue.dispatch(Write('retry.txt', true), 2)); }
        failures() { return json(queue.failed()); }
        retry(string id) { return json(queue.retry(id)); }
    }""")
    source("routes/web.gnr", """Route::get('/', Producer::index);
    Route::get('/send', Producer::send);
    Route::get('/fail', Producer::fail);
    Route::get('/failures', Producer::failures);
    Route::get('/retry/{id}', Producer::retry);""")
    source("routes/console.gnr", """import app.jobs.Write;
    function void schedule(Scheduler schedule) {
        schedule.cron('shared', '* * * * *', Write('scheduled.txt', false)).timezone('UTC').withoutOverlapping().onOneServer();
    }""")
    prefix = "gungnir:application-test:" + str(time.time_ns()) + ":"
    header = project / "bootstrap/app.hpp"
    native = header.read_text().replace("#include <gungnir/queue/memory_driver.hpp>", "#include <gungnir/queue/redis_driver.hpp>\n#include <gungnir/scheduler/redis_lock.hpp>\n#include <cstdlib>")
    native = native.replace("services.queue = std::make_shared<gungnir::queue::MemoryDriver>();", """gungnir::queue::RedisSettings queue;
        queue.host = std::getenv("GUNGNIR_REDIS_HOST") ? std::getenv("GUNGNIR_REDIS_HOST") : "127.0.0.1";
        queue.port = std::getenv("GUNGNIR_REDIS_PORT") ? static_cast<std::uint16_t>(std::stoi(std::getenv("GUNGNIR_REDIS_PORT"))) : 6379;
        queue.database = 15;
        queue.prefix = """ + json.dumps(prefix + "queue:") + """;
        services.queue = std::make_shared<gungnir::queue::RedisDriver>(queue);""")
    native = native.replace("services.scheduler_locks = std::make_shared<gungnir::scheduler::MemoryLockStore>();", """gungnir::scheduler::RedisLockSettings locks;
        locks.host = queue.host; locks.port = queue.port; locks.database = 15;
        locks.prefix = """ + json.dumps(prefix + "locks:") + """;
        services.scheduler_locks = std::make_shared<gungnir::scheduler::RedisLockStore>(locks);""")
    header.write_text(native)
    with socket.socket() as reservation:
        reservation.bind(("127.0.0.1", 0))
        port = reservation.getsockname()[1]
    source(".env", f"APP_HOST=127.0.0.1\nAPP_PORT={port}\nDB_CONNECTION=\n")
    run("build")
    def request(path):
        with urllib.request.urlopen(f"http://127.0.0.1:{port}{path}", timeout=5) as result:
            return result.read().decode()
    binary = project / ".gungnir/build/app"
    def start(log):
        process = subprocess.Popen([str(binary)], cwd=project, env=env, stdout=log, stderr=log)
        deadline = time.monotonic() + 20
        while time.monotonic() < deadline:
            assert process.poll() is None, "Producer exited"
            try:
                if request("/") == "ready":
                    return process
            except OSError:
                pass
            time.sleep(0.05)
        raise AssertionError("Producer did not start")
    with (project / "redis.log").open("w") as log:
        process = start(log)
        try:
            assert request("/send")
        finally:
            process.send_signal(signal.SIGTERM)
            process.wait(timeout=10)
        # Work survives producer exit and is reconstructed by a new application.
        assert "1 job(s) processed" in run("queue:work", "--once")
        assert (project / "storage/job.txt").read_text() == "done"
        assert "0 job(s) processed" in run("queue:work", "--once")
        process = start(log)
        try:
            job_id = request("/fail")
            assert "1 job(s) processed" in run("queue:work", "--once")
            assert "1 job(s) processed" in run("queue:work", "--once")
            failure = json.loads(request("/failures"))
            assert len(failure) == 1 and failure[0]["id"] == job_id and failure[0]["attempts"] == 2
            assert "payload" not in failure[0]
            source("storage/permit.txt", "yes")
            assert json.loads(request("/retry/" + job_id)) is True
            assert "1 job(s) processed" in run("queue:work", "--once")
            assert (project / "storage/retry.txt").read_text() == "done"
            assert json.loads(request("/failures")) == []
        finally:
            process.send_signal(signal.SIGTERM)
            process.wait(timeout=10)
        # Fresh scheduler processes share one occurrence lock through Redis.
        minute = int(time.time() // 60)
        assert "1 task(s) executed" in run("schedule:run")
        second = run("schedule:run")
        if int(time.time() // 60) == minute:
            assert "0 task(s) executed" in second
        assert "1 job(s) processed" in run("queue:work", "--once")
        assert (project / "storage/scheduled.txt").read_text() == "done"
print("Generated Redis producer, worker restart, durable retries and shared scheduling passed")
