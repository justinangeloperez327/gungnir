"""Manual operations for the single-host Linux reference deployment."""
import argparse
from contextlib import contextmanager
import fcntl
import hashlib
import http.client
import json
import os
from pathlib import Path, PurePosixPath
import re
import shutil
import sqlite3
import subprocess
import tempfile
import time

HERE = Path(__file__).resolve().parent
APPLICATION = ["application:" + role for role in ("web-a", "web-b", "worker", "scheduler")]
STOPPED = {"STOPPED", "FATAL"}


def require(condition, message):
    if not condition:
        raise ValueError(message)


def safe_path(value):
    value = str(value)
    require(re.fullmatch(r"/[A-Za-z0-9_./-]+", value) and os.path.normpath(value) == value,
            "Use an absolute, normalized path containing letters, numbers, /, ., _ or -")
    return Path(value)


def private_write(path, text):
    path.parent.mkdir(parents=True, exist_ok=True, mode=0o700)
    fd, temporary = tempfile.mkstemp(dir=path.parent, prefix=".write-")
    try:
        with os.fdopen(fd, "w") as output:
            output.write(text)
            output.flush()
            os.fsync(output.fileno())
        os.replace(temporary, path)
    finally:
        Path(temporary).unlink(missing_ok=True)


def read_env(path):
    result = {}
    for line in Path(path).read_text().splitlines():
        if not line.strip() or line.lstrip().startswith("#"):
            continue
        key, separator, value = line.partition("=")
        key, value = key.strip(), value.strip()
        require(separator and re.fullmatch(r"[A-Z][A-Z0-9_]*", key) and key not in result,
                "Deployment environment entries must have unique KEY=value names")
        require(not any(char in value for char in "'\"#\r\n"),
                "The deployment environment uses literal, unquoted values")
        result[key] = value
    return result


def digest(path):
    with path.open("rb") as stream:
        return hashlib.file_digest(stream, "sha256").hexdigest()


def release(root, revision):
    require(re.fullmatch(r"[A-Za-z0-9][A-Za-z0-9_-]{0,63}", revision), "Invalid release revision")
    result = root / "releases" / revision
    require(result.is_dir() and not result.is_symlink(), "Release must be a directory inside this deployment")
    manifest = json.loads((result / "release.json").read_text())
    require(manifest.get("schema") == 1 and manifest.get("revision") == revision, "Invalid release manifest")
    expected = {"REVISION", ".gungnir/build/app", ".gungnir/build/background", ".gungnir/build/migrations"}
    require(set(manifest.get("sha256", {})) == expected, "Release manifest must cover the expected executables and revision")
    for name, checksum in manifest["sha256"].items():
        target = result / name
        require(target.is_file() and not target.is_symlink() and digest(target) == checksum, "Release integrity check failed")
    require((result / "REVISION").read_text().strip() == revision, "Release revision does not match its manifest")
    for name in expected - {"REVISION"}:
        require(os.access(result / name, os.X_OK), "Release executable is not executable")
    return result


def build(root, args):
    sdk = safe_path(args.sdk)
    cli = sdk / "bin/gungnir"
    require(cli.is_file(), "SDK must contain bin/gungnir")
    require(re.fullmatch(r"[A-Za-z0-9][A-Za-z0-9_-]{0,63}", args.revision), "Invalid release revision")
    target = root / "releases" / args.revision
    require(not target.exists(), "Release already exists; build a new revision")
    target.parent.mkdir(parents=True, exist_ok=True)
    shutil.copytree(HERE / "project", target)
    (target / "REVISION").write_text(args.revision + "\n")
    environment = dict(os.environ, GUNGNIR_CMAKE_PREFIX=str(sdk), CMAKE_BUILD_PARALLEL_LEVEL="2")
    subprocess.run([str(cli), "build", "--release"], cwd=target, env=environment, check=True)
    version = subprocess.check_output([str(cli), "--version"], text=True).strip()
    names = ["REVISION", ".gungnir/build/app", ".gungnir/build/background", ".gungnir/build/migrations"]
    private_write(target / "release.json", json.dumps({
        "schema": 1, "revision": args.revision, "sdk_version": version,
        "sha256": {name: digest(target / name) for name in names}
    }, indent=2) + "\n")
    release(root, args.revision)
    print("Built release", args.revision)


def binary(name):
    result = shutil.which(name)
    require(result is not None, "Install the required Linux tool: " + name)
    return str(safe_path(result))


def configure(root, args):
    require(not (root / "deployment.json").exists(), "Deployment already configured")
    target = release(root, args.revision)
    env = read_env(safe_path(args.env_file))
    require(env.get("APP_ENV") == "production" and env.get("APP_DEBUG") == "false" and
            env.get("APP_HOST") == "127.0.0.1", "Require production, debug=false and a loopback app listener")
    require(env.get("DB_CONNECTION") == "sqlite" and env.get("DB_POOL_SIZE") == "1",
            "This example requires the explicit single-host SQLite profile")
    require(env.get("DEPLOY_DATA_DIR") == str(root / "data") and
            env.get("DB_DATABASE") == str(root / "data/application.sqlite"), "Shared data paths must belong to this deployment")
    for key in ("APP_API_TOKEN", "REDIS_PASSWORD"):
        require(re.fullmatch(r"[a-f0-9]{64}", env.get(key, "")), "Generate a fresh 64-character hex secret for " + key)
    require(env["APP_API_TOKEN"] != env["REDIS_PASSWORD"], "Use independent API and Redis secrets")
    domain = env.get("APP_PUBLIC_HOST", "")
    label = r"[a-z0-9](?:[a-z0-9-]{0,61}[a-z0-9])?"
    require(len(domain) <= 253 and re.fullmatch(label + r"(?:\." + label + r")*", domain), "Invalid public DNS hostname")
    require(re.fullmatch(r"[a-z0-9][a-z0-9-]{0,63}", env.get("APP_NAMESPACE", "")), "Invalid application namespace")
    require(re.fullmatch(r"[0-9]+", env.get("REDIS_PORT", "")), "Invalid Redis port")
    ports = [args.http_port, args.https_port, args.web_a_port, args.web_b_port, int(env["REDIS_PORT"])]
    require(all(1 <= port <= 65535 for port in ports) and len(set(ports)) == len(ports), "Use five distinct valid ports")
    certificate, private_key = safe_path(args.certificate), safe_path(args.private_key)
    require(certificate.is_file() and private_key.is_file(), "Provision the TLS certificate and private key")
    for directory in ("config", "run/nginx", "logs", "data/storage", "data/redis"):
        (root / directory).mkdir(parents=True, exist_ok=True, mode=0o700)
    private_write(root / "config/deployment.env", Path(args.env_file).read_text())
    tools = {name: binary(name) for name in ("nginx", "redis-server", "redis-cli", "redis-check-rdb", "supervisord", "supervisorctl")}
    options = """autostart=true
autorestart=true
startsecs=1
startretries=3
stopsignal=TERM
stopwaitsecs=45
stopasgroup=true
killasgroup=true
stdout_logfile=@ROOT@/logs/%(program_name)s.jsonl
stdout_logfile_maxbytes=20MB
stdout_logfile_backups=5
stderr_logfile=@ROOT@/logs/%(program_name)s.error.log
stderr_logfile_maxbytes=10MB
stderr_logfile_backups=3"""
    substitutions = {
        "ROOT": str(root), "DOMAIN": domain, "CERTIFICATE": str(certificate), "PRIVATE_KEY": str(private_key),
        "HTTP": str(args.http_port), "HTTPS": str(args.https_port), "WEB_A": str(args.web_a_port), "WEB_B": str(args.web_b_port),
        "REDIS_PORT": env["REDIS_PORT"], "REDIS_PASSWORD": env["REDIS_PASSWORD"],
        "NGINX": tools["nginx"], "REDIS_SERVER": tools["redis-server"], "SUPERVISORD": tools["supervisord"],
        # Keep all processes under the account invoking this reference. Production
        # uses an unprivileged service user; isolated root CI must not chown to an
        # unmapped nginx nobody user or lose access to the private data directory.
        "NGINX_USER": "user root;" if os.geteuid() == 0 else "",
        "APPLICATION_OPTIONS": options,
    }
    for name in ("nginx.conf", "redis.conf", "supervisord.conf", "gungnir-example.service"):
        text = (HERE / "templates" / (name + ".in")).read_text()
        # The application options contain a ROOT placeholder themselves.
        for key in ("APPLICATION_OPTIONS", *[key for key in substitutions if key != "APPLICATION_OPTIONS"]):
            text = text.replace("@" + key + "@", substitutions[key])
        require(not re.search(r"@[A-Z_]+@", text), "Unresolved deployment template")
        private_write(root / "config" / name, text)
    subprocess.run([tools["nginx"], "-t", "-p", str(root / "run/nginx") + "/", "-c", str(root / "config/nginx.conf")], check=True)
    os.symlink(target, root / "current")
    private_write(root / "deployment.json", json.dumps({
        "schema": 1, "root": str(root), "domain": domain, "web_ports": [args.web_a_port, args.web_b_port],
        "http_port": args.http_port, "https_port": args.https_port, "tools": tools
    }, indent=2) + "\n")
    print("Configured deployment; run Supervisor with", root / "config/supervisord.conf")


def metadata(root):
    result = json.loads((root / "deployment.json").read_text())
    require(result.get("schema") == 1 and result.get("root") == str(root), "Invalid deployment metadata")
    return result


def control(root, *arguments, check=True):
    command = [metadata(root)["tools"]["supervisorctl"], "-c", str(root / "config/supervisord.conf"), *arguments]
    result = subprocess.run(command, capture_output=True, text=True, timeout=90)
    require(not check or result.returncode == 0, "Supervisor operation failed: " + result.stdout.strip())
    return result.stdout


def states(root):
    result = {}
    for line in control(root, "status", check=False).splitlines():
        pieces = line.split()
        if len(pieces) >= 2:
            result[pieces[0]] = pieces[1]
    require(set(result) == {*APPLICATION, "redis", "proxy"}, "Supervisor control is unavailable or has an unexpected process set")
    return result


def ensure_stopped(root, names):
    current = states(root)
    require(all(current[name] in STOPPED for name in names), "Stop the relevant supervised processes before this operation")


def pause(root):
    private_write(root / "data/maintenance", "maintenance\n")
    current = states(root)
    active = [name for name in APPLICATION if current[name] not in STOPPED]
    if active:
        control(root, "stop", *active, check=False)
    ensure_stopped(root, APPLICATION)
    print("Application admission and background work paused")


def redis_command(root, *arguments):
    env = read_env(root / "config/deployment.env")
    command = [metadata(root)["tools"]["redis-cli"], "-h", "127.0.0.1", "-p", env["REDIS_PORT"], "--raw", *arguments]
    result = subprocess.run(command, env=dict(os.environ, REDISCLI_AUTH=env["REDIS_PASSWORD"]),
                            capture_output=True, text=True, timeout=60)
    require(result.returncode == 0 and not result.stdout.startswith(("ERR", "NOAUTH", "WRONGPASS")), "Redis operation failed")
    return result.stdout.strip()


def resume_processes(root, timeout):
    current = states(root)
    if current["redis"] in STOPPED:
        control(root, "start", "redis")
    pending = root / "data/redis/enable-aof"
    if pending.exists():
        require(redis_command(root, "CONFIG", "SET", "appendonly", "yes") == "OK", "Could not enable restored AOF")
        deadline = time.monotonic() + timeout
        while True:
            info = redis_command(root, "INFO", "persistence")
            if "aof_rewrite_in_progress:0" in info and "aof_last_bgrewrite_status:ok" in info:
                break
            require(time.monotonic() < deadline, "Restored AOF rewrite did not complete")
            time.sleep(0.1)
        require(redis_command(root, "CONFIG", "REWRITE") == "OK", "Could not persist restored Redis configuration")
        pending.unlink()
    (root / "data/maintenance").unlink(missing_ok=True)
    current = states(root)
    stopped = [name for name in APPLICATION if current[name] in STOPPED]
    if stopped:
        control(root, "start", *stopped)
    meta = metadata(root)
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        ready = all(value == "RUNNING" for value in states(root).values())
        for port in meta["web_ports"]:
            try:
                connection = http.client.HTTPConnection("127.0.0.1", port, timeout=1)
                connection.request("GET", "/health/ready", headers={"Host": meta["domain"]})
                response = connection.getresponse()
                ready = ready and response.status == 200 and json.loads(response.read()).get("ready") is True
                connection.close()
            except (OSError, http.client.HTTPException, ValueError):
                ready = False
        if ready:
            print("Both HTTP instances and all supervised processes are ready")
            return
        time.sleep(0.1)
    raise ValueError("Deployment did not become ready; inspect the protected process logs")


def resume(root, timeout=30):
    try:
        resume_processes(root, timeout)
    except Exception:
        # Failed startup must leave admission and background writers paused.
        private_write(root / "data/maintenance", "maintenance\n")
        try:
            pause(root)
        except Exception:
            pass  # The maintenance marker still blocks managed startup/admission.
        raise


def regular_tree(directory):
    require(directory.is_dir() and not directory.is_symlink(), "Expected a regular backup directory")
    for item in directory.rglob("*"):
        require(not item.is_symlink() and (item.is_file() or item.is_dir()), "Backup trees must not contain links or special files")


def verify_backup(root, directory):
    directory = safe_path(directory)
    regular_tree(directory)
    manifest = json.loads((directory / "backup.json").read_text())
    env = read_env(root / "config/deployment.env")
    require(manifest.get("schema") == 1 and manifest.get("namespace") == env["APP_NAMESPACE"], "Backup belongs to a different profile or namespace")
    expected = manifest.get("sha256", {})
    require({"application.sqlite", "redis.rdb"} <= set(expected), "Incomplete backup manifest")
    actual = {str(path.relative_to(directory)) for path in directory.rglob("*") if path.is_file()} - {"backup.json"}
    require(actual == set(expected), "Backup file set does not match its manifest")
    for name, checksum in expected.items():
        parsed = PurePosixPath(name)
        require(not parsed.is_absolute() and ".." not in parsed.parts and
                (name in {"application.sqlite", "redis.rdb"} or name.startswith("storage/")), "Invalid backup member path")
        require(re.fullmatch(r"[a-f0-9]{64}", checksum) and digest(directory / name) == checksum, "Backup integrity check failed")
    with sqlite3.connect("file:" + str(directory / "application.sqlite") + "?mode=ro&immutable=1", uri=True) as connection:
        require(connection.execute("PRAGMA integrity_check").fetchone()[0] == "ok", "SQLite backup is corrupt")
    subprocess.run([metadata(root)["tools"]["redis-check-rdb"], str(directory / "redis.rdb")],
                   check=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, timeout=30)
    return manifest


def backup(root, output):
    ensure_stopped(root, APPLICATION)
    require((root / "data/maintenance").is_file(), "Pause application admission before taking a coordinated backup")
    output = safe_path(output)
    require(not output.exists(), "Backup destination must not already exist")
    require(root / "data" not in output.parents, "Keep backups outside the live data directory")
    output.parent.mkdir(parents=True, exist_ok=True)
    temporary = Path(tempfile.mkdtemp(dir=output.parent, prefix=".backup-"))
    try:
        deadline = time.monotonic() + 30
        def progress(*_):
            require(time.monotonic() < deadline, "SQLite backup deadline exceeded")
        with sqlite3.connect("file:" + str(root / "data/application.sqlite") + "?mode=ro", uri=True) as source:
            with sqlite3.connect(temporary / "application.sqlite") as target:
                source.backup(target, pages=64, progress=progress, sleep=0.05)
                # The copied header can retain WAL mode. Publish one standalone
                # snapshot file, and never create WAL/SHM files during verification.
                require(target.execute("PRAGMA journal_mode = DELETE").fetchone()[0] == "delete",
                        "Could not finalize the standalone SQLite snapshot")
        require(redis_command(root, "SAVE") == "OK", "Redis snapshot failed")
        shutil.copyfile(root / "data/redis/dump.rdb", temporary / "redis.rdb")
        regular_tree(root / "data/storage")
        shutil.copytree(root / "data/storage", temporary / "storage")
        files = {str(path.relative_to(temporary)): digest(path) for path in temporary.rglob("*") if path.is_file()}
        private_write(temporary / "backup.json", json.dumps({
            "schema": 1, "namespace": read_env(root / "config/deployment.env")["APP_NAMESPACE"],
            "revision": (root / "current/REVISION").read_text().strip(), "sha256": files
        }, indent=2) + "\n")
        verify_backup(root, temporary)
        os.replace(temporary, output)
        print("Verified coordinated backup", output)
    finally:
        if temporary.exists():
            shutil.rmtree(temporary)


def restore(root, source):
    ensure_stopped(root, [*APPLICATION, "redis"])
    require((root / "data/maintenance").is_file(), "Pause application admission before restoring")
    source = safe_path(source)
    verify_backup(root, source)
    configuration = root / "config/redis.conf"
    text = configuration.read_text()
    require(re.search(r"(?m)^appendonly (yes|no)$", text), "Redis configuration lacks appendonly")
    temporary = Path(tempfile.mkdtemp(dir=root, prefix=".restore-"))
    try:
        shutil.copyfile(source / "application.sqlite", temporary / "application.sqlite")
        (temporary / "redis").mkdir(mode=0o700)
        shutil.copyfile(source / "redis.rdb", temporary / "redis/dump.rdb")
        shutil.copytree(source / "storage", temporary / "storage")
        private_write(temporary / "maintenance", "maintenance\n")
        private_write(temporary / "redis/enable-aof", "enable after RDB load\n")
        archive = root / "recovery" / str(time.time_ns())
        archive.parent.mkdir(exist_ok=True, mode=0o700)
        os.replace(root / "data", archive)
        os.replace(temporary, root / "data")
        # Load the RDB alone. resume() enables AOF and waits for the new base
        # before admitting work; an old AOF must never override the restored RDB.
        private_write(configuration, re.sub(r"(?m)^appendonly (yes|no)$", "appendonly no", text))
        print("Restored data; previous state preserved at", archive)
    finally:
        if temporary.exists():
            shutil.rmtree(temporary)


def switch_link(root, target):
    temporary = root / ".current-next"
    require(not temporary.exists() and not temporary.is_symlink(), "A previous release switch is unfinished")
    os.symlink(target, temporary)
    os.replace(temporary, root / "current")


def promote(root, revision):
    candidate = release(root, revision)
    previous = root / "current"
    require(previous.is_symlink(), "Current release must be the managed symlink")
    old = release(root, previous.resolve().name)
    require(json.loads((old / "release.json").read_text())["sdk_version"] ==
            json.loads((candidate / "release.json").read_text())["sdk_version"], "Releases must use the same SDK version")
    pause(root)
    switch_link(root, candidate)
    try:
        resume(root)
    except Exception:
        pause(root)
        switch_link(root, old)
        resume(root)
        raise ValueError("Candidate did not become ready; the previous release was restored")
    print("Promoted release", revision)


@contextmanager
def operation_lock(root):
    root.mkdir(parents=True, exist_ok=True, mode=0o700)
    with (root / "operation.lock").open("a") as lock:
        fcntl.flock(lock, fcntl.LOCK_EX)
        yield


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", required=True)
    commands = parser.add_subparsers(dest="command", required=True)
    builder = commands.add_parser("build")
    builder.add_argument("--sdk", required=True)
    builder.add_argument("--revision", required=True)
    renderer = commands.add_parser("configure")
    for name in ("revision", "env-file", "certificate", "private-key"):
        renderer.add_argument("--" + name, required=True)
    for name, default in (("http-port", 8080), ("https-port", 8443), ("web-a-port", 8001), ("web-b-port", 8002)):
        renderer.add_argument("--" + name, type=int, default=default)
    for name in ("status", "pause", "resume"):
        commands.add_parser(name)
    commands.add_parser("backup").add_argument("--output", required=True)
    commands.add_parser("verify-backup").add_argument("--input", required=True)
    commands.add_parser("restore").add_argument("--input", required=True)
    commands.add_parser("promote").add_argument("--revision", required=True)
    args = parser.parse_args()
    root = safe_path(args.root)
    with operation_lock(root):
        if args.command == "build": build(root, args)
        elif args.command == "configure": configure(root, args)
        elif args.command == "status": print(json.dumps(states(root), indent=2))
        elif args.command == "pause": pause(root)
        elif args.command == "resume": resume(root)
        elif args.command == "backup": backup(root, args.output)
        elif args.command == "verify-backup": verify_backup(root, args.input); print("Backup verified")
        elif args.command == "restore": restore(root, args.input)
        elif args.command == "promote": promote(root, args.revision)


if __name__ == "__main__":
    try:
        main()
    except (ValueError, OSError, subprocess.SubprocessError, sqlite3.Error) as error:
        raise SystemExit(str(error))
