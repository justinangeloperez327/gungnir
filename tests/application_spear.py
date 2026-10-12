"""Installed SDK + unchanged .gnr sources -> real HTTP Spear application."""
from concurrent.futures import ThreadPoolExecutor
from contextlib import contextmanager
import hashlib
import http.client
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
    build, cli_name = Path(sys.argv[1]).resolve(), Path(sys.argv[2]).name
    env = os.environ.copy()
    env.setdefault('CMAKE_BUILD_PARALLEL_LEVEL', '2')
    for key in list(env):
        if key.startswith(('APP_', 'DB_', 'VIEW_')):
            env.pop(key)
    with tempfile.TemporaryDirectory(prefix='gungnir-spear-') as temporary:
        root = Path(temporary)
        sdk, project = root / 'installed sdk', root / 'Spear application'
        subprocess.run(['cmake', '--install', str(build), '--prefix', str(sdk)], env=env,
                       check=True, stdout=subprocess.DEVNULL, timeout=90)
        cli = sdk / 'bin' / cli_name
        env['GUNGNIR_CMAKE_PREFIX'] = str(sdk)
        subprocess.run([str(cli), 'new', 'SpearExample', str(project)], env=env,
                       check=True, stdout=subprocess.DEVNULL, timeout=30)
        assert (project / 'views/welcome.spear').is_file()
        assert not (project / 'views/welcome.html').exists()
        fixture = sdk / 'share/gungnir/examples/spear'
        for directory in ('app', 'routes', 'views', 'bootstrap'):
            shutil.copytree(fixture / directory, project / directory, dirs_exist_ok=True)
        (project / '.env').write_text('APP_ENV=testing\nAPP_DEBUG=false\nVIEW_PATH=views\nDB_CONNECTION=\n')
        routes = project / 'routes/web.gnr'
        routes.write_text(routes.read_text() + "\nRoute::get('/broken', HomeController::broken);\nRoute::get('/cycle', HomeController::cycle);\nRoute::get('/invalid', HomeController::invalid);\n")
        (project / 'views/broken.spear').write_text('line one\n@if(true){{ name + 1 }}@endif')
        (project / 'views/cycle.spear').write_text("@include('cycle')")
        bootstrap = project / 'bootstrap/app.hpp'
        bootstrap.write_text(bootstrap.read_text().replace('#include <algorithm>', '#include <algorithm>\n#include <iostream>').replace(
            '// App-specific services and helpers go here.', '''
    app.router().use([](gungnir::Request& request, gungnir::Next next) -> gungnir::Task<gungnir::Response> {
        try { co_return co_await next(request); }
        catch (const std::exception& error) { std::cerr << error.what() << '\\n'; throw; }
    });'''))
        result = subprocess.run([str(cli), 'build'], cwd=project, env=env, text=True,
                                stdout=subprocess.PIPE, stderr=subprocess.STDOUT, timeout=300)
        assert result.returncode == 0, result.stdout
        binary = project / '.gungnir/build' / ('app.exe' if os.name == 'nt' else 'app')
        assert binary.is_file()
        def artifacts():
            return {file: (hashlib.sha256(file.read_bytes()).hexdigest(), file.stat().st_mtime_ns)
                    for file in (project / '.gungnir').rglob('*') if file.is_file()
                    and (file == binary or file.suffix in ('.cpp', '.hpp', '.h', '.o', '.obj', '.a', '.lib'))}
        unchanged = artifacts()

        @contextmanager
        def serve(mode='testing', development=False, view_path='views'):
            with socket.socket() as available:
                available.bind(('127.0.0.1', 0))
                port = available.getsockname()[1]
            process_env = env | {'APP_HOST': '127.0.0.1', 'APP_PORT': str(port), 'APP_ENV': mode, 'VIEW_PATH': view_path}
            log_path = project / f'{mode}-{development}.log'
            with log_path.open('w+') as log:
                command = [str(cli), 'dev'] if development else [str(binary)]
                process = subprocess.Popen(command, cwd=project, env=process_env, stdout=log, stderr=subprocess.STDOUT)

                def request(path, expected=200):
                    connection = http.client.HTTPConnection('127.0.0.1', port, timeout=10)
                    try:
                        connection.request('GET', path)
                        response = connection.getresponse()
                        body = response.read().decode()
                        assert response.status == expected, (path, response.status, body)
                        if expected in (200, 201, 202) and path != '/ready':
                            assert response.getheader('Content-Type') == 'text/html; charset=utf-8'
                        return body
                    finally:
                        connection.close()

                try:
                    deadline = time.monotonic() + 60
                    while time.monotonic() < deadline:
                        assert process.poll() is None, log_path.read_text()
                        try:
                            assert request('/ready') == 'ready'
                            break
                        except OSError:
                            time.sleep(0.05)
                    else:
                        raise AssertionError('Application did not become ready: ' + log_path.read_text())
                    yield request, process, log_path
                except Exception:
                    log.flush()
                    print(log_path.read_text(), file=sys.stderr)
                    raise
                finally:
                    if process.poll() is None:
                        process.terminate()
                        try:
                            process.wait(timeout=15)
                        except subprocess.TimeoutExpired:
                            process.kill()
                            process.wait(timeout=5)

        partial = project / 'views/partials/user.spear'
        original = partial.read_text()
        with serve() as (request, process, log):
            body = request('/?title=' + urllib.parse.quote('<script>雪</script>'), 201)
            assert '<title>&lt;script&gt;雪&lt;/script&gt;</title>' in body
            assert '<h1>&lt;script&gt;雪&lt;/script&gt;</h1>' in body
            assert '<li>1: Ada</li>' in body and '<li>2: &lt;Bob&gt;</li>' in body
            assert '<em>Trusted</em>' in body and 'READY' in body and 'Spear example' in body
            assert 'must-stay-hidden' not in body and 'This comment' not in body
            assert 'No users' in request('/empty') and 'Empty list' in request('/empty')
            assert 'Legacy template' in request('/legacy')
            assert request('/numbers', 202).strip() == '18446744073709551615|-9223372036854775808|true'

            def isolated(index):
                title = f'request-{index}'
                value = request('/?title=' + title, 201)
                assert f'<h1>{title}</h1>' in value and f'<title>{title}</title>' in value
            with ThreadPoolExecutor(max_workers=8) as workers:
                list(workers.map(isolated, range(40)))
            stamp = partial.stat().st_mtime_ns
            partial.write_text(original.replace('<li>', '<p> ').replace('</li>', '</p> '))
            # Content changes, including unchanged timestamps, invalidate the parsed include.
            os.utime(partial, ns=(stamp, stamp))
            assert '<p> 1: Ada</p> ' in request('/', 201)
            request('/broken', 500)
            assert 'broken.spear:2:' in log.read_text()
            (project / 'views/broken.spear').write_text('recovered {{ title }}')
            assert request('/broken') == 'recovered '
            request('/cycle', 500)
            request('/invalid', 500)
            assert process.poll() is None

        partial.write_text(original)
        with serve('production') as (request, process, log):
            assert '<li>1: Ada</li>' in request('/', 201)
            partial.write_text(original.replace('<li>', '<aside>').replace('</li>', '</aside>'))
            assert '<li>1: Ada</li>' in request('/', 201), 'Production AST cache reloaded without invalidation'
            assert process.poll() is None
        with serve('production') as (request, process, log):
            assert '<aside>1: Ada</aside>' in request('/', 201), 'Restart did not load the new template'
        partial.write_text(original)
        assert artifacts() == unchanged, 'Template requests changed native build artifacts'
        # A configured view root inside config/ must also avoid native rebuilds.
        shutil.copytree(project / 'views', project / 'config/templates', dirs_exist_ok=True)
        partial = project / 'config/templates/partials/user.spear'
        with serve(development=True, view_path='config/templates') as (request, process, log):
            assert '<li>1: Ada</li>' in request('/', 201)
            unchanged = artifacts()
            copies = sorted((project / '.gungnir/dev').glob('app-*'))
            assert len(copies) == 1
            before = log.read_text().count('gungnir dev: building')
            partial.write_text(original.replace('<li>', '<div>').replace('</li>', '</div>'))
            assert '<div>1: Ada</div>' in request('/', 201)
            deadline = time.monotonic() + 1
            while time.monotonic() < deadline:
                assert process.poll() is None
                time.sleep(0.05)
            assert sorted((project / '.gungnir/dev').glob('app-*')) == copies
            assert log.read_text().count('gungnir dev: building') == before
        assert artifacts() == unchanged, 'Template-only development edit changed native build artifacts'
        print('Installed Spear application, escaping, hidden models, exact numbers, concurrent requests, compatibility, source diagnostics, development reload and production restart passed')


if __name__ == '__main__':
    main()
