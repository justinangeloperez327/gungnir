# Getting Started

> **Status: experimental.** Use a C++23 compiler, CMake 3.25 or newer, and a pinned Gungnir revision. The first example uses supported typed controller syntax and no database.

## Install

### Windows

Beginning with v0.1.1, download the Windows setup executable from GitHub Releases:

```text
gungnir-v0.1.1-windows-x86_64-setup.exe
```

Run the installer and enable the option to add Gungnir to `PATH`. The installer places the framework, compiler, CLI, headers, libraries, and CMake package files under the selected installation directory and registers an uninstaller.

Verify:

```powershell
gungnir --version
```

The CLI discovers the installed framework automatically. You do not need to set `GUNGNIR_CMAKE_PREFIX` for a normal installer or portable-package layout.

### Portable packages

Windows ZIP and Linux tarball packages remain available. Extract the package and add its `bin` directory to `PATH`.

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

Replace `app/controllers/home_controller.gnr` with:

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
gungnirc app/controllers/home_controller.gnr --strict --check
gungnir build
gungnir run
```

The generated environment uses `APP_HOST=127.0.0.1` and `APP_PORT=8000`. Open `http://127.0.0.1:8000/`; the expected response is `Hello from Gungnir`.

`--check` runs frontend/lowering diagnostics for one file. `gungnir build` also compiles generated native code against the installed framework. Both checks matter. `gungnir dev` watches sources, configuration, bootstrap, and views, rebuilds after changes, and restarts the application on a successful build. See [CLI details](cli-codegen.md).

## Inspect generated C++

```sh
gungnirc app/controllers/home_controller.gnr --strict -o .gungnir/home_controller.cpp
```

Structured output includes the runtime headers and can be compiled with the installed Gungnir include directory and `-std=c++23` (or the equivalent compiler option). Use `gungnir build` to link the framework and supply the project bootstrap. Imported modules must be checked together through the project build or `gungnirc ROOT --project`.

## Next steps

- [Controllers](controller.md) and [routing](routing.md) describe the current HTTP contract.
- [Requests](request.md) and [validation](validation.md) describe string-based input and supported rules.
- [Models](model.md) and [migrations](migration.md) explain current field-based model generation and schema planning.
- [Design specifications](design/README.md) preserve the intended simpler language. Implement their compiler/runtime support before copying target-only syntax into an application.

## Verification scope

The project integration test installs the framework, generates and compiles an application, checks HTTP startup, and exercises incremental rebuilds and development restarts. It does not replace external-service or production load tests.
