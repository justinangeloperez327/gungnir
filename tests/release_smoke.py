"""Create, compile and serve an application using only the installed SDK."""
import os
from pathlib import Path
import socket
import subprocess
import sys
import tempfile
import time
import urllib.request

prefix = Path(sys.argv[1]).resolve()
cli = prefix / "bin" / ("gungnir.exe" if os.name == "nt" else "gungnir")
env = os.environ.copy()
env.pop("GUNGNIR_CMAKE_PREFIX", None)  # Exercise automatic SDK discovery.
env["CMAKE_BUILD_PARALLEL_LEVEL"] = "2"
with tempfile.TemporaryDirectory(prefix="gungnir-release-smoke-") as temporary:
    project = Path(temporary) / "app"
    subprocess.run([str(cli), "new", "ReleaseSmoke", str(project)], env=env, check=True, timeout=30)
    with socket.socket() as reservation:
        reservation.bind(("127.0.0.1", 0))
        port = reservation.getsockname()[1]
    (project / ".env").write_text(f"APP_HOST=127.0.0.1\nAPP_PORT={port}\nDB_CONNECTION=\nVIEW_PATH=views\n")
    # This checks the ordinary module/route path rather than generated C++ edits.
    (project / "app/controllers/ReleaseCheck.gnr").write_text("controller ReleaseCheck {index(){return text('release-ready');}}")
    (project / "routes/web.gnr").write_text("Route::get('/', ReleaseCheck::index);")
    subprocess.run([str(cli), "build", "--release"], cwd=project, env=env, check=True, timeout=240)
    candidates = [project / ".gungnir/build/app", project / ".gungnir/build/app.exe", project / ".gungnir/build/Release/app.exe"]
    binary = next(path for path in candidates if path.is_file())
    with (project / "server.log").open("w+") as log:
        process = subprocess.Popen([str(binary)], cwd=project, env=env, stdout=log, stderr=log)
        try:
            deadline = time.monotonic() + 15
            while time.monotonic() < deadline:
                if process.poll() is not None:
                    log.seek(0)
                    raise AssertionError("Installed app exited: " + log.read())
                try:
                    with urllib.request.urlopen(f"http://127.0.0.1:{port}/", timeout=2) as response:
                        assert response.status == 200 and response.read() == b"release-ready"
                    break
                except OSError:
                    time.sleep(0.05)
            else:
                raise AssertionError("Installed app did not serve HTTP")
        finally:
            process.terminate()
            try:
                process.wait(timeout=10)
            except subprocess.TimeoutExpired:
                process.kill()
                process.wait(timeout=5)
print("Installed SDK creates, builds and serves a Release application")
