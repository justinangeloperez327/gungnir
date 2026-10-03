# Getting Started

> **Status: Gungnir 0.9 preview.** The structured compiler profile is feature-frozen for the 0.9 line. Use a C++23 compiler and CMake 3.25 or newer.

## Install

### Windows

Download the Windows setup executable from GitHub Releases:

```text
gungnir-v<version>-windows-x86_64-setup.exe
```

Run the installer normally. Beginning with v0.1.2, the installer adds `<install directory>\bin` to the **current user's PATH** automatically. It does not modify the system PATH, so an already-long machine PATH does not block Gungnir from being registered.

After installation, close existing terminal windows and open a new PowerShell or Command Prompt:

```powershell
gungnir --version
gungnirc --version
gungnirc --print-contract
```

The CLI discovers the installed framework automatically. You do not need to set `GUNGNIR_CMAKE_PREFIX` for a normal installer or portable-package layout.

Uninstalling Gungnir removes its own user-PATH entry without rewriting unrelated PATH entries.

### Portable packages

Windows ZIP and Linux tarball packages remain available. The current Linux asset is `gungnir-v0.9.0-linux-x86_64.tar.gz`. Extract the package and add its `bin` directory to `PATH`.

For custom layouts, `GUNGNIR_CMAKE_PREFIX` can explicitly point the CLI at the Gungnir installation prefix.

### Build from source

From the Gungnir repository:

```sh
cmake -S . -B build -DGUNGNIR_BUILD_TOOLS=ON -DGUNGNIR_BUILD_TESTS=OFF -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX="$PWD/install"
cmake --build build --config Release
cmake --install build --config Release
export PATH="$PWD/install/bin:$PATH"
```

Optional database and transport adapters require their corresponding CMake options and native dependencies.

## Create an application

```sh
gungnir new hello
cd hello
```

The generator creates a structured project, environment configuration, application and route directories, a welcome view, and an editable `bootstrap/app.hpp`.

Replace `app/controllers/HomeController.gnr` with:

```gnr
controller HomeController {
    Response index() {
        return text("Hello from Gungnir");
    }
}
```

Keep `routes/web.gnr` as:

```gnr
Route::get("/", HomeController::index);
```

## Validate and run

```sh
gungnirc app/controllers/HomeController.gnr --check
gungnir build
gungnir run
```

The generated environment uses `APP_HOST=127.0.0.1` and `APP_PORT=8000`. Open `http://127.0.0.1:8000/`; the expected response is `Hello from Gungnir`.

`--check` runs the structured parser, semantic/type validation, control-flow analysis, and framework validation, then stops at `ValidatedProject`. It does not lower to C++ IR or compile native C++. `gungnir build` performs the backend/native compilation step. Both checks matter. `gungnir dev` watches sources, configuration, bootstrap, and views, rebuilds after changes, and restarts the application on a successful build. See [CLI details](cli-codegen.md).

## Inspect generated C++

```sh
gungnirc app/controllers/HomeController.gnr -o .gungnir/home_controller.cpp
```

Structured output includes the runtime headers and can be compiled with the installed Gungnir include directory and `-std=c++23` (or the equivalent compiler option). Use `gungnir build` to link the framework and supply the project bootstrap. Imported modules must be checked together through the project build or `gungnirc ROOT --project`.

## Canonical example

The copyable minimal application used by the documentation contract lives under [examples/hello](../examples/hello/README.md). Its controller is validated by the current structured compiler in documentation CI.

## Next steps

- [Controllers](controller.md) and [routing](routing.md) describe the current HTTP contract.
- [Requests](request.md) and [validation](validation.md) describe string-based input and supported rules.
- [Models](model.md) and [migrations](migration.md) explain current field-based model generation and schema planning.
- [Design specifications](design/README.md) preserve the intended simpler language. Implement their compiler/runtime support before copying target-only syntax into an application.

## Verification scope

The project integration test installs the framework, generates and compiles an application, checks HTTP startup, and exercises incremental rebuilds and development restarts. It does not replace external-service or production load tests.
