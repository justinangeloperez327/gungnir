"""Installed generated delivery through Redis, loopback SMTP and SQLite."""
import email
from email import policy
import json
import os
from pathlib import Path
import signal
import socket
import socketserver
import sqlite3
import subprocess
import sys
import tempfile
import threading
import time
import urllib.request

build, cli_name = Path(sys.argv[1]).resolve(), Path(sys.argv[2]).name
env = os.environ.copy()
env["CMAKE_BUILD_PARALLEL_LEVEL"] = "2"
# Prefer the provisioned system curl over unrelated local static installations.
env.setdefault("CURL_ROOT", "/usr")

class SMTP(socketserver.StreamRequestHandler):
    def handle(self):
        self.connection.settimeout(10)
        def reply(value):
            self.wfile.write(value + b"\r\n")
            self.wfile.flush()
        reply(b"220 localhost test SMTP")
        recipients = []
        while True:
            line = self.rfile.readline(65536)
            if not line:
                return
            command = line.split(b" ", 1)[0].strip().upper()
            if command in (b"EHLO", b"HELO"):
                reply(b"250-localhost\r\n250 SIZE 10485760")
            elif command == b"MAIL":
                recipients = []
                reply(b"250 sender accepted")
            elif command == b"RCPT":
                recipients.append(line.decode().strip())
                reply(b"250 recipient accepted")
            elif command == b"DATA":
                reply(b"354 send data")
                body = bytearray()
                while True:
                    chunk = self.rfile.readline(10485760)
                    if not chunk:
                        return
                    if chunk == b".\r\n":
                        break
                    body.extend(chunk[1:] if chunk.startswith(b"..") else chunk)
                with self.server.lock:
                    rejected = self.server.reject
                    if not rejected:
                        self.server.messages.append((recipients, bytes(body)))
                reply(b"451 temporary failure" if rejected else b"250 message accepted")
            elif command == b"QUIT":
                reply(b"221 bye")
                return
            elif command in (b"RSET", b"NOOP"):
                reply(b"250 ok")
            else:
                reply(b"502 unsupported test command")

class Server(socketserver.ThreadingTCPServer):
    allow_reuse_address = True
    daemon_threads = True

with Server(("127.0.0.1", 0), SMTP) as smtp, tempfile.TemporaryDirectory(prefix="gungnir-delivery-redis-") as temporary:
    smtp.lock, smtp.messages, smtp.reject = threading.Lock(), [], False
    thread = threading.Thread(target=smtp.serve_forever, daemon=True)
    thread.start()
    root = Path(temporary)
    stage, project = root / "sdk", root / "project"
    subprocess.run(["cmake", "--install", str(build), "--prefix", str(stage)], check=True, stdout=subprocess.DEVNULL)
    cli = stage / "bin" / cli_name
    env["GUNGNIR_CMAKE_PREFIX"] = str(stage)
    subprocess.run([str(cli), "new", "DeliveryRedis", str(project)], env=env, check=True, stdout=subprocess.DEVNULL)
    def source(path, text):
        target = project / path
        target.parent.mkdir(parents=True, exist_ok=True)
        target.write_text(text)
    def run(*arguments):
        result = subprocess.run([str(cli), *arguments], cwd=project, env=env, capture_output=True, text=True, timeout=240)
        assert result.returncode == 0, result.stdout + result.stderr
        return result.stdout
    source("app/models/Recipient.gnr", '''model Recipient {
        fillable = ["name", "email"]; hidden = ["email"];
        casts = {name: "string", email: "string"};
    }''')
    source("app/mail/Welcome.gnr", '''mail Welcome {
        string name;
        subject() { return "Welcome " + name; }
        text() { return "Hello " + name; }
    }''')
    source("app/notifications/Notice.gnr", '''import app.models.Recipient;
    import app.mail.Welcome;
    notification Notice {
        via(Recipient recipient) { return ["mail", "database", "audit"]; }
        toMail(Recipient recipient) { return Welcome(recipient.name); }
        toDatabase(Recipient recipient) { return {name: recipient.name}; }
    }''')
    source("app/controllers/Producer.gnr", '''import app.models.Recipient;
    import app.mail.Welcome;
    import app.notifications.Notice;
    controller Producer {
        inject Mail mail;
        inject Notifications notifications;
        inject Queue queue;
        index() { return text("ready"); }
        direct() { mail.to("direct@example.test").send(Welcome("Direct")); return noContent(); }
        send() { return text(mail.to("queued@example.test").cc("copy@example.test").bcc("private@example.test")
            .replyTo("reply@example.test").attach("bytes.bin", "a\\0b").queue(Welcome("Queued"), 2)); }
        notice() {
            let user = Recipient(); user.id = 42; user.name = "Ada"; user.email = "notice@example.test";
            return text(notifications.queue(user, Notice(), 2));
        }
        retryMail() { return text(mail.to("retry@example.test").queue(Welcome("Retry"), 2)); }
        failures() { return json(queue.failed()); }
        retry(string id) { return json(queue.retry(id)); }
    }''')
    source("routes/web.gnr", '''Route::get("/", Producer::index);
    Route::get("/direct", Producer::direct); Route::get("/send", Producer::send);
    Route::get("/notice", Producer::notice); Route::get("/retry-mail", Producer::retryMail);
    Route::get("/failures", Producer::failures); Route::get("/retry/{id}", Producer::retry);''')
    prefix = "gungnir:delivery-test:" + str(time.time_ns()) + ":"
    redis_host = env.get("GUNGNIR_REDIS_HOST", "127.0.0.1")
    redis_port = int(env.get("GUNGNIR_REDIS_PORT", "6379"))
    source("bootstrap/app.hpp", '''#pragma once
    #include <gungnir/core/services.hpp>
    #include <gungnir/queue/redis_driver.hpp>
    #include <gungnir/mail/smtp_transport.hpp>
    #include <fstream>
    namespace bootstrap {
    class Audit : public gungnir::notifications::Channel {
        std::filesystem::path path_;
    public:
        explicit Audit(std::filesystem::path path) : path_(std::move(path)) {}
        void send(std::string_view recipient, const gungnir::notifications::Notification& notification) override {
            std::ofstream file{path_, std::ios::app};
            file << recipient << " " << notification.database_payload()->dump() << "\\n";
            if (!file.good()) throw std::runtime_error("audit write failed");
        }
    };
    inline void configure(gungnir::Application& app) {
        gungnir::queue::RedisSettings redis;
        redis.host = ''' + json.dumps(redis_host) + ''';
        redis.port = ''' + str(redis_port) + ''';
        redis.database = 15; redis.prefix = ''' + json.dumps(prefix) + ''';
        gungnir::mail::SmtpSettings smtp;
        smtp.host = "127.0.0.1"; smtp.port = ''' + str(smtp.server_address[1]) + ''';
        smtp.security = gungnir::mail::SmtpSecurity::none;
        gungnir::ServiceOptions services;
        services.queue = std::make_shared<gungnir::queue::RedisDriver>(redis);
        services.mail = std::make_shared<gungnir::mail::SmtpTransport>(smtp);
        services.sender = {"sender@example.test", "Delivery"}; services.database_notifications = true;
        app.provider<gungnir::ServicesProvider>(services);
    }
    inline void boot(gungnir::Application& app) {
        app.database().connection()->execute("CREATE TABLE IF NOT EXISTS notifications (id TEXT PRIMARY KEY,type TEXT,recipient TEXT,data TEXT)");
        app.container().resolve<gungnir::notifications::Manager>()->channel("audit",std::make_shared<Audit>(app.base_path()/"audit.txt"));
    }
    }''')
    with socket.socket() as reservation:
        reservation.bind(("127.0.0.1", 0))
        port = reservation.getsockname()[1]
    db = project / "delivery.sqlite"
    source(".env", f"APP_HOST=127.0.0.1\nAPP_PORT={port}\nDB_CONNECTION=sqlite\nDB_DATABASE={db}\n")
    run("build")
    binary = project / ".gungnir/build/app"
    def request(path):
        with urllib.request.urlopen(f"http://127.0.0.1:{port}{path}", timeout=5) as result:
            return result.read().decode()
    def start(log):
        process = subprocess.Popen([str(binary)], cwd=project, env=env, stdout=log, stderr=log)
        deadline = time.monotonic() + 20
        while time.monotonic() < deadline:
            assert process.poll() is None, (project / "app.log").read_text()
            try:
                if request("/") == "ready":
                    return process
            except OSError:
                pass
            time.sleep(0.05)
        raise AssertionError("Producer did not start")
    def stop(process):
        process.send_signal(signal.SIGTERM)
        assert process.wait(timeout=10) == 0, (project / "app.log").read_text()
    with (project / "app.log").open("w") as log:
        process = start(log)
        try:
            request("/direct")
            assert len(smtp.messages) == 1
            assert request("/send") and request("/notice")
            assert len(smtp.messages) == 1
        finally:
            stop(process)
        assert "1 job(s) processed" in run("queue:work", "--once")
        assert "1 job(s) processed" in run("queue:work", "--once")
        assert "0 job(s) processed" in run("queue:work", "--once")
        assert len(smtp.messages) == 3
        messages = {str(email.message_from_bytes(raw, policy=policy.default)["Subject"]): (recipients, email.message_from_bytes(raw, policy=policy.default)) for recipients, raw in smtp.messages}
        recipients, queued = messages["Welcome Queued"]
        assert len(recipients) == 3 and any("private@example.test" in value for value in recipients)
        assert queued["Bcc"] is None and "reply@example.test" in str(queued["Reply-To"])
        attachment = list(queued.iter_attachments())[0]
        assert attachment.get_filename() == "bytes.bin" and attachment.get_payload(decode=True) == b"a\0b"
        with sqlite3.connect(db) as connection:
            rows = connection.execute("SELECT type,recipient,data FROM notifications").fetchall()
            assert len(rows) == 1 and rows[0][1] == "42" and json.loads(rows[0][2]) == {"name": "Ada"}
        assert (project / "audit.txt").read_text() == '42 {"name":"Ada"}\n'
        process = start(log)
        try:
            with smtp.lock:
                smtp.reject = True
            job = request("/retry-mail")
            assert "1 job(s) processed" in run("queue:work", "--once")
            assert json.loads(request("/failures")) == []
            assert "1 job(s) processed" in run("queue:work", "--once")
            failed = json.loads(request("/failures"))
            assert len(failed) == 1 and failed[0]["id"] == job and failed[0]["attempts"] == 2
            assert "payload" not in failed[0]
            with smtp.lock:
                smtp.reject = False
            assert json.loads(request("/retry/" + job)) is True
            assert "1 job(s) processed" in run("queue:work", "--once")
            assert json.loads(request("/failures")) == [] and len(smtp.messages) == 4
        finally:
            stop(process)
    smtp.shutdown()
    thread.join(timeout=10)
print("Generated Redis delivery survived producer exit, SMTP retries and worker restarts; SQLite/custom channels passed")
