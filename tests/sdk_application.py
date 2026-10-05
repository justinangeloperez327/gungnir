"""A relocated core/application SDK: .gnr migrations, stored hashes and login.

Uses the installed CLI and ordinary bootstrap configuration. No generated C++
is patched, and bundled dependencies are tested with package discovery disabled.
"""
from contextlib import closing
from http.cookies import SimpleCookie
import http.client
import json
import os
from pathlib import Path
import shutil
import socket
import sqlite3
import subprocess
import sys
import tempfile
import time
import urllib.parse

prefix = Path(sys.argv[1]).resolve()
settings = dict(line.split("=", 1) for line in (prefix / "share/gungnir/sdk.txt").read_text().splitlines())
application = settings["profile"] == "application"
assert settings["profile"] in ("core", "application"), settings
assert settings["sqlite"] == settings["password"] == ("ON" if application else "OFF"), settings
assert settings["bundled_application_dependencies"] == ("ON" if application else "OFF"), settings
env = os.environ.copy()
for key in ("GUNGNIR_CMAKE_PREFIX", "CMAKE_PREFIX_PATH", "OPENSSL_ROOT_DIR", "SQLite3_ROOT", "VCPKG_ROOT", "VCPKG_INSTALLATION_ROOT"):
    env.pop(key, None)
env["PATH"] = os.pathsep.join(part for part in env.get("PATH", "").split(os.pathsep)
    if "vcpkg" not in part.lower() and "openssl" not in part.lower())
env["CMAKE_BUILD_PARALLEL_LEVEL"] = "2"

with tempfile.TemporaryDirectory(prefix="gungnir-sdk-application-") as temporary:
    root = Path(temporary)
    sdk = root / "relocated sdk"
    shutil.copytree(prefix, sdk)
    cli = sdk / "bin" / ("gungnir.exe" if os.name == "nt" else "gungnir")
    project = root / "application"
    subprocess.run([str(cli), "new", "SDKApplication", str(project)], env=env, check=True, timeout=30)

    def source(path, text):
        target = project / path
        target.parent.mkdir(parents=True, exist_ok=True)
        target.write_text(text)

    def run(*arguments, success=True):
        result = subprocess.run([str(cli), *arguments], cwd=project, env=env, text=True,
            stdout=subprocess.PIPE, stderr=subprocess.STDOUT, timeout=300)
        assert (result.returncode == 0) == success, (arguments, result.returncode, result.stdout)
        return result.stdout

    source("app/controllers/SDKController.gnr", "controller SDKController {index(){return text(Password::hash('test-password'));}}")
    source("routes/web.gnr", "Route::get('/', SDKController::index);")
    if not application:
        output = run("build", "--release", success=False)
        assert "selected Gungnir SDK has no password backend" in " ".join(output.split()), output
        assert "GUNGNIR_WITH_PASSWORD=ON" in output and "GUNGNIR_CMAKE_PREFIX" in output, output
        assert "undefined reference" not in output and "LNK2019" not in output, output
        source("app/controllers/SDKController.gnr", """// Password::hash('comment'); auth.attempt(request, data);
        function string passwordHash() { return "Password::hash('literal')"; }
        controller SDKController {index(){return text(passwordHash());}}""")
        source(".env", "APP_HOST=127.0.0.1\nAPP_PORT=8000\nDB_CONNECTION=sqlite\nDB_DATABASE=app.sqlite\n")
        # The core SDK remains usable for HTTP; database startup fails clearly.
        output = run("run", "--release", success=False)
        assert "backend 'sqlite'" in output and "application SDK" in output, output
        assert "GUNGNIR_WITH_SQLITE=ON" in output and "GUNGNIR_CMAKE_PREFIX" in output, output
    else:
        for notice in ("SQLITE", "OPENSSL"):
            assert (sdk / f"share/gungnir/licenses/{notice}.txt").stat().st_size > 0
        for header in ("opensslconf.h", "configuration.h"):
            assert (sdk / f"include/gungnir/vendor/openssl/{header}").is_file()
        sqlite_header = sdk / "include/gungnir/vendor/sqlite3.h"
        if "sqlite3-vcpkg-config.h" in sqlite_header.read_text():
            assert (sqlite_header.parent / "sqlite3-vcpkg-config.h").is_file(), "SDK is missing SQLite's vcpkg configuration header"
        probe = root / "probe"
        probe.mkdir()
        (probe / "CMakeLists.txt").write_text('''cmake_minimum_required(VERSION 3.25)
project(application_sdk_probe LANGUAGES CXX)
set(CMAKE_DISABLE_FIND_PACKAGE_OpenSSL TRUE)
set(CMAKE_DISABLE_FIND_PACKAGE_SQLite3 TRUE)
find_package(Gungnir CONFIG REQUIRED)
if(NOT Gungnir_WITH_SQLITE OR NOT Gungnir_WITH_PASSWORD OR NOT Gungnir_BUNDLED_APPLICATION_DEPENDENCIES)
    message(FATAL_ERROR "Application SDK feature metadata is incomplete")
endif()
foreach(target gungnir::sdk_sqlite gungnir::sdk_crypto)
    get_target_property(location ${target} IMPORTED_LOCATION)
    if(NOT location MATCHES "relocated sdk/" OR NOT EXISTS "${location}")
        message(FATAL_ERROR "Bundled library did not relocate: ${location}")
    endif()
endforeach()
add_executable(sdk_headers sdk_headers.cpp)
target_compile_features(sdk_headers PRIVATE cxx_std_23)
target_link_libraries(sdk_headers PRIVATE gungnir::gungnir)
''')
        (probe / "sdk_headers.cpp").write_text('''#include <openssl/evp.h>
#include <sqlite3.h>
#include <gungnir/auth/password.hpp>
#ifndef GUNGNIR_WITH_PASSWORD
#error Missing password capability
#endif
int main() { return EVP_MD_get_size(EVP_sha256()) == 32 && sqlite3_libversion_number() >= 3000000 ? 0 : 1; }
''')
        subprocess.run(["cmake", "-S", str(probe), "-B", str(probe / "build"), f"-DCMAKE_PREFIX_PATH={sdk}"],
            env=env, check=True, timeout=60, stdout=subprocess.DEVNULL)
        probe_build = subprocess.run(["cmake", "--build", str(probe / "build"), "--config", "Release", "--parallel", "2"],
            env=env, timeout=120, text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
        assert probe_build.returncode == 0, probe_build.stdout
        candidates = [probe / "build/sdk_headers", probe / "build/sdk_headers.exe", probe / "build/Release/sdk_headers.exe"]
        subprocess.run([str(next(path for path in candidates if path.is_file()))], env=env, check=True, timeout=15)
        # Only the built-in OpenSSL default provider is needed for password APIs.
        (root / "openssl.cnf").write_text("")
        (root / "empty modules").mkdir()
        env["OPENSSL_CONF"] = str(root / "openssl.cnf")
        env["OPENSSL_MODULES"] = str(root / "empty modules")
        source("app/models/User.gnr", '''model User {
            table = 'sdk_users'; timestamps = false;
            fillable = ['name', 'email', 'password']; hidden = ['password'];
            string name; string email; string password;
        }''')
        source("database/migrations/CreateSDKUsers.gnr", '''migration CreateSDKUsers {
            up() { Table::create('sdk_users', (table) => {
                table.id(); table.string('name'); table.string('email').unique(); table.string('password');
            }); }
            down() { Table::dropIfExists('sdk_users'); }
        }''')
        source("app/controllers/SDKController.gnr", '''import app.models.User;
        controller SDKController {
            index() { return text('application-ready'); }
            store(Request request) {
                request.validate({'name':'required|string', 'email':'required|email', 'password':'required|string'});
                return json(User::create({'name':request.input('name'), 'email':request.input('email'), 'password':Password::hash(request.input('password'))}));
            }
            login(Request request) {
                const input = request.validate({'email':'required|email', 'password':'required|string'});
                return json(auth.attempt(request, input));
            }
            logout(Request request) { auth.logout(request); return json(request.authenticated()); }
            status(Request request) { return json({'authenticated':request.authenticated(), 'user':request.user()}); }
            users() { return json(User::all()); }
        }''')
        source("routes/web.gnr", '''Route::get('/', SDKController::index);
        Route::post('/register', SDKController::store);
        Route::post('/login', SDKController::login);
        Route::post('/logout', SDKController::logout);
        Route::get('/status', SDKController::status);
        Route::get('/users', SDKController::users);''')
        source("bootstrap/app.hpp", r'''#pragma once
#include <gungnir/core/services.hpp>
#include <gungnir/session/memory_store.hpp>
namespace bootstrap {
inline void configure(gungnir::Application& app) {
    using namespace gungnir;
    const auto identity = [](std::string_view id)->std::optional<auth::Identity> {
        auto user = ::User::find(model::AttributeValue{String{id}});
        if (!user) return {};
        return auth::Identity{.id=std::to_string(user->id.get()), .attributes={{"name",user->name.get()}}};
    };
    const auto credentials = [identity](std::string_view email)->std::optional<auth::PasswordIdentity> {
        auto user = ::User::query().where("email", String{email}).first();
        if (!user) return {};
        return auth::PasswordIdentity{*identity(std::to_string(user->id.get())), user->password.get()};
    };
    auto guard = std::make_shared<auth::SessionGuard>(credentials, identity);
    app.provider<ServicesProvider>(ServiceOptions{.authentication=guard});
    app.router().use(session::middleware(std::make_shared<session::MemoryStore>()));
    app.router().use(auth::session(identity));
    app.router().use(auth::guard(guard));
}
inline void boot(gungnir::Application&) {}
}
''')
        with socket.socket() as reservation:
            reservation.bind(("127.0.0.1", 0))
            port = reservation.getsockname()[1]
        source(".env", f"APP_HOST=127.0.0.1\nAPP_PORT={port}\nDB_CONNECTION=sqlite\nDB_DATABASE=app.sqlite\n")
        run("build", "--release")
        assert "1 migration(s) applied" in run("migrate", "--release")
        assert "0 migration(s) applied" in run("migrate", "--release")
        assert "[x] CreateSDKUsers" in run("migrate:status", "--release")
        candidates = [project / ".gungnir/build/app", project / ".gungnir/build/app.exe", project / ".gungnir/build/Release/app.exe"]
        binary = next(path for path in candidates if path.is_file())

        def request(path, data=None, cookie=""):
            connection = http.client.HTTPConnection("127.0.0.1", port, timeout=20)
            headers = {"Content-Type":"application/x-www-form-urlencoded", "Accept":"application/json"}
            if cookie:
                headers["Cookie"] = cookie
            connection.request("POST" if data is not None else "GET", path,
                urllib.parse.urlencode(data) if data is not None else None, headers)
            response = connection.getresponse()
            result = (response.status, response.read().decode(), response.getheaders())
            connection.close()
            return result

        def serve(check):
            with (project / "server.log").open("w+") as log:
                process = subprocess.Popen([str(binary)], cwd=project, env=env, stdout=log, stderr=log)
                try:
                    deadline = time.monotonic() + 20
                    while time.monotonic() < deadline:
                        if process.poll() is not None:
                            log.seek(0)
                            raise AssertionError("Application exited: " + log.read())
                        try:
                            response = request("/")
                            assert response[:2] == (200, "application-ready"), response
                            break
                        except OSError:
                            time.sleep(0.05)
                    else:
                        raise AssertionError("Application did not start")
                    check()
                finally:
                    process.terminate()
                    try:
                        process.wait(timeout=10)
                    except subprocess.TimeoutExpired:
                        process.kill()
                        process.wait(timeout=5)

        def login():
            wrong = request("/login", {"email":"freya@example.test", "password":"wrong"})
            assert wrong[:2] == (200, "false"), wrong
            good = request("/login", {"email":"freya@example.test", "password":"correct-password"})
            assert good[:2] == (200, "true"), good
            jar = SimpleCookie()
            for key, value in good[2]:
                if key.lower() == "set-cookie":
                    jar.load(value)
            assert jar, good
            cookie = "; ".join(f"{key}={value.value}" for key, value in jar.items())
            status = request("/status", cookie=cookie)
            assert status[0] == 200 and json.loads(status[1])["authenticated"] is True, status
            assert json.loads(status[1])["user"]["attributes"]["name"] == "Freya", status
            assert request("/logout", {}, cookie)[:2] == (200, "false")
            assert json.loads(request("/status", cookie=cookie)[1])["authenticated"] is False

        def first_start():
            result = request("/register", {"name":"Freya", "email":"freya@example.test", "password":"correct-password"})
            assert result[0] == 200 and json.loads(result[1])["email"] == "freya@example.test", result
            assert "password" not in json.loads(result[1]), result
            assert "password" not in json.loads(request("/users")[1])[0]
            login()

        serve(first_start)
        with closing(sqlite3.connect(project / "app.sqlite")) as database:
            hashed = database.execute("SELECT password FROM sdk_users").fetchone()[0]
            assert hashed.startswith("scrypt$") and "correct-password" not in hashed, hashed
        # Authentication after a new process uses the persisted SQLite hash.
        # Sessions intentionally use the ordinary development memory store.
        serve(login)
        assert "1 migration(s) rolled back" in run("migrate:rollback", "--release")
        with closing(sqlite3.connect(project / "app.sqlite")) as database:
            assert database.execute("SELECT name FROM sqlite_master WHERE name='sdk_users'").fetchone() is None

print("Application SDK: relocated bundled dependencies, migrations, stored passwords, login/logout and restart passed" if application
    else "Core SDK: actionable password build and missing SQLite startup errors passed")
