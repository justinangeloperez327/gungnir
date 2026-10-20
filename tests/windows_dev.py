"""Windows installed-SDK acceptance test: watch, rebuild, recovery, shutdown.

This test requires Windows because it delivers CTRL_BREAK_EVENT to the
development CLI's independent console process group. It intentionally uses a
project path containing spaces and tests the installed SDK, not build-tree DLLs.
"""
import ctypes
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

if os.name != "nt":
    raise SystemExit("windows_dev.py requires Windows")
if len(sys.argv) != 2:
    raise SystemExit("Usage: python tests/windows_dev.py <installed-gungnir.exe>")

cli = Path(sys.argv[1]).resolve()
if not cli.is_file():
    raise AssertionError(f"Installed Gungnir CLI missing: {cli}")

env = os.environ.copy()
env.pop("GUNGNIR_CMAKE_PREFIX", None)
env["CMAKE_BUILD_PARALLEL_LEVEL"] = "2"

with socket.socket() as probe:
    probe.bind(("127.0.0.1", 0))
    port = probe.getsockname()[1]

kernel32 = ctypes.WinDLL("kernel32", use_last_error=True)
kernel32.GetConsoleWindow.restype = ctypes.c_void_p
kernel32.AllocConsole.restype = ctypes.c_int
kernel32.FreeConsole.restype = ctypes.c_int
created_console = not kernel32.GetConsoleWindow()
if created_console and not kernel32.AllocConsole():
    raise OSError(ctypes.get_last_error(), "Unable to allocate console for Ctrl+Break test")

try:
    with tempfile.TemporaryDirectory(prefix="gungnir-windows-dev-") as temporary:
        project = Path(temporary) / "application with spaces"
        created = subprocess.run(
            [str(cli), "new", "WindowsDevSmoke", str(project)],
            cwd=temporary, env=env, capture_output=True, text=True, timeout=30
        )
        if created.returncode != 0:
            raise AssertionError(f"Could not generate test project:\n{created.stdout}\n{created.stderr}")

        controller = project / "app/controllers/HomeController.gnr"
        def write_controller(value):
            controller.write_text(
                'controller HomeController { Response index() { return text("'
                + value + '"); } }\n', encoding="utf-8"
            )

        write_controller("dev-first")
        (project / ".env").write_text(
            f"APP_ENV=testing\nAPP_DEBUG=true\nAPP_HOST=127.0.0.1\n"
            f"APP_PORT={port}\nVIEW_PATH=views\nDB_CONNECTION=\n",
            encoding="utf-8",
        )
        log_path = project / "windows-dev.log"
        def log_tail():
            return log_path.read_text(encoding="utf-8", errors="replace")[-8000:]

        def wait_http(value, process, seconds=180):
            deadline = time.monotonic() + seconds
            while time.monotonic() < deadline:
                if process.poll() is not None:
                    raise AssertionError(
                        f"gungnir dev exited with {process.returncode} while waiting for "
                        f"{value!r}\n{log_tail()}"
                    )
                try:
                    with urllib.request.urlopen(
                        f"http://127.0.0.1:{port}/", timeout=2
                    ) as response:
                        if response.status == 200 and response.read().decode() == value:
                            return
                except (OSError, urllib.error.URLError):
                    pass
                time.sleep(0.15)
            raise AssertionError(f"Timed out waiting for {value!r}:\n{log_tail()}")

        def wait_log(needle, process, seconds=45):
            deadline = time.monotonic() + seconds
            while time.monotonic() < deadline:
                if needle in log_tail():
                    return
                if process.poll() is not None:
                    raise AssertionError(f"Watcher exited while waiting for {needle}:\n{log_tail()}")
                time.sleep(0.15)
            raise AssertionError(f"Expected log {needle!r} not found:\n{log_tail()}")

        with log_path.open("w", encoding="utf-8") as log:
            # CREATE_NEW_PROCESS_GROUP allows CTRL_BREAK_EVENT to target the
            # watcher without signalling the separately grouped application.
            watcher = subprocess.Popen(
                [str(cli), "run", "dev"], cwd=project, env=env,
                stdout=log, stderr=subprocess.STDOUT,
                creationflags=subprocess.CREATE_NEW_PROCESS_GROUP,
            )
            try:
                wait_http("dev-first", watcher)
                dev_dir = project / ".gungnir/dev"
                assert len(list(dev_dir.glob("app-*.exe"))) == 1, log_tail()

                write_controller("dev-second")
                wait_http("dev-second", watcher)
                assert len(list(dev_dir.glob("app-*.exe"))) == 1, log_tail()

                # Reject source errors without replacing the still-serving child.
                controller.write_text(
                    "controller HomeController { Response index() { return unknown; } }\n",
                    encoding="utf-8",
                )
                wait_log("gungnir dev: build failed", watcher)
                wait_http("dev-second", watcher, seconds=10)

                # Recovery must compile, release the prior executable, restart,
                # and serve new behavior on the original port.
                write_controller("dev-third")
                wait_http("dev-third", watcher)
                assert len(list(dev_dir.glob("app-*.exe"))) == 1, log_tail()

                watcher.send_signal(signal.CTRL_BREAK_EVENT)
                return_code = watcher.wait(timeout=25)
                expected = 128 + signal.SIGBREAK
                assert return_code == expected, (
                    f"Ctrl+Break exit code {return_code}, expected {expected}\n{log_tail()}"
                )

                # The child must be gone and the temporary copied executable
                # must be unlinked after graceful watcher shutdown.
                deadline = time.monotonic() + 10
                while time.monotonic() < deadline:
                    with socket.socket() as probe:
                        probe.settimeout(1)
                        disconnected = probe.connect_ex(("127.0.0.1", port)) != 0
                    if disconnected and not list(dev_dir.glob("app-*.exe")):
                        break
                    time.sleep(0.1)
                else:
                    raise AssertionError(
                        f"Watcher left an HTTP listener or copied executable:\n{log_tail()}"
                    )
            finally:
                if watcher.poll() is None:
                    # A failed assertion must never strand the watcher or app
                    # on a hosted runner. taskkill /T includes child processes.
                    subprocess.run(
                        ["taskkill", "/PID", str(watcher.pid), "/T", "/F"],
                        capture_output=True, text=True, timeout=20
                    )
                    watcher.wait(timeout=15)
        print("Windows installed-SDK dev watcher startup, hot reload, failure recovery and Ctrl+Break shutdown passed")
finally:
    if created_console:
        kernel32.FreeConsole()
