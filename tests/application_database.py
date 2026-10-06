"""Installed SDK -> ordinary .gnr application -> real database -> HTTP.

The bootstrap only counts queries and pool leases. All models, migrations,
queries, relationships and transaction work come from unchanged .gnr sources.
"""
from contextlib import contextmanager
import http.client
import json
import os
from pathlib import Path
import shutil
import socket
import subprocess
import sys
import tempfile
import time
import urllib.parse


def main():
    build, cli_name, backend = Path(sys.argv[1]).resolve(), Path(sys.argv[2]).name, sys.argv[3]
    assert backend in ('sqlite', 'postgresql', 'mysql', 'sqlserver', 'mongodb'), backend
    env = os.environ.copy()
    env['CMAKE_BUILD_PARALLEL_LEVEL'] = '2'
    # The consumer inherits native dependency search paths, never source-tree
    # framework targets. Each live CI job provisions an isolated test database.
    with tempfile.TemporaryDirectory(prefix=f'gungnir-application-{backend}-') as temporary:
        root = Path(temporary)
        sdk, project = root / 'installed sdk', root / 'application'
        subprocess.run(['cmake', '--install', str(build), '--prefix', str(sdk)],
                       env=env, check=True, stdout=subprocess.DEVNULL, timeout=60)
        cli = sdk / 'bin' / cli_name
        env['GUNGNIR_CMAKE_PREFIX'] = str(sdk)
        subprocess.run([str(cli), 'new', 'DatabaseAcceptance', str(project)],
                       env=env, check=True, stdout=subprocess.DEVNULL, timeout=30)
        shutil.copytree(Path(__file__).parent / 'fixtures/database', project, dirs_exist_ok=True)
        (project / 'bootstrap/app.hpp').write_text(r'''#pragma once
#include <gungnir/core/services.hpp>
#include <gungnir/orm/query_log.hpp>
#include <atomic>
#include <iostream>
namespace bootstrap {
inline void configure(gungnir::Application& app) {
    app.provider<gungnir::ServicesProvider>();
    app.router().use([](gungnir::Request& request, gungnir::Next next) -> gungnir::Task<gungnir::Response> {
        try { co_return co_await next(request); }
        catch (const std::exception& error) {
            std::cerr << "Database acceptance request failed: " << error.what() << '\n';
            throw;
        }
    });
    auto queries = std::make_shared<std::atomic<gungnir::Int64>>(0);
    app.on_boot([queries](gungnir::Application&) {
        gungnir::orm::listen([queries](const auto&) { ++*queries; });
    });
    app.on_shutdown([](gungnir::Application&) { gungnir::orm::stop_listening(); });
    app.router().get("/fixture/stats", [queries, &app] {
        return gungnir::Response::json(gungnir::Json::object({
            {"queries", queries->load()},
            {"leased", static_cast<gungnir::Int64>(app.database().pool_stats().leased)}
        }));
    });
}
inline void boot(gungnir::Application&) {}
}
''')
        defaults = {
            'postgresql': ('5432', 'gungnir', 'postgres', 'postgres'),
            'mysql': ('3306', 'gungnir', 'root', 'mysql'),
            'sqlserver': ('1433', 'master', 'sa', ''),
            'mongodb': ('27017', 'gungnir', '', ''),
        }
        configuration = {
            'APP_HOST': '127.0.0.1', 'APP_PORT': '8000', 'DB_CONNECTION': backend,
            'DB_POOL_SIZE': '1', 'DB_POOL_ACQUIRE_TIMEOUT_MS': '2000',
        }
        if backend == 'sqlite':
            configuration['DB_DATABASE'] = str(project / 'acceptance.sqlite')
        else:
            port, database, username, password = defaults[backend]
            for name, fallback in (
                ('HOST', '127.0.0.1'), ('PORT', port), ('DATABASE', database),
                ('USERNAME', username), ('PASSWORD', password),
                ('OPTIONS', 'TrustServerCertificate=yes' if backend == 'sqlserver' else ''),
            ):
                configuration[f'DB_{name}'] = env.get(f'GUNGNIR_{backend.upper()}_{name}', fallback)
        # DB_* inherited from a caller must not override the fixture's .env.
        for name in list(env):
            if name.startswith('DB_'):
                env.pop(name)
        (project / '.env').write_text(''.join(f'{key}={value}\n' for key, value in configuration.items()))

        def run(*arguments):
            result = subprocess.run([str(cli), *arguments], cwd=project, env=env,
                                    text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, timeout=300)
            assert result.returncode == 0, (arguments, result.returncode, result.stdout)
            return result.stdout

        run('build')
        assert '1 migration(s) applied' in run('migrate')
        assert '0 migration(s) applied' in run('migrate')
        assert '[x] CreateDatabaseAcceptance' in run('migrate:status')
        binary = project / '.gungnir/build' / ('app.exe' if os.name == 'nt' else 'app')

        @contextmanager
        def serve():
            with socket.socket() as available:
                available.bind(('127.0.0.1', 0))
                port = available.getsockname()[1]
            process_env = env | {'APP_HOST': '127.0.0.1', 'APP_PORT': str(port)}
            with (project / 'server.log').open('w+') as log:
                process = subprocess.Popen([str(binary)], cwd=project, env=process_env,
                                           stdout=log, stderr=subprocess.STDOUT)

                def request(path, data=None, method=None, status=200):
                    connection = http.client.HTTPConnection('127.0.0.1', port, timeout=10)
                    try:
                        connection.request(method or ('POST' if data is not None else 'GET'), path,
                                           urllib.parse.urlencode(data) if data is not None else None,
                                           {'Content-Type': 'application/x-www-form-urlencoded'})
                        response = connection.getresponse()
                        body = response.read().decode()
                        assert response.status == status, (backend, path, response.status, body)
                        if status != 200 or path == '/':
                            return body
                        return json.loads(body)
                    finally:
                        connection.close()

                try:
                    deadline = time.monotonic() + 30
                    while time.monotonic() < deadline:
                        if process.poll() is not None:
                            log.seek(0)
                            raise AssertionError('Application exited: ' + log.read())
                        try:
                            assert request('/') == 'database-ready'
                            break
                        except OSError:
                            time.sleep(0.05)
                    else:
                        raise AssertionError('Application did not start')
                    yield request
                except Exception:
                    log.flush()
                    log.seek(0)
                    print(log.read(), file=sys.stderr)
                    raise
                finally:
                    if process.poll() is None:
                        process.terminate()
                        try:
                            process.wait(timeout=10)
                        except subprocess.TimeoutExpired:
                            process.kill()
                            process.wait(timeout=5)

        def find(request, name):
            return request('/search?' + urllib.parse.urlencode({'name': name}))

        with serve() as request:
            names = ["O'Neil – 雪", "x'); DROP TABLE database_acceptance_users; --", 'Empty']
            users = [request('/users', {'name': name}) for name in names]
            assert len({user['id'] for user in users}) == 3
            for user, name in zip(users, names):
                assert user['id'] > 0 and user['name'] == name and user['active'] is True, (backend, user, name)
                assert user['note'] is None and 'password' not in user
                persisted = request(f"/users/{user['id']}")
                assert persisted == user, (backend, persisted, user)
                assert find(request, name) == [user]
            request('/users/999999', status=404)
            assert request('/users') == users
            page = request('/page')
            assert page == {'data': users[2:], 'total': 3, 'page': 2}, page
            request('/users', {'name': names[0]}, status=500)
            request('/restricted', {}, status=500)
            assert len(request('/users')) == 3
            first, second, empty = users
            posts = [request(f"/users/{owner['id']}/posts", {'title': title}) for owner, title in
                     ((first, 'First'), (first, 'Second'), (second, 'Third'))]
            before = request('/fixture/stats')['queries']
            loaded = request('/loaded')
            stats = request('/fixture/stats')
            assert stats['queries'] - before == 2 and stats['leased'] == 0, stats
            assert [row['user'] for row in loaded] == users
            assert [row['posts'] for row in loaded] == [posts[:2], posts[2:], []], loaded
            before = stats['queries']
            inverse = request('/posts')
            stats = request('/fixture/stats')
            assert stats['queries'] - before == 2 and stats['leased'] == 0, stats
            assert [row['post'] for row in inverse] == posts
            assert [row['owner'] for row in inverse] == [first, first, second]
            updated = request(f"/users/{empty['id']}", {'name': 'Updated'}, method='PUT')
            assert updated['name'] == 'Updated' and updated['active'] is False and updated['note'] == 'updated'
            assert request(f"/users/{empty['id']}") == updated
            assert request(f"/users/{empty['id']}", method='DELETE') is True
            request(f"/users/{empty['id']}", status=404)
            if backend == 'mongodb':
                # This adapter exposes document persistence, not a replica-set
                # session/transaction implementation or relational FK checks.
                orphan = request('/orphan', {})
                assert orphan['owner_id'] == 999999
                request('/commit', {}, status=500)
                request('/rollback', {}, status=500)
                assert find(request, 'committed') == []
            else:
                request('/orphan', {}, status=500)
                committed = request('/commit', {})
                assert committed['name'] == 'committed'
                request('/rollback', {}, status=404)
                assert find(request, 'committed') == [committed]
            assert find(request, 'rolled-back') == [] and find(request, 'nested-rolled-back') == []
            persisted = request('/users')
            assert request('/fixture/stats')['leased'] == 0

        with serve() as request:
            assert request('/users') == persisted
            assert request('/fixture/stats')['leased'] == 0
        assert '1 migration(s) rolled back' in run('migrate:rollback')
        assert '[ ] CreateDatabaseAcceptance' in run('migrate:status')
        assert '1 migration(s) applied' in run('migrate')
        with serve() as request:
            assert request('/users') == [] and request('/posts') == []
        assert '1 migration(s) rolled back' in run('migrate:rollback')
    print(f'Installed {backend} application: migrations, bindings, CRUD, hidden/null/Unicode values, '
          'pagination, eager/inverse relationships, transaction capabilities, restart and rollback passed')


if __name__ == '__main__':
    main()
