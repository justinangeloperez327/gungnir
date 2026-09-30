# Getting Started

> **Status: experimental.** Use a C++23 compiler, CMake 3.25 or newer, and a pinned Gungnir revision. The first example uses supported typed controller syntax and no database.

## Build and install

From the Gungnir repository:

```sh
cmake -S . -B build -DGUNGNIR_BUILD_TOOLS=ON -DGUNGNIR_BUILD_TESTS=OFF -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX="$PWD/install"
cmake --build build --config Release
cmake --install build --config Release
export PATH="$PWD/install/bin:$PATH"
export GUNGNIR_CMAKE_PREFIX="$PWD/install"
```

These commands are for a POSIX shell. On Windows, set PATH and GUNGNIR_CMAKE_PREFIX to the corresponding installed directories, and use your C++23 toolchain's CMake generator. Optional database/transport adapters require their own build flags and dependencies.

## Create an application

```sh
gungnir new hello
cd hello
```

The generator creates `.gungnir-project`, environment configuration, application and route directories, and a welcome view. Some generated declarations still use legacy C++ class syntax; the current frontend also accepts first-class controller declarations.

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
gungnirc app/controllers/home_controller.gnr --check
gungnir build
gungnir run
```

The generated environment uses `APP_HOST=127.0.0.1` and `APP_PORT=8000`. Open `http://127.0.0.1:8000/`; the expected response is `Hello from Gungnir`.

`--check` runs frontend/lowering diagnostics for one file. `gungnir build` also compiles generated native code against the installed framework. Both checks matter. `gungnir dev` runs in development mode; it does not provide a file-watch/hot-reload guarantee.

## Inspect generated C++

```sh
gungnirc app/controllers/home_controller.gnr -o .gungnir/home_controller.cpp
```

Standalone generated code needs framework includes and aliases supplied by a native translation unit. Project builds supply this bootstrap. A compile-only wrapper for inspecting the controller is:

```cpp
#include <gungnir/controller/controller.hpp>
using gungnir::Response;
#include "home_controller.cpp"
```

Compile that wrapper with the Gungnir include directory and `-std=c++23` (or the equivalent compiler option). A compile-only check validates the real interfaces; it does not link or run the HTTP server.

## Next steps

- [Controllers](controller.md) and [routing](routing.md) describe the current HTTP contract.
- [Requests](request.md) and [validation](validation.md) describe string-based input and supported rules.
- [Models](model.md) and [migrations](migration.md) explain current field-based model generation and schema planning.
- [Design specifications](design/README.md) preserve the intended simpler language. Implement their compiler/runtime support before copying target-only syntax into an application.

## Verification scope

The controller above is checked through the current transpiler and a native compile-only wrapper. The build/install/run commands describe the project CLI path; they require a full checkout and a configured CMake toolchain. They do not constitute a live database, TLS or production load test.
