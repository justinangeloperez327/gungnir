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
# Hosted Windows runners may have a console host that cannot allocate or
# expose a console window (AllocConsole can return ERROR_ACCESS_DENIED).
# Still exercise the real watcher/build/recovery lifecycle in headless CI.
console_available = bool(kernel32.GetConsoleWindow())
created_console = False
if not console_available:
    if kernel32.AllocConsole():
        created_console = True
        console_available = True
    else:
        error = ctypes.get_last_error()
        print(
            f"Windows console unavailable (error {error}); "
            "Ctrl+Break shutdown assertion will be skipped",
            flush=True,
        )

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

        # Verify that the app runs its shutdown hooks on Windows restarts,
        # rather than only observing that its TCP port became available.
        bootstrap = project / "bootstrap/app.hpp"
        source = bootstrap.read_text(encoding="utf-8")
        hook = (
            '    app.on_shutdown([](gungnir::Application& application) {\n'
            '        std::ofstream(application.base_path() / "shutdown.marker", '
            'std::ios::app) << "stopped\\n";\n'
            '    });\n'
        )
        anchor = "    // Add service bindings, middleware, and providers here."
        assert source.count(anchor) == 1, "Generated bootstrap hook location changed"
        bootstrap.write_text(
            "#include <fstream>\n" + source.replace(anchor, hook + anchor),
            encoding="utf-8",
        )
        shutdown_marker = project / "shutdown.marker"

        def shutdown_count():
            return (shutdown_marker.read_text(encoding="utf-8").count("stopped")
                    if shutdown_marker.is_file() else 0)

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
                if console_available:
                    assert shutdown_count() == 1, (
                        f"Old app did not complete graceful shutdown: {log_tail()}"
                    )

                # Reject source errors without replacing the still-serving child.
                controller.write_text(
                    "controller HomeController { Response index() { return unknown; } }\n",
                    encoding="utf-8",
                )
                # Source validation reports GNR diagnostics before a native
                # build starts, so it does not produce the build-failed marker.
                # Check the compiler error and that the previous child survives.
                wait_log("GNR2202", watcher)
                wait_http("dev-second", watcher, seconds=10)

                # Recovery must compile, release the prior executable, restart,
                # and serve new behavior on the original port.
                write_controller("dev-third")
                wait_http("dev-third", watcher)
                assert len(list(dev_dir.glob("app-*.exe"))) == 1, log_tail()
                if console_available:
                    assert shutdown_count() == 2, (
                        f"Restarted app did not complete graceful shutdown: {log_tail()}"
                    )

                if console_available:
                    watcher.send_signal(signal.CTRL_BREAK_EVENT)
                    return_code = watcher.wait(timeout=25)
                    expected = 128 + signal.SIGBREAK
                    assert return_code == expected, (
                        f"Ctrl+Break exit code {return_code}, expected {expected}\n{log_tail()}"
                    )

                    assert shutdown_count() == 3, (
                        f"Watcher exit skipped application shutdown: {log_tail()}"
                    )

                    # A graceful shutdown must stop the child and remove the
                    # temporary copied executable.
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
                else:
                    print("SKIP: Windows console-specific Ctrl+Break test "
                          "(watch/rebuild/recovery exercised)", flush=True)
            finally:
                if watcher.poll() is None:
                    # A failed assertion must never strand the watcher or app
                    # on a hosted runner. taskkill /T includes child processes.
                    subprocess.run(
                        ["taskkill", "/PID", str(watcher.pid), "/T", "/F"],
                        capture_output=True, text=True, timeout=20
                    )
                    watcher.wait(timeout=15)
        print("Windows installed-SDK dev watcher startup, hot reload, and failure recovery passed"
              + (", including Ctrl+Break shutdown" if console_available
                 else " (console-specific shutdown was skipped)"))
finally:
    if created_console:
        kernel32.FreeConsole()
