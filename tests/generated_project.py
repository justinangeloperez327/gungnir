"""Installed CLI: generators -> validation -> native build -> live HTTP + dev restart."""
import os
import json
import pathlib
import signal
import socket
import sqlite3
import subprocess
import sys
import time
import urllib.error
import urllib.request

cli, project_arg, sqlite = sys.argv[1:]
project = pathlib.Path(project_arg)
env = os.environ.copy()
env.pop("GUNGNIR_CMAKE_PREFIX", None)
env["CMAKE_BUILD_PARALLEL_LEVEL"] = "2"


def run(*args, expect=0):
    value = subprocess.run([cli, *args], cwd=project, env=env, text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, timeout=240)
    if (expect is None and value.returncode == 0) or (expect is not None and value.returncode != expect):
        raise AssertionError(f"{args}: exit {value.returncode}\n{value.stdout}")
    return value.stdout


for kind, name in (("model", "User"), ("event", "Created"), ("job", "Ping"), ("mail", "Welcome"), ("request", "StoreUser"), ("middleware", "Pass"), ("controller", "Extra"), ("migration", "create_users_table")):
    run("make:" + kind, name)
for kind, name, dependency in (("listener", "Record", "app.events.Created::Created"), ("policy", "User", "app.models.User::User"), ("notification", "Greeting", "app.models.User::User")):
    run("make:" + kind, name, dependency)
assert (project / "app/models/User.gnr").is_file()
assert (project / "app/controllers/ExtraController.gnr").is_file()
assert (project / "app/middleware/PassMiddleware.gnr").is_file()
assert (project / "database/migrations/create_users_table.gnr").is_file()
(project / "app/models/User.gnr").write_text('''import app.models.Post;
model User {
    table = "users";
    string name;
    posts() { return hasMany<Post>("user_id"); }
}
''')
(project / "app/models/Post.gnr").write_text('model Post { int user_id; string title; }\n')
# Exercise import ordering and injection in a real application.
home = project / "app/controllers/HomeController.gnr"
home.write_text("import app.controllers.ExtraController;\ncontroller HomeController { inject ExtraController extra; index() { return extra.index(); } }\n")
extra = project / "app/controllers/ExtraController.gnr"
extra.write_text("controller ExtraController { index() { return text('first'); } }\n")
routes = project / "routes/web.gnr"
routes.write_text('''Route::get("/", HomeController::index).name("home");
Route::prefix("/api").name("api.").middleware("PassMiddleware").group(() => {
    Route::prefix("/v1").name("v1.").middleware(RouteMiddleware).group(() => {
        Route::get("/users/{user}", RouteController::show).name("users.show").whereNumber("user");
        Route::get("/url/{id}", RouteController::url);
        Route::get("/scalar/{id}/{enabled}", RouteController::scalar);
        Route::get("/echo/{value}", RouteController::echo).name("echo");
    });
});
Route::fallback(RouteController::missing);
''')
(project / "app/controllers/RouteController.gnr").write_text('''import app.models.User;
controller RouteController {
    async show(User user) { return json(user); }
    url(int id) { return text(Route::url("api.v1.users.show", { user: id })); }
    scalar(bool enabled, Request request, int id) { return json({ id: id, enabled: enabled }); }
    echo(string value) { return text(value); }
    missing() { return text("fallback", 404); }
}
''')
(project / "app/middleware/PassMiddleware.gnr").write_text('''middleware PassMiddleware {
    async Response handle(Request request, Next next) {
        let response = await next(request);
        response.header("X-Global", "yes");
        return response;
    }
}
''')
(project / "app/middleware/RouteMiddleware.gnr").write_text('''middleware RouteMiddleware {
    async Response handle(Request request, Next next) {
        let response = await next(request);
        response.header("X-Route", "yes");
        return response;
    }
}
''')
migration = project / "database/migrations/create_users_table.gnr"
migration.write_text(migration.read_text().replace("table.id();", "table.id();\n            table.string('name');"))
bootstrap = project / "bootstrap/app.hpp"
bootstrap.write_text(bootstrap.read_text().replace("// Add service bindings, middleware, and providers here.", "app.middleware<PassMiddleware>();").replace("inline void boot(gungnir::Application&)", "inline void boot(gungnir::Application& app)").replace("// Services and generated listeners, policies, and jobs are registered.", """
    auto events = app.container().resolve<gungnir::events::Dispatcher>();
    if (events->listener_count(Created::event_name) != 1) throw std::logic_error("listener not registered");
    events->dispatch(Created{1});
    auto dispatcher = app.container().resolve<gungnir::queue::Dispatcher>();
    dispatcher->dispatch(PingJob{});
    if (!app.container().resolve<gungnir::queue::Worker>()->run_one()) throw std::logic_error("job not registered");
    User user;
    user.id = 7;
    if (!app.container().resolve<gungnir::auth::ResourceAuthorization>()->inspect("view", user, user).allowed) throw std::logic_error("policy not registered");
    (void)WelcomeMail{}.message();
    (void)GreetingNotification{}.bind(user);
"""))
# Use a dynamically reserved port and tolerate startup scheduling variability.
with socket.socket() as reservation:
    reservation.bind(("127.0.0.1", 0))
    port = reservation.getsockname()[1]
env["APP_HOST"] = "127.0.0.1"
env["APP_PORT"] = str(port)
(project / ".env").write_text(f"APP_HOST=127.0.0.1\nAPP_PORT={port}\nAPP_ENV=testing\nAPP_DEBUG=true\nVIEW_PATH=views\nDB_CONNECTION=\n")
run("build")
generated = project / ".gungnir/generated"
header = generated / "program.hpp"
unit = generated / "app.controllers.ExtraController.cpp"
header_time, unit_time = header.stat().st_mtime_ns, unit.stat().st_mtime_ns
build = project / ".gungnir/build"
objects = {p: p.stat().st_mtime_ns for p in build.rglob("*") if p.suffix in (".o", ".obj")}
assert objects, "No native object files"
run("build")
assert header.stat().st_mtime_ns == header_time and unit.stat().st_mtime_ns == unit_time
assert all(p.stat().st_mtime_ns == stamp for p, stamp in objects.items()), "No-op build recompiled native objects"
# Invalid routes fail before changing any generated output.
route_source = routes.read_text()
snapshot = {p: (p.read_bytes(), p.stat().st_mtime_ns) for p in generated.rglob("*") if p.is_file()}
routes.write_text(route_source.replace('whereNumber("user")', 'whereNumber("missing")'))
diagnostic = run("build", expect=1)
assert "GNR2320" in diagnostic and "routes" in diagnostic
assert snapshot == {p: (p.read_bytes(), p.stat().st_mtime_ns) for p in generated.rglob("*") if p.is_file()}, "Invalid route changed generated output"
routes.write_text(route_source)
# Migration planning is independent of a live database adapter.
with (project / ".env").open("a") as output:
    output.write("DB_CONNECTION=postgresql\n")
assert 'CREATE TABLE' in run("migrate:plan")
if sqlite == "ON":
    with (project / ".env").open("a") as output:
        output.write("DB_CONNECTION=sqlite\nDB_DATABASE=" + str(project / "test.sqlite") + "\n")
    assert "1 migration(s) applied" in run("migrate")
    assert "0 migration(s) applied" in run("migrate")
    assert "[x] create_users_table" in run("migrate:status")
    assert "1 migration(s) rolled back" in run("migrate:rollback")
    assert "1 migration(s) applied" in run("migrate")
    with sqlite3.connect(project / "test.sqlite") as connection:
        connection.execute("INSERT INTO users (id, name) VALUES (?, ?)", (42, "routed user"))
database_env = "DB_CONNECTION=sqlite\nDB_DATABASE=" + str(project / "test.sqlite") + "\n" if sqlite == "ON" else "DB_CONNECTION=\n"
application_env = f"APP_HOST=127.0.0.1\nAPP_PORT={port}\nAPP_ENV=testing\nAPP_DEBUG=true\nVIEW_PATH=views\n" + database_env
(project / ".env").write_text(application_env)


def wait_response(expected, process, timeout=180):
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        if process.poll() is not None:
            raise AssertionError(f"App exited {process.returncode}; see {project / 'dev.log'}")
        try:
            with urllib.request.urlopen(f"http://127.0.0.1:{port}/", timeout=1) as response:
                if response.status == 200 and response.read().decode() == expected:
                    return
        except (OSError, urllib.error.URLError):
            pass
        time.sleep(0.1)
    raise AssertionError(f"Timed out waiting for {expected}; see {project / 'dev.log'}")


def stop(process):
    process.terminate()
    try:
        process.wait(timeout=10)
    except subprocess.TimeoutExpired:
        process.kill()
        process.wait(timeout=5)
        raise AssertionError("Process failed to stop")


def response(path):
    try:
        result = urllib.request.urlopen(f"http://127.0.0.1:{port}{path}", timeout=5)
    except urllib.error.HTTPError as error:
        result = error
    with result:
        return result.status, result.read().decode(), result.headers


# `run` propagates child failure status, including bootstrap failures.
with (project / ".env").open("a") as output:
    output.write("DB_CONNECTION=does-not-exist\n")
assert run("run", expect=1)
(project / ".env").write_text(application_env)
with (project / "dev.log").open("w") as log:
    if os.name == "nt":
        # Windows verifies native HTTP startup; signal/watch lifecycle runs on
        # POSIX where Python can deliver the same console signals as a terminal.
        binary = build / "Debug/app.exe"
        if not binary.exists():
            binary = build / "app.exe"
        process = subprocess.Popen([str(binary)], cwd=project, env=env, stdout=log, stderr=log)
    else:
        process = subprocess.Popen([cli, "dev"], cwd=project, env=env, stdout=log, stderr=log)
    try:
        wait_response("first", process)
        status, body, headers = response("/api/v1/scalar/7/true")
        assert status == 200 and json.loads(body) == {"id": 7, "enabled": True}
        assert headers["X-Global"] == "yes" and headers["X-Route"] == "yes"
        assert response("/api/v1/url/42")[:2] == (200, "/api/v1/users/42")
        assert response("/api/v1/echo/a%2Fb%20%7Bvalue%7D")[:2] == (200, "a/b {value}")
        assert response("/api/v1/scalar/7/invalid")[0] == 404
        status, body, headers = response("/missing")
        assert (status, body) == (404, "fallback") and headers["X-Global"] == "yes" and headers.get("X-Route") is None
        if sqlite == "ON":
            status, body, headers = response("/api/v1/users/42")
            assert status == 200 and json.loads(body)["name"] == "routed user"
            assert headers["X-Route"] == "yes"
            assert response("/api/v1/users/999999")[0] == 404
            assert response("/api/v1/users/invalid")[:2] == (404, "fallback")
        if os.name != "nt":
            time.sleep(0.4)
            extra.write_text("controller ExtraController { index() { return text('second'); } }\n")
            wait_response("second", process)
            assert header.stat().st_mtime_ns == header_time, "Body edit changed the shared interface"
            # Break an imported module: diagnostics appear and the healthy child stays up.
            extra.write_text("controller ExtraController { index() { return absent; } }\n")
            deadline = time.monotonic() + 15
            while "Unknown" not in (project / "dev.log").read_text() and "Unresolved" not in (project / "dev.log").read_text():
                if time.monotonic() > deadline:
                    raise AssertionError("Missing rebuild diagnostic")
                time.sleep(0.1)
            wait_response("second", process, 5)
            extra.write_text("controller ExtraController { index() { return text('third'); } }\n")
            wait_response("third", process)
            process.send_signal(signal.SIGINT)
            assert process.wait(timeout=10) == 130
            with socket.socket() as probe:
                probe.settimeout(1)
                assert probe.connect_ex(("127.0.0.1", port)) != 0, "Watcher left the child serving after exit"
    finally:
        if process.poll() is None:
            stop(process)
# Failed compilation and bad flags must be observable by CI callers.
extra.write_text("controller ExtraController { index() { return missing; } }\n")
run("build", expect=1)
run("build", "--typo", expect=1)
extra.write_text("controller ExtraController { index() { return text('valid'); } }\n")
with bootstrap.open("a") as output:
    output.write("\n#error intentional_native_failure\n")
assert "intentional_native_failure" in run("build", expect=None)
print("Generated application, routing/model binding, incremental build, bootstrap, migrations, and dev checks passed")
