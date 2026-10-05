"""Installed .gnr app: views, policies, scoped DI and persistent authentication."""
from concurrent.futures import ThreadPoolExecutor
import hashlib
import http.client
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
import urllib.parse

build, cli_name = Path(sys.argv[1]).resolve(), Path(sys.argv[2]).name
env = os.environ.copy()
env["CMAKE_BUILD_PARALLEL_LEVEL"] = "2"
with tempfile.TemporaryDirectory(prefix="gungnir-application-redis-") as temporary:
    root = Path(temporary)
    stage, project = root / "sdk", root / "project"
    subprocess.run(["cmake", "--install", str(build), "--prefix", str(stage)], check=True, stdout=subprocess.DEVNULL)
    cli = stage / "bin" / cli_name
    env["GUNGNIR_CMAKE_PREFIX"] = str(stage)
    subprocess.run([str(cli), "new", "ApplicationRedis", str(project)], env=env, check=True, stdout=subprocess.DEVNULL)

    def source(path, text):
        target = project / path
        target.parent.mkdir(parents=True, exist_ok=True)
        target.write_text(text)

    def run(*arguments):
        result = subprocess.run([str(cli), *arguments], cwd=project, env=env, text=True, capture_output=True, timeout=240)
        assert result.returncode == 0, result.stdout + result.stderr
        return result.stdout

    source("app/models/User.gnr", """model User {
        table = 'application_users'; timestamps = false;
        fillable = ['name', 'email', 'password']; hidden = ['password'];
        string name; string email; string password;
    }""")
    source("app/models/Project.gnr", """model Project {
        table = 'application_projects'; timestamps = false;
        fillable = ['owner_id', 'title']; int owner_id; string title;
    }""")
    source("app/policies/ProjectPolicy.gnr", """import app.models.User; import app.models.Project;
    policy ProjectPolicy {
        view(User actor, Project project) { return actor.id == project.owner_id ? allow() : deny('Project access denied'); }
    }""")
    source("database/migrations/CreateApplication.gnr", """migration CreateApplication {
        up() {
            Table::create('application_users', (table) => {table.id(); table.string('name'); table.string('email').unique(); table.string('password');});
            Table::create('application_projects', (table) => {table.id(); table.integer('owner_id'); table.string('title');});
        }
        down() {Table::dropIfExists('application_projects'); Table::dropIfExists('application_users');}
    }""")
    source("app/controllers/Portal.gnr", """import app.models.User; import app.models.Project;
    controller Portal {
        inject Config config; inject Logger logger; inject Telemetry telemetry;
        index() {return text('ready');}
        seed() {
            const first = User::create({'name':'<Freya>', 'email':'freya@example.test', 'password':Password::hash('correct-password')});
            User::create({'name':'Odin', 'email':'odin@example.test', 'password':Password::hash('other-password')});
            return json(Project::create({'owner_id':first.id, 'title':'<Project>'}));
        }
        login(Request request) {
            const credentials = request.validate({'email':'required|email', 'password':'required|string'});
            return json(auth.attempt(request, credentials, request.input('remember') == '1'));
        }
        logout(Request request) {auth.logout(request); return json(request.authenticated());}
        status(Request request) {return json({'authenticated':request.authenticated(), 'user':request.user()});}
        show(Request request, Project project) {
            authorize(request, 'view', project);
            const span = telemetry.span('project.view', {'project_id':project.id});
            span.attribute('password', 'must-be-redacted');
            logger.info('Project viewed', {'authorization':request.header('Authorization')});
            telemetry.counter('project.views', 1, {'result':'allowed'});
            span.end();
            return view('projects/show', {'project':project, 'users':User::all(), 'app':config.string('app.name'), 'scope':config.integer('scope.id')});
        }
        scope() {return text(json(config.integer('scope.id')).body());}
    }""")
    source("app/middleware/ScopeTrace.gnr", """middleware ScopeTrace {
        inject Config config;
        async handle(Request request, Next next) {
            const id = config.integer('scope.id');
            let response = await next(request);
            response.header('X-Scope', json(id).body());
            return response;
        }
    }""")
    source("routes/web.gnr", """Route::get('/', Portal::index);
    Route::post('/seed', Portal::seed);
    Route::post('/login', Portal::login);
    Route::post('/logout', Portal::logout);
    Route::get('/status', Portal::status);
    Route::get('/scope', Portal::scope).middleware('ScopeTrace');
    Route::get('/projects/{project}', Portal::show).middleware('ScopeTrace');""")
    source("views/layouts/app.html", "<html><title>{{#yield 'title'}}Application{{/yield}}</title>{{#yield 'content'}}empty{{/yield}}</html>")
    source("views/components/panel.html", "<aside>{{{slot}}}</aside>")
    source("views/partials/user.html", "<p>{{user.name}}:{{user.password}}</p>")
    source("views/projects/show.html", "{{#layout 'layouts/app'}}{{#section 'title'}}{{app}}{{/section}}{{#section 'content'}}<h1>{{project.title}}</h1>{{#component 'components/panel'}}{{#each users}}{{> 'partials/user' user=this}}{{/each}}{{/component}}<i>{{scope}}</i>{{/section}}{{/layout}}")
    prefix = "gungnir:application:" + str(time.time_ns()) + ":"
    native = r'''#pragma once
#include <gungnir/core/services.hpp>
#include <gungnir/auth/redis_remember_store.hpp>
#include <gungnir/session/redis_store.hpp>
#include <atomic>
#include <cstdlib>
#include <fstream>
#include <mutex>
namespace bootstrap {
struct Audit : gungnir::logging::Sink, gungnir::observability::SpanSink, gungnir::observability::MetricSink {
    std::mutex mutex; std::ofstream output;
    explicit Audit(const std::filesystem::path& path) : output(path, std::ios::app) {}
    void record(gungnir::Json value) { std::lock_guard lock{mutex}; output << value.dump() << '\n'; output.flush(); }
    void write(const gungnir::logging::Record& value) override {
        record(gungnir::Json::object({{"kind","log"},{"name",value.message},{"attributes",gungnir::http::make_json(value.context)}}));
    }
    void export_span(const gungnir::observability::SpanRecord& value) override {
        record(gungnir::Json::object({{"kind","span"},{"name",value.name},{"trace",value.trace_id},{"parent",value.parent_span_id},{"attributes",gungnir::http::make_json(value.attributes)}}));
    }
    void export_metric(const gungnir::observability::MetricPoint& value) override {
        record(gungnir::Json::object({{"kind","metric"},{"name",value.name},{"trace",value.trace_id}}));
    }
    void flush() override { record(gungnir::Json::object({{"kind","cleanup"},{"name","flush"}})); }
    void shutdown() override { record(gungnir::Json::object({{"kind","cleanup"},{"name","shutdown"}})); }
};
inline void configure(gungnir::Application& app) {
    using namespace gungnir;
    const String host = std::getenv("GUNGNIR_REDIS_HOST") ? std::getenv("GUNGNIR_REDIS_HOST") : "127.0.0.1";
    const auto port = static_cast<std::uint16_t>(std::getenv("GUNGNIR_REDIS_PORT") ? std::stoi(std::getenv("GUNGNIR_REDIS_PORT")) : 6379);
    session::RedisSessionSettings sessions; sessions.redis.host=host; sessions.redis.port=port; sessions.redis.database=15;
    sessions.redis.prefix=PREFIX_SESSION;
    auto session_store=std::make_shared<session::RedisStore>(sessions);
    auth::RedisRememberSettings tokens; tokens.client.nodes={{host,port}}; tokens.client.database=15; tokens.prefix=PREFIX_REMEMBER;
    auto remember=std::make_shared<auth::RedisRememberStore>(tokens);
    const auto identities=[](std::string_view id)->std::optional<auth::Identity> {
        auto user=::User::find(model::AttributeValue{String{id}});
        if (!user) return {};
        return auth::Identity{.id=std::to_string(user->id.get()), .attributes={{"name",user->name.get()}}};
    };
    const auto credentials=[identities](std::string_view email)->std::optional<auth::PasswordIdentity> {
        auto user=::User::query().where("email",String{email}).first();
        if (!user) return {};
        return auth::PasswordIdentity{*identities(std::to_string(user->id.get())),user->password.get()};
    };
    auto guard=std::make_shared<auth::SessionGuard>(credentials,identities,remember);
    auto audit=std::make_shared<Audit>(app.base_path()/"audit.jsonl");
    ServiceOptions options; options.authentication=guard; options.logger=std::make_shared<logging::Logger>(); options.logger->sink(audit);
    options.tracer=std::make_shared<observability::Tracer>(audit); options.meter=std::make_shared<observability::Meter>(audit);
    app.provider<ServicesProvider>(options);
    auto ids=std::make_shared<std::atomic<Int64>>(0);
    app.scoped<config::Service>([ids](ServiceScope&) {
        auto values=std::make_shared<config::Repository>(); values->set("scope.id",++*ids).set("app.name","Release application");
        return config::Service{values};
    });
    app.router().use(session::middleware(session_store)); app.router().use(auth::session(identities)); app.router().use(auth::guard(guard));
}
inline void boot(gungnir::Application&) {}
}
'''.replace("PREFIX_SESSION", json.dumps(prefix + "session:")).replace("PREFIX_REMEMBER", json.dumps(prefix + "remember:"))
    source("bootstrap/app.hpp", native)
    source(".env", f"APP_HOST=127.0.0.1\nAPP_PORT=8000\nDB_CONNECTION=sqlite\nDB_DATABASE={project / 'application.sqlite'}\n")
    run("build")
    run("migrate")
    binary = project / ".gungnir/build/app"

    def request(port, path, data=None, cookies=""):
        connection = http.client.HTTPConnection("127.0.0.1", port, timeout=10)
        headers = {"Accept":"application/json", "Authorization":"private-bearer", "Content-Type":"application/x-www-form-urlencoded"}
        if cookies:
            headers["Cookie"] = cookies
        connection.request("POST" if data is not None else "GET", path, urllib.parse.urlencode(data) if data is not None else None, headers)
        response = connection.getresponse()
        body = response.read().decode()
        result = (response.status, body, response.getheaders())
        connection.close()
        return result

    def cookies(response):
        jar = SimpleCookie()
        for name, value in response[2]:
            if name.lower() == "set-cookie":
                jar.load(value)
        return jar

    def value(response, name):
        return cookies(response)[name].value

    def signed(response):
        assert response[0] == 200, response
        return json.loads(response[1])["authenticated"]

    def redis_command(*arguments):
        # Inspect only this fixture's namespace without a redis-py/CLI dependency.
        with socket.create_connection((env.get("GUNGNIR_REDIS_HOST","127.0.0.1"), int(env.get("GUNGNIR_REDIS_PORT","6379"))), timeout=5) as connection:
            with connection.makefile("rb") as stream:
                def read():
                    kind, payload = stream.read(1), stream.readline().removesuffix(b"\r\n")
                    if kind in (b"+", b":"):
                        return payload
                    assert kind == b"$", (kind,payload)
                    size = int(payload)
                    if size == -1:
                        return None
                    result = stream.read(size)
                    assert stream.read(2) == b"\r\n"
                    return result
                for command in (("SELECT","15"), arguments):
                    encoded = [str(argument).encode() for argument in command]
                    connection.sendall(b"*"+str(len(encoded)).encode()+b"\r\n"+b"".join(b"$"+str(len(item)).encode()+b"\r\n"+item+b"\r\n" for item in encoded))
                    result = read()
                return result

    log = (project / "server.log").open("w")
    processes = []

    def start():
        with socket.socket() as reservation:
            reservation.bind(("127.0.0.1", 0))
            port = reservation.getsockname()[1]
        child_env = dict(env, APP_PORT=str(port))
        process = subprocess.Popen([str(binary)], cwd=project, env=child_env, stdout=log, stderr=log)
        processes.append(process)
        deadline = time.monotonic() + 20
        while time.monotonic() < deadline:
            assert process.poll() is None, (project / "server.log").read_text()
            try:
                if request(port, "/")[1] == "ready":
                    return process, port
            except OSError:
                pass
            time.sleep(0.05)
        raise AssertionError((project / "server.log").read_text())

    def stop(process):
        process.send_signal(signal.SIGTERM)
        assert process.wait(timeout=10) == 0, (project / "server.log").read_text()

    try:
        first, port = start()
        assert request(port,"/seed",{})[0] == 200
        guest = request(port,"/status")
        guest_session = value(guest,"gungnir_session")
        assert not signed(guest)
        assert request(port,"/projects/1")[0] == 401
        failed = request(port,"/login",{"email":"freya@example.test", "password":"wrong", "remember":"1"},"gungnir_session="+guest_session)
        assert failed[1] == "false" and not signed(request(port,"/status",cookies="gungnir_session="+guest_session))
        login = request(port,"/login",{"email":"freya@example.test", "password":"correct-password", "remember":"1"},"gungnir_session="+guest_session)
        assert login[1] == "true", login
        session_id, token = value(login,"gungnir_session"), value(login,"gungnir_remember")
        assert session_id != guest_session and len(token) == 64
        digest = hashlib.sha256(token.encode()).hexdigest()
        assert redis_command("GET",prefix+"remember:"+digest) == b"1"
        assert redis_command("GET",prefix+"remember:"+token) is None
        assert int(redis_command("PTTL",prefix+"remember:"+digest)) > 0
        persisted = redis_command("GET",prefix+"session:"+session_id)
        assert persisted and token.encode() not in persisted and b"correct-password" not in persisted
        for name in ("gungnir_session", "gungnir_remember"):
            assert cookies(login)[name]["secure"] and cookies(login)[name]["httponly"]
        assert not signed(request(port,"/status",cookies="gungnir_session="+guest_session))
        stop(first)
        second, port = start()
        third, other_port = start()
        session_cookie = "gungnir_session=" + session_id
        assert signed(request(port,"/status",cookies=session_cookie))
        assert signed(request(other_port,"/status",cookies=session_cookie))
        rendered = request(port,"/projects/1",cookies=session_cookie)
        assert rendered[0] == 200 and "&lt;Project&gt;" in rendered[1] and "&lt;Freya&gt;" in rendered[1] and "Release application" in rendered[1], rendered
        assert "password" not in rendered[1] and "correct-password" not in rendered[1]
        assert request(port,"/projects/999",cookies=session_cookie)[0] == 404
        a, b = request(port,"/scope"), request(port,"/scope")
        assert a[0] == b[0] == 200 and a[1] == dict((name.lower(), value) for name, value in a[2])["x-scope"] and b[1] == dict((name.lower(), value) for name, value in b[2])["x-scope"] and a[1] != b[1], (a,b)
        denied = request(port,"/login",{"email":"odin@example.test", "password":"other-password"})
        assert request(port,"/projects/1",cookies="gungnir_session="+value(denied,"gungnir_session"))[0] == 403
        # Independent server processes race the same token; exactly one may recall it.
        with ThreadPoolExecutor(max_workers=2) as pool:
            recalled = list(pool.map(lambda current:request(current,"/status",cookies="gungnir_remember="+token), (port,other_port)))
        assert sum(signed(response) for response in recalled) == 1, recalled
        winner = next(response for response in recalled if signed(response))
        replacement = value(winner,"gungnir_remember")
        assert replacement != token
        assert not signed(request(port,"/status",cookies="gungnir_remember="+token))
        logout = request(other_port,"/logout",{},"gungnir_session="+value(winner,"gungnir_session")+"; gungnir_remember="+replacement)
        assert logout[1] == "false" and cookies(logout)["gungnir_remember"]["max-age"] == "0"
        assert not signed(request(port,"/status",cookies="gungnir_session="+value(winner,"gungnir_session")))
        assert not signed(request(port,"/status",cookies="gungnir_remember="+replacement))
        stop(second)
        stop(third)
        records = [json.loads(line) for line in (project / "audit.jsonl").read_text().splitlines()]
        spans = [record for record in records if record["kind"] == "span"]
        application = next(record for record in spans if record["name"] == "project.view")
        assert application["attributes"]["password"] == "[redacted]" and application["parent"]
        assert any(record["name"].startswith("http.") and record["trace"] == application["trace"] for record in spans)
        assert any(record["name"] == "database.query" for record in spans)
        logged = next(record for record in records if record["kind"] == "log" and record["name"] == "Project viewed")
        assert logged["attributes"]["authorization"] == "[redacted]" and logged["attributes"]["trace_id"] == application["trace"]
        assert any(record["kind"] == "metric" and record["name"] == "project.views" and record["trace"] == application["trace"] for record in records)
        assert sum(record["kind"] == "cleanup" and record["name"] == "shutdown" for record in records) >= 6
        text = (project / "audit.jsonl").read_text()
        assert "private-bearer" not in text and "must-be-redacted" not in text and "correct-password" not in text
    finally:
        for process in processes:
            if process.poll() is None:
                process.terminate()
                process.wait(timeout=10)
        log.close()
print("Installed views, model policies, scoped DI, telemetry, persistent sessions and atomic cross-process remember recall passed")
