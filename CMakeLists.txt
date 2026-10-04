cmake_minimum_required(VERSION 3.25)

# CMake requires a numeric project version. During active framework
# development this is an internal placeholder, not a public release number.
project(gungnir VERSION 0.0.0 LANGUAGES CXX)

set(GUNGNIR_RELEASE_CHANNEL "development")
set(GUNGNIR_VERSION_FULL "development")

set(
    GUNGNIR_GENERATED_INCLUDE_DIR
    "${CMAKE_CURRENT_BINARY_DIR}/generated/include"
)

file(
    MAKE_DIRECTORY
    "${GUNGNIR_GENERATED_INCLUDE_DIR}/gungnir"
)

configure_file(
    cmake/version.hpp.in
    "${GUNGNIR_GENERATED_INCLUDE_DIR}/gungnir/version.hpp"
    @ONLY
)

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

list(
    APPEND
    CMAKE_MODULE_PATH
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake"
)

option(GUNGNIR_WITH_S3 "Build the libcurl S3-compatible object storage adapter" OFF)
option(GUNGNIR_WITH_SQLITE "Build and register the SQLite adapter" OFF)
option(GUNGNIR_WITH_PASSWORD "Build OpenSSL scrypt password hashing" OFF)

option(GUNGNIR_BUILD_TESTS "Build Gungnir tests" ON)
option(GUNGNIR_BUILD_TOOLS "Build Gungnir command-line tools" ON)
option(
    GUNGNIR_BUILD_BENCHMARKS
    "Build reproducible Gungnir performance benchmarks"
    OFF
)
option(
    GUNGNIR_BUILD_FUZZERS
    "Build Clang/libFuzzer compiler robustness targets"
    OFF
)
option(GUNGNIR_WITH_POSTGRESQL "Build the libpq PostgreSQL adapter" OFF)
option(
    GUNGNIR_POSTGRESQL_INTEGRATION_TESTS
    "Build live PostgreSQL integration tests"
    OFF
)
option(
    GUNGNIR_WITH_MYSQL
    "Build the MySQL/MariaDB C client adapter"
    OFF
)
option(
    GUNGNIR_MYSQL_INTEGRATION_TESTS
    "Build live MySQL integration tests"
    OFF
)
option(
    GUNGNIR_WITH_SQLSERVER
    "Build the ODBC SQL Server adapter"
    OFF
)
option(
    GUNGNIR_SQLSERVER_INTEGRATION_TESTS
    "Build live SQL Server integration tests"
    OFF
)
option(
    GUNGNIR_WITH_MONGODB
    "Build the libmongoc MongoDB adapter"
    OFF
)
option(
    GUNGNIR_MONGODB_INTEGRATION_TESTS
    "Build live MongoDB integration tests"
    OFF
)

option(
    GUNGNIR_WITH_REDIS
    "Build the hiredis Redis cache, queue, and session adapters"
    OFF
)
option(
    GUNGNIR_REDIS_INTEGRATION_TESTS
    "Build live Redis integration tests"
    OFF
)

option(
    GUNGNIR_WITH_SMTP
    "Build the libcurl SMTP mail adapter"
    OFF
)
option(
    GUNGNIR_SMTP_INTEGRATION_TESTS
    "Build live SMTP integration tests"
    OFF
)

option(
    GUNGNIR_WITH_OTLP
    "Build the libcurl OTLP/HTTP observability exporter"
    OFF
)

option(
    GUNGNIR_WITH_TLS
    "Build OpenSSL-backed HTTPS server transport support"
    OFF
)

option(
    GUNGNIR_WITH_HTTP2
    "Build nghttp2-backed HTTP/2 server transport support"
    OFF
)
option(
    GUNGNIR_OTLP_INTEGRATION_TESTS
    "Build live OTLP HTTP integration tests"
    OFF
)

if(
    GUNGNIR_POSTGRESQL_INTEGRATION_TESTS AND
    NOT GUNGNIR_WITH_POSTGRESQL
)
    message(
        FATAL_ERROR
        "PostgreSQL integration tests require GUNGNIR_WITH_POSTGRESQL=ON"
    )
endif()

if(
    GUNGNIR_MYSQL_INTEGRATION_TESTS AND
    NOT GUNGNIR_WITH_MYSQL
)
    message(
        FATAL_ERROR
        "MySQL integration tests require GUNGNIR_WITH_MYSQL=ON"
    )
endif()

if(
    GUNGNIR_SQLSERVER_INTEGRATION_TESTS AND
    NOT GUNGNIR_WITH_SQLSERVER
)
    message(
        FATAL_ERROR
        "SQL Server integration tests require GUNGNIR_WITH_SQLSERVER=ON"
    )
endif()

if(
    GUNGNIR_MONGODB_INTEGRATION_TESTS AND
    NOT GUNGNIR_WITH_MONGODB
)
    message(
        FATAL_ERROR
        "MongoDB integration tests require GUNGNIR_WITH_MONGODB=ON"
    )
endif()

if(
    GUNGNIR_REDIS_INTEGRATION_TESTS AND
    NOT GUNGNIR_WITH_REDIS
)
    message(
        FATAL_ERROR
        "Redis integration tests require GUNGNIR_WITH_REDIS=ON"
    )
endif()

if(
    GUNGNIR_SMTP_INTEGRATION_TESTS AND
    NOT GUNGNIR_WITH_SMTP
)
    message(
        FATAL_ERROR
        "SMTP integration tests require GUNGNIR_WITH_SMTP=ON"
    )
endif()

if(
    GUNGNIR_OTLP_INTEGRATION_TESTS AND
    NOT GUNGNIR_WITH_OTLP
)
    message(
        FATAL_ERROR
        "OTLP integration tests require GUNGNIR_WITH_OTLP=ON"
    )
endif()

if(
    GUNGNIR_WITH_HTTP2 AND
    NOT GUNGNIR_WITH_TLS
)
    message(
        FATAL_ERROR
        "HTTP/2 transport currently requires GUNGNIR_WITH_TLS=ON"
    )
endif()

find_package(Threads REQUIRED)

if(GUNGNIR_WITH_POSTGRESQL)
    find_package(PostgreSQL REQUIRED)
endif()

if(GUNGNIR_WITH_MYSQL)
    find_package(MariaDB REQUIRED)
endif()

if(GUNGNIR_WITH_SQLSERVER)
    find_package(ODBC REQUIRED)
endif()

if(GUNGNIR_WITH_MONGODB)
    find_package(mongoc 2.5.4 CONFIG REQUIRED)
endif()

if(GUNGNIR_WITH_REDIS)
    find_package(Hiredis REQUIRED)
endif()

if(
    GUNGNIR_WITH_SMTP OR
    GUNGNIR_WITH_OTLP OR
    (
        GUNGNIR_WITH_HTTP2 AND
        GUNGNIR_BUILD_TESTS
    )
)
    find_package(CURL 7.56 REQUIRED)
endif()

if(GUNGNIR_WITH_TLS OR GUNGNIR_WITH_PASSWORD)
    find_package(OpenSSL 3.0 REQUIRED)
endif()

if(GUNGNIR_WITH_HTTP2)
    find_package(Nghttp2 REQUIRED)
endif()

add_library(gungnir
    src/auth/session.cpp
    src/config/environment.cpp
    src/config/repository.cpp
    src/core/application.cpp
    src/core/container.cpp
    src/core/executor.cpp
    src/core/timer.cpp
    src/controller/controller.cpp
    src/database/compiler.cpp
    src/database/connection.cpp
    src/database/driver.cpp
    src/database/manager.cpp
    src/database/pool.cpp
    src/database/registry.cpp
    src/database/runtime.cpp
    src/database/settings.cpp
    src/database/transaction.cpp
    src/http/exception_handler.cpp
    src/http/json.cpp
    src/http/message.cpp
    src/http/request.cpp
    src/http/cookie.cpp
    src/http/response.cpp
    src/http/server.cpp
    src/http/security.cpp
    src/http/websocket.cpp
    src/migration/column.cpp
    src/migration/context.cpp
    src/migration/definition.cpp
    src/migration/migration.cpp
    src/migration/runner.cpp
    src/migration/table.cpp
    src/production/runtime_adapters.cpp
    src/production/runtime_host.cpp
    src/production/signal_watcher.cpp
    src/routing/route.cpp
    src/routing/router.cpp
    src/security/security.cpp
    src/scheduler/iana_timezone.cpp
    src/session/middleware.cpp
    src/storage/local_disk.cpp
    src/validation/validator.cpp
    src/view/engine.cpp
    src/view/runtime.cpp
    src/view/value.cpp
)

add_library(gungnir::gungnir ALIAS gungnir)
if(GUNGNIR_WITH_S3)
    find_package(CURL 7.75 REQUIRED)
    target_sources(gungnir PRIVATE src/storage/s3_disk.cpp)
    target_link_libraries(gungnir PUBLIC CURL::libcurl)
    target_compile_definitions(gungnir PUBLIC GUNGNIR_WITH_S3=1)
endif()
if(GUNGNIR_WITH_SQLITE)
    find_package(SQLite3 REQUIRED)
    target_sources(gungnir PRIVATE src/database/sqlite.cpp)
    target_link_libraries(gungnir PUBLIC SQLite::SQLite3)
    target_compile_definitions(gungnir PUBLIC GUNGNIR_WITH_SQLITE=1)
endif()
if(GUNGNIR_WITH_PASSWORD)
    target_sources(gungnir PRIVATE src/auth/password.cpp)
    target_link_libraries(gungnir PUBLIC OpenSSL::Crypto)
    target_compile_definitions(gungnir PUBLIC GUNGNIR_WITH_PASSWORD=1)
endif()

add_library(gungnir-orm
    src/orm/compiler.cpp
    src/orm/mutation.cpp
    src/orm/advanced_mutation.cpp
)

add_library(gungnir::orm ALIAS gungnir-orm)

add_library(gungnir-language
    src/language/async_lowering.cpp
    src/language/bootstrap_lowering.cpp
    src/language/controller_lowering.cpp
    src/language/compiler.cpp
    src/language/cpp_ir.cpp
    src/language/cpp_ir_verifier.cpp
    src/language/emitter.cpp
    src/language/syntax.cpp
    src/language/validated.cpp
    src/language/lexer.cpp
    src/language/lsp.cpp
    src/language/middleware_lowering.cpp
    src/language/migration_lowering.cpp
    src/language/model_lowering.cpp
    src/language/parser.cpp
    src/language/semantic.cpp
    src/language/transpiler.cpp
    src/language/validation_lowering.cpp
    src/language/view_lowering.cpp
)

add_library(gungnir::language ALIAS gungnir-language)

target_compile_features(gungnir-language PUBLIC cxx_std_23)
target_include_directories(gungnir-language
    PUBLIC
        $<BUILD_INTERFACE:${GUNGNIR_GENERATED_INCLUDE_DIR}>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

set_target_properties(gungnir-language PROPERTIES
    CXX_EXTENSIONS OFF
    POSITION_INDEPENDENT_CODE ON
    EXPORT_NAME language
    VERSION ${PROJECT_VERSION}
    SOVERSION ${PROJECT_VERSION_MAJOR}
)

if(GUNGNIR_BUILD_FUZZERS)
    if(NOT CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        message(
            FATAL_ERROR
            "GUNGNIR_BUILD_FUZZERS requires Clang/libFuzzer"
        )
    endif()

    target_compile_options(
        gungnir-language
        PRIVATE
            -fsanitize=address,undefined,fuzzer-no-link
            -fno-omit-frame-pointer
    )

    foreach(fuzzer IN ITEMS lexer compiler)
        add_executable(
            gungnir_${fuzzer}_fuzzer
            tests/fuzz/${fuzzer}_fuzz.cpp
        )
        target_link_libraries(
            gungnir_${fuzzer}_fuzzer
            PRIVATE
                gungnir::language
        )
        target_compile_features(
            gungnir_${fuzzer}_fuzzer
            PRIVATE
                cxx_std_23
        )
        target_compile_options(
            gungnir_${fuzzer}_fuzzer
            PRIVATE
                -fsanitize=address,undefined,fuzzer-no-link
                -fno-omit-frame-pointer
        )
        target_link_options(
            gungnir_${fuzzer}_fuzzer
            PRIVATE
                -fsanitize=fuzzer,address,undefined
        )
    endforeach()
endif()

add_library(
    gungnir-cli-support
    STATIC
        src/cli/project.cpp
        src/cli/structured_project.cpp
        src/cli/lsp.cpp
        src/cli/dev.cpp
)

target_link_libraries(
    gungnir-cli-support
    PUBLIC
        gungnir::language
    PRIVATE
        gungnir::gungnir
)

target_compile_features(
    gungnir-cli-support
    PUBLIC
        cxx_std_23
)

target_include_directories(
    gungnir-cli-support
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)

set_target_properties(
    gungnir-cli-support
    PROPERTIES
        CXX_EXTENSIONS OFF
        POSITION_INDEPENDENT_CODE ON
)

target_compile_features(gungnir-orm PUBLIC cxx_std_23)
target_link_libraries(gungnir-orm PUBLIC gungnir::gungnir)
target_include_directories(gungnir-orm
    PUBLIC
        $<BUILD_INTERFACE:${GUNGNIR_GENERATED_INCLUDE_DIR}>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

set_target_properties(gungnir-orm PROPERTIES
    CXX_EXTENSIONS OFF
    POSITION_INDEPENDENT_CODE ON
    EXPORT_NAME orm
    VERSION ${PROJECT_VERSION}
    SOVERSION ${PROJECT_VERSION_MAJOR}
)

target_compile_features(gungnir PUBLIC cxx_std_23)
target_link_libraries(gungnir PRIVATE Threads::Threads)

if(GUNGNIR_WITH_TLS)
    target_compile_definitions(
        gungnir
        PRIVATE
            GUNGNIR_WITH_TLS=1
    )

    target_link_libraries(
        gungnir
        PRIVATE
            OpenSSL::SSL
            OpenSSL::Crypto
    )
endif()

if(GUNGNIR_WITH_HTTP2)
    target_sources(
        gungnir
        PRIVATE
            src/http/http2_session.cpp
    )

    target_compile_definitions(
        gungnir
        PRIVATE
            GUNGNIR_WITH_HTTP2=1
    )

    target_link_libraries(
        gungnir
        PRIVATE
            Nghttp2::Nghttp2
    )
endif()

if(WIN32)
    target_link_libraries(
        gungnir
        PRIVATE
            ws2_32
            bcrypt
    )
endif()

target_include_directories(gungnir
    PUBLIC
        $<BUILD_INTERFACE:${GUNGNIR_GENERATED_INCLUDE_DIR}>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

set_target_properties(gungnir PROPERTIES
    CXX_EXTENSIONS OFF
    POSITION_INDEPENDENT_CODE ON
    EXPORT_NAME gungnir
    VERSION ${PROJECT_VERSION}
    SOVERSION ${PROJECT_VERSION_MAJOR}
)

if(BUILD_SHARED_LIBS)
    foreach(
        target
        IN ITEMS
            gungnir
            gungnir-orm
            gungnir-language
    )
        target_compile_definitions(
            ${target}
            PUBLIC
                GUNGNIR_SHARED=1
            PRIVATE
                GUNGNIR_BUILDING_LIBRARY=1
        )

        if(WIN32)
            set_target_properties(
                ${target}
                PROPERTIES
                    WINDOWS_EXPORT_ALL_SYMBOLS ON
            )
        endif()
    endforeach()
endif()

if(GUNGNIR_WITH_POSTGRESQL)
    add_library(
        gungnir-postgresql
        src/database/postgresql.cpp
    )

    add_library(
        gungnir::postgresql
        ALIAS
        gungnir-postgresql
    )

    target_compile_features(
        gungnir-postgresql
        PUBLIC
            cxx_std_23
    )

    target_link_libraries(
        gungnir-postgresql
        PUBLIC
            gungnir::gungnir
            PostgreSQL::PostgreSQL
    )

    target_include_directories(
        gungnir-postgresql
        PUBLIC
            $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
            $<INSTALL_INTERFACE:include>
    )

    set_target_properties(
        gungnir-postgresql
        PROPERTIES
            CXX_EXTENSIONS OFF
            POSITION_INDEPENDENT_CODE ON
            EXPORT_NAME postgresql
    )
endif()

if(GUNGNIR_WITH_MYSQL)
    add_library(
        gungnir-mysql
        src/database/mysql.cpp
    )

    add_library(
        gungnir::mysql
        ALIAS
        gungnir-mysql
    )

    target_compile_features(
        gungnir-mysql
        PUBLIC
            cxx_std_23
    )

    target_link_libraries(
        gungnir-mysql
        PUBLIC
            gungnir::gungnir
            MariaDB::MariaDB
    )

    target_include_directories(
        gungnir-mysql
        PUBLIC
            $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
            $<INSTALL_INTERFACE:include>
    )

    set_target_properties(
        gungnir-mysql
        PROPERTIES
            CXX_EXTENSIONS OFF
            POSITION_INDEPENDENT_CODE ON
            EXPORT_NAME mysql
    )
endif()

if(GUNGNIR_WITH_SQLSERVER)
    add_library(
        gungnir-sqlserver
        src/database/sqlserver.cpp
    )

    add_library(
        gungnir::sqlserver
        ALIAS
        gungnir-sqlserver
    )

    target_compile_features(
        gungnir-sqlserver
        PUBLIC
            cxx_std_23
    )

    target_link_libraries(
        gungnir-sqlserver
        PUBLIC
            gungnir::gungnir
            ODBC::ODBC
    )

    target_include_directories(
        gungnir-sqlserver
        PUBLIC
            $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
            $<INSTALL_INTERFACE:include>
    )

    set_target_properties(
        gungnir-sqlserver
        PROPERTIES
            CXX_EXTENSIONS OFF
            POSITION_INDEPENDENT_CODE ON
            EXPORT_NAME sqlserver
    )
endif()

if(GUNGNIR_WITH_MONGODB)
    add_library(
        gungnir-mongodb
        src/database/mongodb.cpp
    )

    add_library(
        gungnir::mongodb
        ALIAS
        gungnir-mongodb
    )

    target_compile_features(
        gungnir-mongodb
        PUBLIC
            cxx_std_23
    )

    target_link_libraries(
        gungnir-mongodb
        PUBLIC
            gungnir::gungnir
            mongoc::mongoc
    )

    target_include_directories(
        gungnir-mongodb
        PUBLIC
            $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
            $<INSTALL_INTERFACE:include>
    )

    set_target_properties(
        gungnir-mongodb
        PROPERTIES
            CXX_EXTENSIONS OFF
            POSITION_INDEPENDENT_CODE ON
            EXPORT_NAME mongodb
    )
endif()

if(GUNGNIR_WITH_REDIS)
    add_library(
        gungnir-redis
        src/cache/redis_store.cpp
        src/queue/redis_driver.cpp
        src/redis/client.cpp
        src/scheduler/redis_lock.cpp
        src/session/redis_store.cpp
    )

    add_library(
        gungnir::redis
        ALIAS
        gungnir-redis
    )

    target_compile_features(
        gungnir-redis
        PUBLIC
            cxx_std_23
    )

    target_link_libraries(
        gungnir-redis
        PUBLIC
            gungnir::gungnir
            Hiredis::Hiredis
    )

    if(TARGET Hiredis::SSL)
        target_link_libraries(
            gungnir-redis
            PUBLIC
                Hiredis::SSL
        )

        target_compile_definitions(
            gungnir-redis
            PRIVATE
                GUNGNIR_HIREDIS_SSL=1
        )
    endif()

    target_include_directories(
        gungnir-redis
        PUBLIC
            $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
            $<INSTALL_INTERFACE:include>
    )

    set_target_properties(
        gungnir-redis
        PROPERTIES
            CXX_EXTENSIONS OFF
            POSITION_INDEPENDENT_CODE ON
            EXPORT_NAME redis
    )
endif()

if(GUNGNIR_WITH_SMTP)
    add_library(
        gungnir-smtp
        src/mail/smtp_transport.cpp
    )

    add_library(
        gungnir::smtp
        ALIAS
        gungnir-smtp
    )

    target_compile_features(
        gungnir-smtp
        PUBLIC
            cxx_std_23
    )

    target_link_libraries(
        gungnir-smtp
        PUBLIC
            gungnir::gungnir
            CURL::libcurl
    )

    target_include_directories(
        gungnir-smtp
        PUBLIC
            $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
            $<INSTALL_INTERFACE:include>
    )

    set_target_properties(
        gungnir-smtp
        PROPERTIES
            CXX_EXTENSIONS OFF
            POSITION_INDEPENDENT_CODE ON
            EXPORT_NAME smtp
    )
endif()

if(GUNGNIR_WITH_OTLP)
    add_library(
        gungnir-otlp
        src/observability/otlp_http_exporter.cpp
    )

    add_library(
        gungnir::otlp
        ALIAS
        gungnir-otlp
    )

    target_compile_features(
        gungnir-otlp
        PUBLIC
            cxx_std_23
    )

    target_link_libraries(
        gungnir-otlp
        PUBLIC
            gungnir::gungnir
            CURL::libcurl
    )

    target_include_directories(
        gungnir-otlp
        PUBLIC
            $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
            $<INSTALL_INTERFACE:include>
    )

    set_target_properties(
        gungnir-otlp
        PROPERTIES
            CXX_EXTENSIONS OFF
            POSITION_INDEPENDENT_CODE ON
            EXPORT_NAME otlp
    )
endif()

if(MSVC)
    target_compile_options(gungnir PRIVATE /W4 /permissive-)
    target_compile_options(gungnir-orm PRIVATE /W4 /permissive-)
    target_compile_options(gungnir-language PRIVATE /W4 /permissive-)
    target_compile_options(gungnir-cli-support PRIVATE /W4 /permissive-)

    if(GUNGNIR_WITH_POSTGRESQL)
        target_compile_options(
            gungnir-postgresql
            PRIVATE
                /W4
                /permissive-
        )
    endif()

    if(GUNGNIR_WITH_MYSQL)
        target_compile_options(
            gungnir-mysql
            PRIVATE
                /W4
                /permissive-
        )
    endif()

    if(GUNGNIR_WITH_SQLSERVER)
        target_compile_options(
            gungnir-sqlserver
            PRIVATE
                /W4
                /permissive-
        )
    endif()

    if(GUNGNIR_WITH_MONGODB)
        target_compile_options(
            gungnir-mongodb
            PRIVATE
                /W4
                /permissive-
        )
    endif()

    if(GUNGNIR_WITH_REDIS)
        target_compile_options(
            gungnir-redis
            PRIVATE
                /W4
                /permissive-
        )
    endif()

    if(GUNGNIR_WITH_SMTP)
        target_compile_options(
            gungnir-smtp
            PRIVATE
                /W4
                /permissive-
        )
    endif()

    if(GUNGNIR_WITH_OTLP)
        target_compile_options(
            gungnir-otlp
            PRIVATE
                /W4
                /permissive-
        )
    endif()
else()
    target_compile_options(gungnir PRIVATE -Wall -Wextra -Wpedantic -Wconversion -Wshadow)
    target_compile_options(gungnir-orm PRIVATE -Wall -Wextra -Wpedantic -Wconversion -Wshadow)
    target_compile_options(gungnir-language PRIVATE -Wall -Wextra -Wpedantic -Wconversion -Wshadow)
    target_compile_options(gungnir-cli-support PRIVATE -Wall -Wextra -Wpedantic -Wconversion -Wshadow)

    if(GUNGNIR_WITH_POSTGRESQL)
        target_compile_options(
            gungnir-postgresql
            PRIVATE
                -Wall
                -Wextra
                -Wpedantic
                -Wconversion
                -Wshadow
        )
    endif()

    if(GUNGNIR_WITH_MYSQL)
        target_compile_options(
            gungnir-mysql
            PRIVATE
                -Wall
                -Wextra
                -Wpedantic
                -Wconversion
                -Wshadow
        )
    endif()

    if(GUNGNIR_WITH_SQLSERVER)
        target_compile_options(
            gungnir-sqlserver
            PRIVATE
                -Wall
                -Wextra
                -Wpedantic
                -Wconversion
                -Wshadow
        )
    endif()

    if(GUNGNIR_WITH_MONGODB)
        target_compile_options(
            gungnir-mongodb
            PRIVATE
                -Wall
                -Wextra
                -Wpedantic
                -Wconversion
                -Wshadow
        )
    endif()

    if(GUNGNIR_WITH_REDIS)
        target_compile_options(
            gungnir-redis
            PRIVATE
                -Wall
                -Wextra
                -Wpedantic
                -Wconversion
                -Wshadow
        )
    endif()

    if(GUNGNIR_WITH_SMTP)
        target_compile_options(
            gungnir-smtp
            PRIVATE
                -Wall
                -Wextra
                -Wpedantic
                -Wconversion
                -Wshadow
        )
    endif()

    if(GUNGNIR_WITH_OTLP)
        target_compile_options(
            gungnir-otlp
            PRIVATE
                -Wall
                -Wextra
                -Wpedantic
                -Wconversion
                -Wshadow
        )
    endif()
endif()

if(GUNGNIR_BUILD_TOOLS)
    add_executable(gungnirc tools/gungnirc.cpp)
    target_link_libraries(gungnirc PRIVATE gungnir::language)
    target_compile_features(gungnirc PRIVATE cxx_std_23)
    target_compile_definitions(
        gungnirc
        PRIVATE
            GUNGNIR_VERSION="${GUNGNIR_VERSION_FULL}"
    )

    add_executable(gungnir-cli tools/gungnir.cpp)
    target_link_libraries(
        gungnir-cli
        PRIVATE
            gungnir-cli-support
    )
    target_compile_features(
        gungnir-cli
        PRIVATE
            cxx_std_23
    )
    target_compile_definitions(
        gungnir-cli
        PRIVATE
            GUNGNIR_VERSION="${GUNGNIR_VERSION_FULL}"
    )
    set_target_properties(
        gungnir-cli
        PROPERTIES
            OUTPUT_NAME gungnir
            PDB_NAME gungnir-cli
    )
endif()

if(GUNGNIR_BUILD_BENCHMARKS)
    add_executable(
        gungnir_benchmark_compiler
        benchmarks/compiler.cpp
    )
    target_link_libraries(
        gungnir_benchmark_compiler
        PRIVATE
            gungnir::language
    )
    target_compile_features(
        gungnir_benchmark_compiler
        PRIVATE
            cxx_std_23
    )
    target_include_directories(
        gungnir_benchmark_compiler
        PRIVATE
            benchmarks
    )

    add_executable(
        gungnir_benchmark_http_routing
        benchmarks/http_routing.cpp
    )
    target_link_libraries(
        gungnir_benchmark_http_routing
        PRIVATE
            gungnir::gungnir
    )
    target_compile_features(
        gungnir_benchmark_http_routing
        PRIVATE
            cxx_std_23
    )
    target_include_directories(
        gungnir_benchmark_http_routing
        PRIVATE
            benchmarks
    )

    add_executable(
        gungnir_benchmark_orm
        benchmarks/orm.cpp
    )
    target_link_libraries(
        gungnir_benchmark_orm
        PRIVATE
            gungnir::orm
    )
    target_compile_features(
        gungnir_benchmark_orm
        PRIVATE
            cxx_std_23
    )
    target_include_directories(
        gungnir_benchmark_orm
        PRIVATE
            benchmarks
    )

    add_custom_target(
        gungnir_benchmarks
        DEPENDS
            gungnir_benchmark_compiler
            gungnir_benchmark_http_routing
            gungnir_benchmark_orm
    )
endif()

if(GUNGNIR_BUILD_TESTS)
    enable_testing()

    add_executable(
        gungnir_public_api_contract_test
        tests/public_api_contract.cpp
    )
    target_link_libraries(
        gungnir_public_api_contract_test
        PRIVATE
            gungnir::gungnir
            gungnir::orm
            gungnir::language
    )
    target_compile_features(
        gungnir_public_api_contract_test
        PRIVATE
            cxx_std_23
    )
    add_test(
        NAME gungnir.public_api_contract
        COMMAND gungnir_public_api_contract_test
    )

    add_executable(gungnir_project_tooling_test tests/project_tooling.cpp)
    target_link_libraries(gungnir_project_tooling_test PRIVATE gungnir-cli-support)
    add_test(NAME gungnir.project_tooling COMMAND gungnir_project_tooling_test)

    # Compile important public entry points in isolation, so include order
    # cannot hide missing dependencies. This target is part of the PR gate.
    set(public_header_units)
    foreach(header IN ITEMS gungnir/gungnir.hpp gungnir/orm/orm.hpp
            gungnir/core/services.hpp gungnir/language/compiler.hpp
            gungnir/language/formatter.hpp gungnir/language/lsp.hpp
            gungnir/language/module.hpp gungnir/language/dependency_graph.hpp
            gungnir/cli/project.hpp gungnir/cli/lsp.hpp
            gungnir/routing/controller_binding.hpp)
        string(REPLACE "/" "_" unit "${header}")
        set(source "${CMAKE_CURRENT_BINARY_DIR}/public-headers/${unit}.cpp")
        file(GENERATE OUTPUT "${source}" CONTENT "#include <${header}>\n")
        list(APPEND public_header_units "${source}")
    endforeach()
    add_library(gungnir_public_headers OBJECT ${public_header_units})
    target_link_libraries(gungnir_public_headers PRIVATE gungnir::gungnir gungnir::orm gungnir::language)

    add_executable(gungnir_runtime_safety_test tests/runtime_safety.cpp)
    target_link_libraries(gungnir_runtime_safety_test PRIVATE gungnir::gungnir gungnir::orm)
    target_compile_features(gungnir_runtime_safety_test PRIVATE cxx_std_23)
    add_test(NAME gungnir.runtime_safety COMMAND gungnir_runtime_safety_test)

    add_executable(gungnir_frontend_diagnostics_test tests/frontend_diagnostics.cpp)
    target_link_libraries(gungnir_frontend_diagnostics_test PRIVATE gungnir::language)
    target_compile_features(gungnir_frontend_diagnostics_test PRIVATE cxx_std_23)
    add_test(NAME gungnir.frontend_diagnostics COMMAND gungnir_frontend_diagnostics_test)

    add_executable(gungnir_structured_runtime_test tests/structured_runtime.cpp)
    target_link_libraries(gungnir_structured_runtime_test PRIVATE gungnir::gungnir gungnir::orm)
    target_compile_features(gungnir_structured_runtime_test PRIVATE cxx_std_23)
    add_test(NAME gungnir.structured_runtime COMMAND gungnir_structured_runtime_test)

    add_executable(gungnir_structured_language_test tests/structured_language.cpp)
    target_link_libraries(gungnir_structured_language_test PRIVATE gungnir::language)
    target_compile_features(gungnir_structured_language_test PRIVATE cxx_std_23)
    add_test(NAME gungnir.structured_language COMMAND gungnir_structured_language_test)

    add_executable(
        gungnir_framework_semantics_test
        tests/framework_semantics.cpp
    )
    target_link_libraries(
        gungnir_framework_semantics_test
        PRIVATE
            gungnir::language
    )
    target_compile_features(
        gungnir_framework_semantics_test
        PRIVATE
            cxx_std_23
    )
    add_test(
        NAME gungnir.framework_semantics
        COMMAND gungnir_framework_semantics_test
    )

    add_executable(gungnir_compiler_correctness_test tests/compiler_correctness.cpp)
    target_link_libraries(gungnir_compiler_correctness_test PRIVATE gungnir::language)
    target_compile_features(gungnir_compiler_correctness_test PRIVATE cxx_std_23)
    add_test(NAME gungnir.compiler_correctness COMMAND gungnir_compiler_correctness_test)

    add_executable(gungnir_model_relationships_test tests/model_relationships.cpp)
    target_link_libraries(gungnir_model_relationships_test PRIVATE gungnir::language)
    target_compile_features(gungnir_model_relationships_test PRIVATE cxx_std_23)
    add_test(NAME gungnir.model_relationships COMMAND gungnir_model_relationships_test)

    add_executable(gungnir_compiler_stability_test tests/compiler_stability.cpp)
    target_link_libraries(gungnir_compiler_stability_test PRIVATE gungnir::language)
    target_compile_features(gungnir_compiler_stability_test PRIVATE cxx_std_23)
    add_test(NAME gungnir.compiler_stability COMMAND gungnir_compiler_stability_test)

    add_executable(gungnir_compiler_robustness_test tests/compiler_robustness.cpp)
    target_link_libraries(gungnir_compiler_robustness_test PRIVATE gungnir::language)
    target_compile_features(gungnir_compiler_robustness_test PRIVATE cxx_std_23)
    add_test(NAME gungnir.compiler_robustness COMMAND gungnir_compiler_robustness_test)
    set_tests_properties(gungnir.compiler_robustness PROPERTIES TIMEOUT 30)

    add_executable(gungnir_language_test tests/language.cpp)
    target_link_libraries(gungnir_language_test PRIVATE gungnir::language)
    target_compile_features(gungnir_language_test PRIVATE cxx_std_23)
    add_test(NAME gungnir.language COMMAND gungnir_language_test)

    if(GUNGNIR_BUILD_TOOLS)
        set(GUNGNIR_STRUCTURED_ORM "${CMAKE_CURRENT_BINARY_DIR}/generated/structured_orm_program.cpp")
        add_custom_command(OUTPUT "${GUNGNIR_STRUCTURED_ORM}"
            COMMAND $<TARGET_FILE:gungnirc> "${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/structured/orm.gnr" --strict --no-line-directives -o "${GUNGNIR_STRUCTURED_ORM}"
            DEPENDS gungnirc tests/fixtures/structured/orm.gnr VERBATIM)
        set_source_files_properties("${GUNGNIR_STRUCTURED_ORM}" PROPERTIES HEADER_FILE_ONLY TRUE)
        add_executable(gungnir_structured_orm_contract_test tests/structured_orm_contract.cpp "${GUNGNIR_STRUCTURED_ORM}")
        target_include_directories(gungnir_structured_orm_contract_test PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/generated")
        target_link_libraries(gungnir_structured_orm_contract_test PRIVATE gungnir::gungnir gungnir::orm)
        target_compile_features(gungnir_structured_orm_contract_test PRIVATE cxx_std_23)
        add_test(NAME gungnir.structured_orm_contract COMMAND gungnir_structured_orm_contract_test)
        if(GUNGNIR_WITH_SQLITE)
            add_executable(gungnir_structured_orm_test tests/structured_orm.cpp "${GUNGNIR_STRUCTURED_ORM}")
            target_include_directories(gungnir_structured_orm_test PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/generated")
            target_link_libraries(gungnir_structured_orm_test PRIVATE gungnir::gungnir gungnir::orm)
            target_compile_features(gungnir_structured_orm_test PRIVATE cxx_std_23)
            add_test(NAME gungnir.structured_orm COMMAND gungnir_structured_orm_test)
        endif()
        set(GUNGNIR_STRUCTURED_HTTP_PROGRAM "${CMAKE_CURRENT_BINARY_DIR}/generated/structured_http_program.cpp")
        add_custom_command(OUTPUT "${GUNGNIR_STRUCTURED_HTTP_PROGRAM}"
            COMMAND $<TARGET_FILE:gungnirc> "${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/structured/http.gnr" --strict --no-line-directives -o "${GUNGNIR_STRUCTURED_HTTP_PROGRAM}"
            DEPENDS gungnirc tests/fixtures/structured/http.gnr VERBATIM)
        add_executable(gungnir_structured_http_test tests/structured_http.cpp "${GUNGNIR_STRUCTURED_HTTP_PROGRAM}")
        set_source_files_properties("${GUNGNIR_STRUCTURED_HTTP_PROGRAM}" PROPERTIES HEADER_FILE_ONLY TRUE)
        target_include_directories(gungnir_structured_http_test PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/generated")
        target_link_libraries(gungnir_structured_http_test PRIVATE gungnir::gungnir gungnir::orm gungnir::language)
        target_compile_features(gungnir_structured_http_test PRIVATE cxx_std_23)
        add_test(NAME gungnir.structured_http COMMAND gungnir_structured_http_test)
        set(GUNGNIR_STRUCTURED_CONTEXT_PROGRAM "${CMAKE_CURRENT_BINARY_DIR}/generated/structured_context_program.cpp")
        add_custom_command(OUTPUT "${GUNGNIR_STRUCTURED_CONTEXT_PROGRAM}"
            COMMAND $<TARGET_FILE:gungnirc> "${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/structured/request_context.gnr" --strict --no-line-directives -o "${GUNGNIR_STRUCTURED_CONTEXT_PROGRAM}"
            DEPENDS gungnirc tests/fixtures/structured/request_context.gnr VERBATIM)
        add_executable(gungnir_structured_request_context_test tests/structured_request_context.cpp "${GUNGNIR_STRUCTURED_CONTEXT_PROGRAM}")
        set_source_files_properties("${GUNGNIR_STRUCTURED_CONTEXT_PROGRAM}" PROPERTIES HEADER_FILE_ONLY TRUE)
        target_include_directories(gungnir_structured_request_context_test PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/generated")
        target_link_libraries(gungnir_structured_request_context_test PRIVATE gungnir::gungnir gungnir::orm gungnir::language)
        target_compile_features(gungnir_structured_request_context_test PRIVATE cxx_std_23)
        add_test(NAME gungnir.structured_request_context COMMAND gungnir_structured_request_context_test)
        set(GUNGNIR_STRUCTURED_ROUTING_PROGRAM "${CMAKE_CURRENT_BINARY_DIR}/generated/structured_routing_program.cpp")
        add_custom_command(OUTPUT "${GUNGNIR_STRUCTURED_ROUTING_PROGRAM}"
            COMMAND $<TARGET_FILE:gungnirc> "${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/structured/routing.gnr" --strict --no-line-directives -o "${GUNGNIR_STRUCTURED_ROUTING_PROGRAM}"
            DEPENDS gungnirc tests/fixtures/structured/routing.gnr VERBATIM)
        add_executable(gungnir_structured_routing_test tests/structured_routing.cpp "${GUNGNIR_STRUCTURED_ROUTING_PROGRAM}")
        set_source_files_properties("${GUNGNIR_STRUCTURED_ROUTING_PROGRAM}" PROPERTIES HEADER_FILE_ONLY TRUE)
        target_include_directories(gungnir_structured_routing_test PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/generated")
        target_link_libraries(gungnir_structured_routing_test PRIVATE gungnir::gungnir gungnir::orm gungnir::language)
        target_compile_definitions(gungnir_structured_routing_test PRIVATE GUNGNIR_ROUTING_FIXTURE="${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/structured/routing.gnr")
        target_compile_features(gungnir_structured_routing_test PRIVATE cxx_std_23)
        add_test(NAME gungnir.structured_routing COMMAND gungnir_structured_routing_test)
        set(GUNGNIR_STRUCTURED_SERVICES_PROGRAM "${CMAKE_CURRENT_BINARY_DIR}/generated/structured_services_program.cpp")
        add_custom_command(OUTPUT "${GUNGNIR_STRUCTURED_SERVICES_PROGRAM}"
            COMMAND $<TARGET_FILE:gungnirc> "${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/structured/services.gnr" --strict --no-line-directives -o "${GUNGNIR_STRUCTURED_SERVICES_PROGRAM}"
            DEPENDS gungnirc tests/fixtures/structured/services.gnr VERBATIM)
        add_executable(gungnir_structured_services_test tests/structured_services.cpp "${GUNGNIR_STRUCTURED_SERVICES_PROGRAM}")
        set_source_files_properties("${GUNGNIR_STRUCTURED_SERVICES_PROGRAM}" PROPERTIES HEADER_FILE_ONLY TRUE)
        target_include_directories(gungnir_structured_services_test PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/generated")
        target_link_libraries(gungnir_structured_services_test PRIVATE gungnir::gungnir gungnir::orm gungnir::language)
        target_compile_features(gungnir_structured_services_test PRIVATE cxx_std_23)
        add_test(NAME gungnir.structured_services COMMAND gungnir_structured_services_test)
        set(GUNGNIR_STRUCTURED_AUTH_PROGRAM "${CMAKE_CURRENT_BINARY_DIR}/generated/structured_authentication_program.cpp")
        add_custom_command(OUTPUT "${GUNGNIR_STRUCTURED_AUTH_PROGRAM}"
            COMMAND $<TARGET_FILE:gungnirc> "${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/structured/authentication.gnr" --strict --no-line-directives -o "${GUNGNIR_STRUCTURED_AUTH_PROGRAM}"
            DEPENDS gungnirc tests/fixtures/structured/authentication.gnr VERBATIM)
        add_executable(gungnir_structured_authentication_test tests/structured_authentication.cpp "${GUNGNIR_STRUCTURED_AUTH_PROGRAM}")
        set_source_files_properties("${GUNGNIR_STRUCTURED_AUTH_PROGRAM}" PROPERTIES HEADER_FILE_ONLY TRUE)
        target_include_directories(gungnir_structured_authentication_test PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/generated")
        target_link_libraries(gungnir_structured_authentication_test PRIVATE gungnir::gungnir gungnir::orm gungnir::language)
        target_compile_features(gungnir_structured_authentication_test PRIVATE cxx_std_23)
        add_test(NAME gungnir.structured_authentication COMMAND gungnir_structured_authentication_test)
        set(GUNGNIR_STRUCTURED_PROGRAM "${CMAKE_CURRENT_BINARY_DIR}/generated/structured_program.cpp")
        add_custom_command(OUTPUT "${GUNGNIR_STRUCTURED_PROGRAM}"
            COMMAND $<TARGET_FILE:gungnirc> "${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/structured/program.gnr" --strict --no-line-directives -o "${GUNGNIR_STRUCTURED_PROGRAM}"
            DEPENDS gungnirc tests/fixtures/structured/program.gnr VERBATIM)
        set(GUNGNIR_STRUCTURED_MODULES "${CMAKE_CURRENT_BINARY_DIR}/generated/structured_modules.cpp")
        add_custom_command(OUTPUT "${GUNGNIR_STRUCTURED_MODULES}"
            COMMAND $<TARGET_FILE:gungnirc> "${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/structured/modules" --project --no-line-directives -o "${GUNGNIR_STRUCTURED_MODULES}"
            DEPENDS gungnirc tests/fixtures/structured/modules/main.gnr tests/fixtures/structured/modules/math.gnr VERBATIM)
        add_executable(gungnir_structured_generated_test tests/structured_generated.cpp "${GUNGNIR_STRUCTURED_PROGRAM}" "${GUNGNIR_STRUCTURED_MODULES}")
        set_source_files_properties("${GUNGNIR_STRUCTURED_MODULES}" PROPERTIES HEADER_FILE_ONLY TRUE)
        set_source_files_properties("${GUNGNIR_STRUCTURED_PROGRAM}" PROPERTIES HEADER_FILE_ONLY TRUE)
        target_include_directories(gungnir_structured_generated_test PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/generated")
        target_link_libraries(gungnir_structured_generated_test PRIVATE gungnir::gungnir gungnir::orm)
        target_compile_features(gungnir_structured_generated_test PRIVATE cxx_std_23)
        add_test(NAME gungnir.structured_generated COMMAND gungnir_structured_generated_test)
        set(
            GUNGNIR_GENERATED_ASYNC_TEST
            "${CMAKE_CURRENT_BINARY_DIR}/generated/async_controller.cpp"
        )

        add_custom_command(
            OUTPUT "${GUNGNIR_GENERATED_ASYNC_TEST}"
            COMMAND
                ${CMAKE_COMMAND} -E make_directory
                "${CMAKE_CURRENT_BINARY_DIR}/generated"
            COMMAND
                $<TARGET_FILE:gungnirc>
                "${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/async_controller.gnr"
                --compat
                -o
                "${GUNGNIR_GENERATED_ASYNC_TEST}"
            DEPENDS
                gungnirc
                tests/fixtures/async_controller.gnr
            VERBATIM
        )

        add_executable(
            gungnir_language_async_generated_test
            "${GUNGNIR_GENERATED_ASYNC_TEST}"
        )
        target_link_libraries(
            gungnir_language_async_generated_test
            PRIVATE
                gungnir::gungnir
        )
        target_compile_features(
            gungnir_language_async_generated_test
            PRIVATE
                cxx_std_23
        )
        add_test(
            NAME gungnir.language_async_generated
            COMMAND gungnir_language_async_generated_test
        )

        set(
            GUNGNIR_GENERATED_VIEW_TEST
            "${CMAKE_CURRENT_BINARY_DIR}/generated/view_controller.cpp"
        )

        add_custom_command(
            OUTPUT "${GUNGNIR_GENERATED_VIEW_TEST}"
            COMMAND
                ${CMAKE_COMMAND} -E make_directory
                "${CMAKE_CURRENT_BINARY_DIR}/generated"
            COMMAND
                $<TARGET_FILE:gungnirc>
                "${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/view_controller.gnr"
                --compat
                -o
                "${GUNGNIR_GENERATED_VIEW_TEST}"
            DEPENDS
                gungnirc
                tests/fixtures/view_controller.gnr
            VERBATIM
        )

        add_executable(
            gungnir_language_view_generated_test
            "${GUNGNIR_GENERATED_VIEW_TEST}"
        )
        target_link_libraries(
            gungnir_language_view_generated_test
            PRIVATE
                gungnir::gungnir
        )
        target_compile_features(
            gungnir_language_view_generated_test
            PRIVATE
                cxx_std_23
        )
        add_test(
            NAME gungnir.language_view_generated
            COMMAND
                gungnir_language_view_generated_test
                "${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/views"
        )

        set(
            GUNGNIR_GENERATED_COLLECTIONS_TEST
            "${CMAKE_CURRENT_BINARY_DIR}/generated/collections_controller.cpp"
        )
        add_custom_command(
            OUTPUT "${GUNGNIR_GENERATED_COLLECTIONS_TEST}"
            COMMAND
                ${CMAKE_COMMAND} -E make_directory
                "${CMAKE_CURRENT_BINARY_DIR}/generated"
            COMMAND
                $<TARGET_FILE:gungnirc>
                "${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/collections_controller.gnr"
                --compat
                -o
                "${GUNGNIR_GENERATED_COLLECTIONS_TEST}"
            DEPENDS
                gungnirc
                tests/fixtures/collections_controller.gnr
            VERBATIM
        )
        add_executable(
            gungnir_language_collections_generated_test
            "${GUNGNIR_GENERATED_COLLECTIONS_TEST}"
        )
        target_link_libraries(
            gungnir_language_collections_generated_test
            PRIVATE gungnir::gungnir
        )
        target_compile_features(
            gungnir_language_collections_generated_test PRIVATE cxx_std_23
        )
        add_test(
            NAME gungnir.language_collections_generated
            COMMAND gungnir_language_collections_generated_test
        )
    endif()

    add_executable(gungnir_cli_test tests/cli.cpp)
    target_link_libraries(
        gungnir_cli_test
        PRIVATE
            gungnir-cli-support
    )
    target_compile_features(
        gungnir_cli_test
        PRIVATE
            cxx_std_23
    )
    add_test(
        NAME gungnir.cli
        COMMAND gungnir_cli_test
    )

    if(GUNGNIR_BUILD_TOOLS)
        find_package(Python3 COMPONENTS Interpreter REQUIRED)
        add_test(NAME gungnir.docs_orm_examples COMMAND ${Python3_EXECUTABLE}
            ${CMAKE_CURRENT_SOURCE_DIR}/tests/docs_orm_examples.py $<TARGET_FILE:gungnirc>)
        add_test(NAME gungnir.docs_http_examples COMMAND ${Python3_EXECUTABLE}
            ${CMAKE_CURRENT_SOURCE_DIR}/tests/docs_http_examples.py $<TARGET_FILE:gungnirc>)
        add_test(NAME gungnir.docs_service_examples COMMAND ${Python3_EXECUTABLE}
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/docs_service_examples.py" $<TARGET_FILE:gungnirc>)
        add_test(NAME gungnir.docs_routing_examples COMMAND ${Python3_EXECUTABLE}
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/docs_routing_examples.py" $<TARGET_FILE:gungnirc>)
        add_test(NAME gungnir.docs_auth_examples COMMAND ${Python3_EXECUTABLE}
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/docs_auth_examples.py" $<TARGET_FILE:gungnirc>)
        add_test(NAME gungnir.docs_context_examples COMMAND ${Python3_EXECUTABLE}
            ${CMAKE_CURRENT_SOURCE_DIR}/tests/docs_context_examples.py $<TARGET_FILE:gungnirc>)
        add_test(NAME gungnir.lsp_protocol COMMAND ${Python3_EXECUTABLE}
            ${CMAKE_CURRENT_SOURCE_DIR}/tests/lsp_protocol.py $<TARGET_FILE:gungnir-cli>)
        add_test(
            NAME gungnir.cli_integration
            COMMAND
                ${CMAKE_COMMAND}
                -DGUNGNIR_BUILD_DIR=${CMAKE_CURRENT_BINARY_DIR}
                -DGUNGNIR_CLI=$<TARGET_FILE:gungnir-cli>
                -DGUNGNIR_SOURCE_DIR=${CMAKE_CURRENT_SOURCE_DIR}
                -DPython3_EXECUTABLE=${Python3_EXECUTABLE}
                -DGUNGNIR_WITH_SQLITE=${GUNGNIR_WITH_SQLITE}
                -DGUNGNIR_BUILD_CONFIG=$<CONFIG>
                -P
                ${CMAKE_CURRENT_SOURCE_DIR}/tests/cli_integration.cmake
        )
        set_tests_properties(gungnir.cli_integration PROPERTIES TIMEOUT 900 RUN_SERIAL TRUE)
        add_test(
            NAME gungnir.compiler_profiles_cli
            COMMAND
                ${CMAKE_COMMAND}
                -DGUNGNIRC=$<TARGET_FILE:gungnirc>
                -P
                ${CMAKE_CURRENT_SOURCE_DIR}/tests/compiler_profiles.cmake
        )
        add_test(
            NAME gungnir.compiler_release_contract
            COMMAND
                ${CMAKE_COMMAND}
                -DGUNGNIRC=$<TARGET_FILE:gungnirc>
                -DGUNGNIR_VERSION=${GUNGNIR_VERSION_FULL}
                -P
                ${CMAKE_CURRENT_SOURCE_DIR}/tests/compiler_release_contract.cmake
        )
        add_test(
            NAME gungnir.compiler_conformance_snapshot
            COMMAND
                ${CMAKE_COMMAND}
                -DGUNGNIRC=$<TARGET_FILE:gungnirc>
                -DGUNGNIR_SOURCE_DIR=${CMAKE_CURRENT_SOURCE_DIR}
                -DGUNGNIR_OUTPUT_DIR=${CMAKE_CURRENT_BINARY_DIR}/compiler-conformance
                -P
                ${CMAKE_CURRENT_SOURCE_DIR}/tests/compiler_conformance.cmake
        )
    endif()

    add_executable(gungnir_bootstrap_test tests/bootstrap.cpp)
    target_link_libraries(gungnir_bootstrap_test PRIVATE gungnir::gungnir)
    target_compile_features(gungnir_bootstrap_test PRIVATE cxx_std_23)
    add_test(NAME gungnir.bootstrap COMMAND gungnir_bootstrap_test)

    add_executable(gungnir_plumbing_test tests/plumbing.cpp)
    target_link_libraries(gungnir_plumbing_test PRIVATE gungnir::gungnir)
    target_compile_features(gungnir_plumbing_test PRIVATE cxx_std_23)
    add_test(NAME gungnir.plumbing COMMAND gungnir_plumbing_test)

    add_executable(gungnir_style_test tests/style.cpp)
    target_link_libraries(gungnir_style_test PRIVATE gungnir::gungnir)
    target_compile_features(gungnir_style_test PRIVATE cxx_std_23)
    add_test(NAME gungnir.style COMMAND gungnir_style_test)

    add_executable(gungnir_http_test tests/http.cpp)
    target_link_libraries(gungnir_http_test PRIVATE gungnir::gungnir)
    target_compile_features(gungnir_http_test PRIVATE cxx_std_23)
    add_test(NAME gungnir.http COMMAND gungnir_http_test)

    add_executable(gungnir_http_reactor_test tests/http_reactor.cpp)
    target_link_libraries(gungnir_http_reactor_test PRIVATE gungnir::gungnir)
    target_compile_features(gungnir_http_reactor_test PRIVATE cxx_std_23)
    if(WIN32)
        target_link_libraries(gungnir_http_reactor_test PRIVATE ws2_32)
    endif()
    add_test(NAME gungnir.http_reactor COMMAND gungnir_http_reactor_test)

    add_executable(gungnir_timer_runtime_test tests/timer_runtime.cpp)
    target_link_libraries(gungnir_timer_runtime_test PRIVATE gungnir::gungnir)
    target_compile_features(gungnir_timer_runtime_test PRIVATE cxx_std_23)
    add_test(NAME gungnir.timer_runtime COMMAND gungnir_timer_runtime_test)

    add_executable(gungnir_http_server_test tests/http_server.cpp)
    target_link_libraries(gungnir_http_server_test PRIVATE gungnir::gungnir)
    target_compile_features(gungnir_http_server_test PRIVATE cxx_std_23)
    add_test(NAME gungnir.http_server COMMAND gungnir_http_server_test)

    add_executable(
        gungnir_http_streaming_test
        tests/http_streaming.cpp
    )
    target_link_libraries(
        gungnir_http_streaming_test
        PRIVATE
            gungnir::gungnir
    )
    target_compile_features(
        gungnir_http_streaming_test
        PRIVATE
            cxx_std_23
    )
    add_test(
        NAME gungnir.http_streaming
        COMMAND gungnir_http_streaming_test
    )

    add_executable(
        gungnir_websocket_test
        tests/websocket.cpp
    )
    target_link_libraries(
        gungnir_websocket_test
        PRIVATE
            gungnir::gungnir
    )
    target_compile_features(
        gungnir_websocket_test
        PRIVATE
            cxx_std_23
    )
    add_test(
        NAME gungnir.websocket
        COMMAND gungnir_websocket_test
    )

    if(GUNGNIR_WITH_HTTP2)
        add_executable(
            gungnir_http2_server_test
            tests/http2_server.cpp
        )

        target_link_libraries(
            gungnir_http2_server_test
            PRIVATE
                gungnir::gungnir
                CURL::libcurl
                OpenSSL::SSL
                OpenSSL::Crypto
        )

        target_compile_features(
            gungnir_http2_server_test
            PRIVATE
                cxx_std_23
        )

        add_test(
            NAME gungnir.http2_server
            COMMAND gungnir_http2_server_test
        )
    endif()

    if(GUNGNIR_WITH_TLS)
        add_executable(
            gungnir_tls_server_test
            tests/tls_server.cpp
        )

        target_link_libraries(
            gungnir_tls_server_test
            PRIVATE
                gungnir::gungnir
                OpenSSL::SSL
                OpenSSL::Crypto
        )

        target_compile_features(
            gungnir_tls_server_test
            PRIVATE
                cxx_std_23
        )

        add_test(
            NAME gungnir.tls_server
            COMMAND gungnir_tls_server_test
        )
    endif()

    add_executable(gungnir_request_test tests/request.cpp)
    target_link_libraries(gungnir_request_test PRIVATE gungnir::gungnir)
    target_compile_features(gungnir_request_test PRIVATE cxx_std_23)
    add_test(NAME gungnir.request COMMAND gungnir_request_test)

    add_executable(gungnir_middleware_test tests/middleware.cpp)
    target_link_libraries(gungnir_middleware_test PRIVATE gungnir::gungnir)
    target_compile_features(gungnir_middleware_test PRIVATE cxx_std_23)
    add_test(NAME gungnir.middleware COMMAND gungnir_middleware_test)

    add_executable(gungnir_validation_test tests/validation.cpp)
    target_link_libraries(gungnir_validation_test PRIVATE gungnir::gungnir)
    target_compile_features(gungnir_validation_test PRIVATE cxx_std_23)
    add_test(NAME gungnir.validation COMMAND gungnir_validation_test)

    add_executable(gungnir_events_test tests/events.cpp)
    target_link_libraries(gungnir_events_test PRIVATE gungnir::gungnir)
    target_compile_features(gungnir_events_test PRIVATE cxx_std_23)
    add_test(NAME gungnir.events COMMAND gungnir_events_test)

    add_executable(gungnir_mail_notifications_test tests/mail_notifications.cpp)
    target_link_libraries(gungnir_mail_notifications_test PRIVATE gungnir::gungnir)
    target_compile_features(gungnir_mail_notifications_test PRIVATE cxx_std_23)
    add_test(NAME gungnir.mail_notifications COMMAND gungnir_mail_notifications_test)

    add_executable(gungnir_queue_test tests/queue.cpp)
    target_link_libraries(gungnir_queue_test PRIVATE gungnir::gungnir)
    target_compile_features(gungnir_queue_test PRIVATE cxx_std_23)
    add_test(NAME gungnir.queue COMMAND gungnir_queue_test)

    add_executable(
        gungnir_observability_test
        tests/observability.cpp
    )
    target_link_libraries(
        gungnir_observability_test
        PRIVATE
            gungnir::gungnir
    )
    target_compile_features(
        gungnir_observability_test
        PRIVATE
            cxx_std_23
    )
    add_test(
        NAME gungnir.observability
        COMMAND gungnir_observability_test
    )

    add_executable(gungnir_scheduler_test tests/scheduler.cpp)
    target_link_libraries(gungnir_scheduler_test PRIVATE gungnir::gungnir)
    target_compile_features(gungnir_scheduler_test PRIVATE cxx_std_23)
    add_test(NAME gungnir.scheduler COMMAND gungnir_scheduler_test)

    add_executable(
        gungnir_runtime_supervisor_test
        tests/runtime_supervisor.cpp
    )
    target_link_libraries(
        gungnir_runtime_supervisor_test
        PRIVATE
            gungnir::gungnir
    )
    target_compile_features(
        gungnir_runtime_supervisor_test
        PRIVATE
            cxx_std_23
    )
    add_test(
        NAME gungnir.runtime_supervisor
        COMMAND gungnir_runtime_supervisor_test
    )

    add_executable(
        gungnir_runtime_host_test
        tests/runtime_host.cpp
    )
    target_link_libraries(
        gungnir_runtime_host_test
        PRIVATE
            gungnir::gungnir
    )
    target_compile_features(
        gungnir_runtime_host_test
        PRIVATE
            cxx_std_23
    )
    add_test(
        NAME gungnir.runtime_host
        COMMAND gungnir_runtime_host_test
    )

    add_executable(
        gungnir_runtime_lifecycle_test
        tests/runtime_lifecycle.cpp
    )
    target_link_libraries(
        gungnir_runtime_lifecycle_test
        PRIVATE
            gungnir::gungnir
    )
    target_compile_features(
        gungnir_runtime_lifecycle_test
        PRIVATE
            cxx_std_23
    )
    if(WIN32)
        target_link_libraries(
            gungnir_runtime_lifecycle_test
            PRIVATE
                ws2_32
        )
    endif()
    add_test(
        NAME gungnir.runtime_lifecycle
        COMMAND gungnir_runtime_lifecycle_test
    )
    set_tests_properties(
        gungnir.runtime_lifecycle
        PROPERTIES
            TIMEOUT 15
    )

    add_executable(
        gungnir_production_resilience_test
        tests/production_resilience.cpp
    )
    target_link_libraries(
        gungnir_production_resilience_test
        PRIVATE
            gungnir::gungnir
    )
    target_compile_features(
        gungnir_production_resilience_test
        PRIVATE
            cxx_std_23
    )
    add_test(
        NAME gungnir.production_resilience
        COMMAND gungnir_production_resilience_test
    )

    add_executable(
        gungnir_http_overload_test
        tests/http_overload.cpp
    )
    target_link_libraries(
        gungnir_http_overload_test
        PRIVATE
            gungnir::gungnir
    )
    target_compile_features(
        gungnir_http_overload_test
        PRIVATE
            cxx_std_23
    )
    if(WIN32)
        target_link_libraries(
            gungnir_http_overload_test
            PRIVATE
                ws2_32
        )
    endif()
    add_test(
        NAME gungnir.http_overload
        COMMAND gungnir_http_overload_test
    )
    set_tests_properties(
        gungnir.http_overload
        PROPERTIES
            TIMEOUT 15
    )

    add_executable(
        gungnir_runtime_signals_test
        tests/runtime_signals.cpp
    )
    target_link_libraries(
        gungnir_runtime_signals_test
        PRIVATE
            gungnir::gungnir
    )
    target_compile_features(
        gungnir_runtime_signals_test
        PRIVATE
            cxx_std_23
    )
    add_test(
        NAME gungnir.runtime_signals
        COMMAND gungnir_runtime_signals_test
    )

    add_executable(gungnir_storage_test tests/storage.cpp)
    target_link_libraries(gungnir_storage_test PRIVATE gungnir::gungnir)
    target_compile_features(gungnir_storage_test PRIVATE cxx_std_23)
    add_test(NAME gungnir.storage COMMAND gungnir_storage_test)

    add_executable(gungnir_cache_test tests/cache.cpp)
    target_link_libraries(gungnir_cache_test PRIVATE gungnir::gungnir)
    target_compile_features(gungnir_cache_test PRIVATE cxx_std_23)
    add_test(NAME gungnir.cache COMMAND gungnir_cache_test)

    add_executable(gungnir_session_test tests/session.cpp)
    target_link_libraries(gungnir_session_test PRIVATE gungnir::gungnir)
    target_compile_features(gungnir_session_test PRIVATE cxx_std_23)
    add_test(NAME gungnir.session COMMAND gungnir_session_test)

    add_executable(
        gungnir_session_lifecycle_test
        tests/session_lifecycle.cpp
    )
    target_link_libraries(
        gungnir_session_lifecycle_test
        PRIVATE
            gungnir::gungnir
    )
    target_compile_features(
        gungnir_session_lifecycle_test
        PRIVATE
            cxx_std_23
    )
    add_test(
        NAME gungnir.session_lifecycle
        COMMAND gungnir_session_lifecycle_test
    )

    add_executable(gungnir_security_test tests/security.cpp)
    target_link_libraries(gungnir_security_test PRIVATE gungnir::gungnir)
    target_compile_features(gungnir_security_test PRIVATE cxx_std_23)
    add_test(NAME gungnir.security COMMAND gungnir_security_test)

    add_executable(
        gungnir_security_hardening_test
        tests/security_hardening.cpp
    )
    target_link_libraries(
        gungnir_security_hardening_test
        PRIVATE
            gungnir::gungnir
    )
    target_compile_features(
        gungnir_security_hardening_test
        PRIVATE
            cxx_std_23
    )
    add_test(
        NAME gungnir.security_hardening
        COMMAND gungnir_security_hardening_test
    )

    add_executable(
        gungnir_csrf_test
        tests/csrf.cpp
    )
    target_link_libraries(
        gungnir_csrf_test
        PRIVATE
            gungnir::gungnir
    )
    target_compile_features(
        gungnir_csrf_test
        PRIVATE
            cxx_std_23
    )
    add_test(
        NAME gungnir.csrf
        COMMAND gungnir_csrf_test
    )

    add_executable(
        gungnir_auth_session_test
        tests/auth_session.cpp
    )
    target_link_libraries(
        gungnir_auth_session_test
        PRIVATE
            gungnir::gungnir
    )
    target_compile_features(
        gungnir_auth_session_test
        PRIVATE
            cxx_std_23
    )
    add_test(
        NAME gungnir.auth_session
        COMMAND gungnir_auth_session_test
    )

    add_executable(gungnir_exception_test tests/exceptions.cpp)
    target_link_libraries(gungnir_exception_test PRIVATE gungnir::gungnir)
    target_compile_features(gungnir_exception_test PRIVATE cxx_std_23)
    add_test(NAME gungnir.exceptions COMMAND gungnir_exception_test)

    add_executable(gungnir_view_test tests/view.cpp)
    target_link_libraries(gungnir_view_test PRIVATE gungnir::orm)
    target_compile_features(gungnir_view_test PRIVATE cxx_std_23)
    target_compile_definitions(
        gungnir_view_test
        PRIVATE
            GUNGNIR_TEST_VIEW_ROOT="${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/views"
    )
    add_test(NAME gungnir.view COMMAND gungnir_view_test)

    add_executable(
        gungnir_view_runtime_test
        tests/view_runtime.cpp
    )
    target_link_libraries(
        gungnir_view_runtime_test
        PRIVATE
            gungnir::gungnir
    )
    target_compile_features(
        gungnir_view_runtime_test
        PRIVATE
            cxx_std_23
    )
    add_test(
        NAME gungnir.view_runtime
        COMMAND gungnir_view_runtime_test
    )

    add_executable(
        gungnir_view_security_test
        tests/view_security.cpp
    )
    target_link_libraries(
        gungnir_view_security_test
        PRIVATE
            gungnir::gungnir
    )
    target_compile_features(
        gungnir_view_security_test
        PRIVATE
            cxx_std_23
    )
    add_test(
        NAME gungnir.view_security
        COMMAND gungnir_view_security_test
    )

    add_executable(gungnir_controller_routing_test tests/controller_routing.cpp)
    target_link_libraries(gungnir_controller_routing_test PRIVATE gungnir::gungnir)
    target_compile_features(gungnir_controller_routing_test PRIVATE cxx_std_23)
    add_test(NAME gungnir.controller_routing COMMAND gungnir_controller_routing_test)

    add_executable(gungnir_ioc_test tests/ioc.cpp)
    target_link_libraries(gungnir_ioc_test PRIVATE gungnir::gungnir)
    target_compile_features(gungnir_ioc_test PRIVATE cxx_std_23)
    add_test(NAME gungnir.ioc COMMAND gungnir_ioc_test)

    add_executable(gungnir_ioc_scope_test tests/ioc_scope.cpp)
    target_link_libraries(gungnir_ioc_scope_test PRIVATE gungnir::gungnir)
    target_compile_features(gungnir_ioc_scope_test PRIVATE cxx_std_23)
    add_test(NAME gungnir.ioc_scope COMMAND gungnir_ioc_scope_test)

    add_executable(gungnir_migration_test tests/migration.cpp)
    target_link_libraries(gungnir_migration_test PRIVATE gungnir::gungnir)
    target_compile_features(gungnir_migration_test PRIVATE cxx_std_23)
    add_test(NAME gungnir.migration COMMAND gungnir_migration_test)

    add_executable(gungnir_database_test tests/database.cpp)
    target_link_libraries(gungnir_database_test PRIVATE gungnir::gungnir)
    target_compile_features(gungnir_database_test PRIVATE cxx_std_23)
    add_test(NAME gungnir.database COMMAND gungnir_database_test)

    add_executable(
        gungnir_database_correctness_test
        tests/database_correctness.cpp
    )
    target_link_libraries(
        gungnir_database_correctness_test
        PRIVATE
            gungnir::gungnir
    )
    target_compile_features(
        gungnir_database_correctness_test
        PRIVATE
            cxx_std_23
    )
    add_test(
        NAME gungnir.database_correctness
        COMMAND gungnir_database_correctness_test
    )

    if(GUNGNIR_WITH_SQLITE)
        add_executable(
            gungnir_sqlite_correctness_test
            tests/sqlite_correctness.cpp
        )
        target_link_libraries(
            gungnir_sqlite_correctness_test
            PRIVATE
                gungnir::gungnir
        )
        target_compile_features(
            gungnir_sqlite_correctness_test
            PRIVATE
                cxx_std_23
        )
        add_test(
            NAME gungnir.sqlite_correctness
            COMMAND gungnir_sqlite_correctness_test
        )
    endif()

    add_executable(
        gungnir_database_cancellation_test
        tests/database_cancellation.cpp
    )
    target_link_libraries(
        gungnir_database_cancellation_test
        PRIVATE
            gungnir::gungnir
    )
    target_compile_features(
        gungnir_database_cancellation_test
        PRIVATE
            cxx_std_23
    )
    add_test(
        NAME gungnir.database_cancellation
        COMMAND gungnir_database_cancellation_test
    )

    add_executable(
        gungnir_database_scope_safety_test
        tests/database_scope_safety.cpp
    )
    target_link_libraries(
        gungnir_database_scope_safety_test
        PRIVATE
            gungnir::gungnir
    )
    target_compile_features(
        gungnir_database_scope_safety_test
        PRIVATE
            cxx_std_23
    )
    add_test(
        NAME gungnir.database_scope_safety
        COMMAND gungnir_database_scope_safety_test
    )

    add_executable(gungnir_model_test tests/model.cpp)
    target_link_libraries(gungnir_model_test PRIVATE gungnir::gungnir)
    target_compile_features(gungnir_model_test PRIVATE cxx_std_23)
    add_test(NAME gungnir.model COMMAND gungnir_model_test)

    add_executable(gungnir_model_plumbing_test tests/model_plumbing.cpp)
    target_link_libraries(gungnir_model_plumbing_test PRIVATE gungnir::gungnir)
    target_compile_features(gungnir_model_plumbing_test PRIVATE cxx_std_23)
    add_test(NAME gungnir.model_plumbing COMMAND gungnir_model_plumbing_test)

    add_executable(gungnir_orm_test tests/orm.cpp)
    target_link_libraries(gungnir_orm_test PRIVATE gungnir::orm)
    target_compile_features(gungnir_orm_test PRIVATE cxx_std_23)
    add_test(NAME gungnir.orm COMMAND gungnir_orm_test)

    add_executable(
        gungnir_orm_correctness_test
        tests/orm_correctness.cpp
    )
    target_link_libraries(
        gungnir_orm_correctness_test
        PRIVATE
            gungnir::orm
    )
    target_compile_features(
        gungnir_orm_correctness_test
        PRIVATE
            cxx_std_23
    )
    add_test(
        NAME gungnir.orm_correctness
        COMMAND gungnir_orm_correctness_test
    )

    add_executable(
        gungnir_database_bootstrap_test
        tests/database_bootstrap.cpp
    )
    target_link_libraries(
        gungnir_database_bootstrap_test
        PRIVATE
            gungnir::gungnir
    )
    target_compile_features(
        gungnir_database_bootstrap_test
        PRIVATE
            cxx_std_23
    )
    add_test(
        NAME gungnir.database_bootstrap
        COMMAND gungnir_database_bootstrap_test
    )

    add_executable(gungnir_database_runtime_test tests/database_runtime.cpp)
    target_link_libraries(gungnir_database_runtime_test PRIVATE gungnir::gungnir)
    target_compile_features(gungnir_database_runtime_test PRIVATE cxx_std_23)
    add_test(NAME gungnir.database_runtime COMMAND gungnir_database_runtime_test)

    add_executable(gungnir_orm_runtime_test tests/orm_runtime.cpp)
    target_link_libraries(gungnir_orm_runtime_test PRIVATE gungnir::orm)
    target_compile_features(gungnir_orm_runtime_test PRIVATE cxx_std_23)
    add_test(NAME gungnir.orm_runtime COMMAND gungnir_orm_runtime_test)

    add_executable(
        gungnir_orm_relations_test
        tests/orm_relations.cpp
    )
    target_link_libraries(
        gungnir_orm_relations_test
        PRIVATE
            gungnir::orm
    )
    target_compile_features(
        gungnir_orm_relations_test
        PRIVATE
            cxx_std_23
    )
    add_test(
        NAME gungnir.orm_relations
        COMMAND gungnir_orm_relations_test
    )

    add_executable(
        gungnir_orm_pivot_mutations_test
        tests/orm_pivot_mutations.cpp
    )
    target_link_libraries(
        gungnir_orm_pivot_mutations_test
        PRIVATE
            gungnir::orm
    )
    target_compile_features(
        gungnir_orm_pivot_mutations_test
        PRIVATE
            cxx_std_23
    )
    add_test(
        NAME gungnir.orm_pivot_mutations
        COMMAND gungnir_orm_pivot_mutations_test
    )

    add_executable(gungnir_migration_runner_test tests/migration_runner.cpp)
    target_link_libraries(gungnir_migration_runner_test PRIVATE gungnir::gungnir)
    target_compile_features(gungnir_migration_runner_test PRIVATE cxx_std_23)
    add_test(NAME gungnir.migration_runner COMMAND gungnir_migration_runner_test)

    add_executable(gungnir_orm_complete_test tests/orm_complete.cpp)
    target_link_libraries(gungnir_orm_complete_test PRIVATE gungnir::orm)
    target_compile_features(gungnir_orm_complete_test PRIVATE cxx_std_23)
    add_test(NAME gungnir.orm_complete COMMAND gungnir_orm_complete_test)

    add_executable(gungnir_orm_timestamps_test tests/orm_timestamps.cpp)
    target_link_libraries(gungnir_orm_timestamps_test PRIVATE gungnir::orm)
    target_compile_features(gungnir_orm_timestamps_test PRIVATE cxx_std_23)
    add_test(NAME gungnir.orm_timestamps COMMAND gungnir_orm_timestamps_test)

    add_executable(gungnir_orm_soft_deletes_test tests/orm_soft_deletes.cpp)
    target_link_libraries(gungnir_orm_soft_deletes_test PRIVATE gungnir::orm)
    target_compile_features(gungnir_orm_soft_deletes_test PRIVATE cxx_std_23)
    add_test(NAME gungnir.orm_soft_deletes COMMAND gungnir_orm_soft_deletes_test)

    add_executable(gungnir_orm_upsert_test tests/orm_upsert.cpp)
    target_link_libraries(gungnir_orm_upsert_test PRIVATE gungnir::orm)
    target_compile_features(gungnir_orm_upsert_test PRIVATE cxx_std_23)
    add_test(NAME gungnir.orm_upsert COMMAND gungnir_orm_upsert_test)

    add_executable(gungnir_orm_transaction_test tests/orm_transaction.cpp)
    target_link_libraries(gungnir_orm_transaction_test PRIVATE gungnir::orm)
    target_compile_features(gungnir_orm_transaction_test PRIVATE cxx_std_23)
    add_test(NAME gungnir.orm_transaction COMMAND gungnir_orm_transaction_test)

    if(
        GUNGNIR_WITH_POSTGRESQL AND
        GUNGNIR_POSTGRESQL_INTEGRATION_TESTS
    )
        add_executable(
            gungnir_postgresql_integration_test
            tests/postgresql_integration.cpp
        )

        target_link_libraries(
            gungnir_postgresql_integration_test
            PRIVATE
                gungnir::postgresql
        )

        target_compile_features(
            gungnir_postgresql_integration_test
            PRIVATE
                cxx_std_23
        )

        add_test(
            NAME gungnir.postgresql
            COMMAND
                gungnir_postgresql_integration_test
        )
    endif()

    if(
        GUNGNIR_WITH_MYSQL AND
        GUNGNIR_MYSQL_INTEGRATION_TESTS
    )
        add_executable(
            gungnir_mysql_integration_test
            tests/mysql_integration.cpp
        )

        target_link_libraries(
            gungnir_mysql_integration_test
            PRIVATE
                gungnir::mysql
        )

        target_compile_features(
            gungnir_mysql_integration_test
            PRIVATE
                cxx_std_23
        )

        add_test(
            NAME gungnir.mysql
            COMMAND
                gungnir_mysql_integration_test
        )
    endif()

    if(
        GUNGNIR_WITH_SQLSERVER AND
        GUNGNIR_SQLSERVER_INTEGRATION_TESTS
    )
        add_executable(
            gungnir_sqlserver_integration_test
            tests/sqlserver_integration.cpp
        )

        target_link_libraries(
            gungnir_sqlserver_integration_test
            PRIVATE
                gungnir::sqlserver
                gungnir::orm
        )

        target_compile_features(
            gungnir_sqlserver_integration_test
            PRIVATE
                cxx_std_23
        )

        add_test(
            NAME gungnir.sqlserver
            COMMAND
                gungnir_sqlserver_integration_test
        )
    endif()

    if(
        GUNGNIR_WITH_SMTP AND
        GUNGNIR_SMTP_INTEGRATION_TESTS
    )
        add_executable(
            gungnir_smtp_integration_test
            tests/smtp_integration.cpp
        )

        target_link_libraries(
            gungnir_smtp_integration_test
            PRIVATE
                gungnir::smtp
        )

        target_compile_features(
            gungnir_smtp_integration_test
            PRIVATE
                cxx_std_23
        )

        add_test(
            NAME gungnir.smtp
            COMMAND
                gungnir_smtp_integration_test
        )
    endif()

    if(GUNGNIR_WITH_OTLP)
        add_executable(
            gungnir_otlp_test
            tests/otlp.cpp
        )

        target_link_libraries(
            gungnir_otlp_test
            PRIVATE
                gungnir::otlp
        )

        target_compile_features(
            gungnir_otlp_test
            PRIVATE
                cxx_std_23
        )

        add_test(
            NAME gungnir.otlp
            COMMAND
                gungnir_otlp_test
        )
    endif()

    if(
        GUNGNIR_WITH_MONGODB AND
        GUNGNIR_MONGODB_INTEGRATION_TESTS
    )
        add_executable(
            gungnir_mongodb_integration_test
            tests/mongodb_integration.cpp
        )

        target_link_libraries(
            gungnir_mongodb_integration_test
            PRIVATE
                gungnir::mongodb
                gungnir::orm
        )

        target_compile_features(
            gungnir_mongodb_integration_test
            PRIVATE
                cxx_std_23
        )

        add_test(
            NAME gungnir.mongodb
            COMMAND
                gungnir_mongodb_integration_test
        )
    endif()

    if(
        GUNGNIR_WITH_REDIS AND
        GUNGNIR_REDIS_INTEGRATION_TESTS
    )
        add_executable(
            gungnir_redis_integration_test
            tests/redis_integration.cpp
        )

        target_link_libraries(
            gungnir_redis_integration_test
            PRIVATE
                gungnir::redis
        )

        target_compile_features(
            gungnir_redis_integration_test
            PRIVATE
                cxx_std_23
        )

        add_test(
            NAME gungnir.redis
            COMMAND
                gungnir_redis_integration_test
        )
    endif()
endif()


install(
    TARGETS
        gungnir
        gungnir-orm
        gungnir-language
    EXPORT
        GungnirTargets
    ARCHIVE DESTINATION
        ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION
        ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION
        ${CMAKE_INSTALL_BINDIR}
    INCLUDES DESTINATION
        ${CMAKE_INSTALL_INCLUDEDIR}
)

if(GUNGNIR_WITH_POSTGRESQL)
    install(
        TARGETS
            gungnir-postgresql
        EXPORT
            GungnirTargets
        ARCHIVE DESTINATION
            ${CMAKE_INSTALL_LIBDIR}
        LIBRARY DESTINATION
            ${CMAKE_INSTALL_LIBDIR}
        RUNTIME DESTINATION
            ${CMAKE_INSTALL_BINDIR}
        INCLUDES DESTINATION
            ${CMAKE_INSTALL_INCLUDEDIR}
    )
endif()

if(GUNGNIR_WITH_MYSQL)
    install(
        TARGETS
            gungnir-mysql
        EXPORT
            GungnirTargets
        ARCHIVE DESTINATION
            ${CMAKE_INSTALL_LIBDIR}
        LIBRARY DESTINATION
            ${CMAKE_INSTALL_LIBDIR}
        RUNTIME DESTINATION
            ${CMAKE_INSTALL_BINDIR}
        INCLUDES DESTINATION
            ${CMAKE_INSTALL_INCLUDEDIR}
    )

    install(
        FILES
            cmake/FindMariaDB.cmake
        DESTINATION
            ${CMAKE_INSTALL_LIBDIR}/cmake/Gungnir/modules
    )
endif()

if(GUNGNIR_WITH_SQLSERVER)
    install(
        TARGETS
            gungnir-sqlserver
        EXPORT
            GungnirTargets
        ARCHIVE DESTINATION
            ${CMAKE_INSTALL_LIBDIR}
        LIBRARY DESTINATION
            ${CMAKE_INSTALL_LIBDIR}
        RUNTIME DESTINATION
            ${CMAKE_INSTALL_BINDIR}
        INCLUDES DESTINATION
            ${CMAKE_INSTALL_INCLUDEDIR}
    )
endif()

if(GUNGNIR_WITH_MONGODB)
    install(
        TARGETS
            gungnir-mongodb
        EXPORT
            GungnirTargets
        ARCHIVE DESTINATION
            ${CMAKE_INSTALL_LIBDIR}
        LIBRARY DESTINATION
            ${CMAKE_INSTALL_LIBDIR}
        RUNTIME DESTINATION
            ${CMAKE_INSTALL_BINDIR}
        INCLUDES DESTINATION
            ${CMAKE_INSTALL_INCLUDEDIR}
    )
endif()

if(GUNGNIR_WITH_SMTP)
    install(
        TARGETS
            gungnir-smtp
        EXPORT
            GungnirTargets
        ARCHIVE DESTINATION
            ${CMAKE_INSTALL_LIBDIR}
        LIBRARY DESTINATION
            ${CMAKE_INSTALL_LIBDIR}
        RUNTIME DESTINATION
            ${CMAKE_INSTALL_BINDIR}
        INCLUDES DESTINATION
            ${CMAKE_INSTALL_INCLUDEDIR}
    )
endif()

if(GUNGNIR_WITH_OTLP)
    install(
        TARGETS
            gungnir-otlp
        EXPORT
            GungnirTargets
        ARCHIVE DESTINATION
            ${CMAKE_INSTALL_LIBDIR}
        LIBRARY DESTINATION
            ${CMAKE_INSTALL_LIBDIR}
        RUNTIME DESTINATION
            ${CMAKE_INSTALL_BINDIR}
        INCLUDES DESTINATION
            ${CMAKE_INSTALL_INCLUDEDIR}
    )
endif()

if(GUNGNIR_WITH_REDIS)
    install(
        TARGETS
            gungnir-redis
        EXPORT
            GungnirTargets
        ARCHIVE DESTINATION
            ${CMAKE_INSTALL_LIBDIR}
        LIBRARY DESTINATION
            ${CMAKE_INSTALL_LIBDIR}
        RUNTIME DESTINATION
            ${CMAKE_INSTALL_BINDIR}
        INCLUDES DESTINATION
            ${CMAKE_INSTALL_INCLUDEDIR}
    )

    install(
        FILES
            cmake/FindHiredis.cmake
        DESTINATION
            ${CMAKE_INSTALL_LIBDIR}/cmake/Gungnir/modules
    )
endif()

if(GUNGNIR_WITH_HTTP2)
    install(
        FILES
            cmake/FindNghttp2.cmake
        DESTINATION
            ${CMAKE_INSTALL_LIBDIR}/cmake/Gungnir/modules
    )
endif()

install(
    DIRECTORY
        include/
    DESTINATION
        ${CMAKE_INSTALL_INCLUDEDIR}
)

install(
    FILES
        "${GUNGNIR_GENERATED_INCLUDE_DIR}/gungnir/version.hpp"
    DESTINATION
        "${CMAKE_INSTALL_INCLUDEDIR}/gungnir"
)

install(
    EXPORT
        GungnirTargets
    FILE
        GungnirTargets.cmake
    NAMESPACE
        gungnir::
    DESTINATION
        ${CMAKE_INSTALL_LIBDIR}/cmake/Gungnir
)

configure_package_config_file(
    cmake/GungnirConfig.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/GungnirConfig.cmake
    INSTALL_DESTINATION
        ${CMAKE_INSTALL_LIBDIR}/cmake/Gungnir
)

write_basic_package_version_file(
    ${CMAKE_CURRENT_BINARY_DIR}/GungnirConfigVersion.cmake
    VERSION
        ${PROJECT_VERSION}
    COMPATIBILITY
        ExactVersion
)

install(
    FILES
        ${CMAKE_CURRENT_BINARY_DIR}/GungnirConfig.cmake
        ${CMAKE_CURRENT_BINARY_DIR}/GungnirConfigVersion.cmake
    DESTINATION
        ${CMAKE_INSTALL_LIBDIR}/cmake/Gungnir
)

if(GUNGNIR_BUILD_TOOLS)
    install(
        TARGETS
            gungnirc
            gungnir-cli
        RUNTIME DESTINATION
            ${CMAKE_INSTALL_BINDIR}
    )
endif()

install(
    FILES
        README.md
        LICENSE
    DESTINATION
        .
)

if(WIN32)
    install(
        FILES
            cmake/windows/UpdateUserPath.ps1
        DESTINATION
            ${CMAKE_INSTALL_DATADIR}/gungnir/installer
    )
endif()


if(GUNGNIR_BUILD_TESTS)
    add_executable(gungnir_priority3_runtime_test tests/priority3_runtime.cpp)
    target_link_libraries(gungnir_priority3_runtime_test PRIVATE gungnir::gungnir)
    add_test(NAME gungnir_priority3_runtime COMMAND gungnir_priority3_runtime_test)
    set_tests_properties(gungnir_priority3_runtime PROPERTIES TIMEOUT 30)
    add_executable(gungnir_priority3_integrations_test tests/priority3_integrations.cpp)
    target_link_libraries(gungnir_priority3_integrations_test PRIVATE gungnir::gungnir gungnir::orm)
    add_test(NAME gungnir_priority3_integrations COMMAND gungnir_priority3_integrations_test)
    set_tests_properties(gungnir_priority3_integrations PROPERTIES TIMEOUT 30)
    if(GUNGNIR_WITH_S3)
        find_package(Python3 COMPONENTS Interpreter REQUIRED)
        add_executable(gungnir_storage_s3_test tests/storage_s3.cpp)
        target_link_libraries(gungnir_storage_s3_test PRIVATE gungnir::gungnir)
        add_test(NAME gungnir_storage_s3 COMMAND ${Python3_EXECUTABLE}
            ${CMAKE_CURRENT_SOURCE_DIR}/tests/s3_mock.py $<TARGET_FILE:gungnir_storage_s3_test>)
        set_tests_properties(gungnir_storage_s3 PROPERTIES TIMEOUT 30)
    endif()
endif()

# Test assertions include operations under test. Keep them active in every
# configuration, including generated Release CI builds.
if(GUNGNIR_BUILD_TESTS)
    get_property(gungnir_directory_targets DIRECTORY PROPERTY BUILDSYSTEM_TARGETS)
    foreach(target IN LISTS gungnir_directory_targets)
        if(target MATCHES "_test$")
            if(MSVC)
                target_compile_options(${target} PRIVATE /UNDEBUG)
            else()
                target_compile_options(${target} PRIVATE -UNDEBUG)
            endif()
        endif()
    endforeach()
endif()

# CPack keeps the release artifacts aligned with the CMake install layout.
set(CPACK_PACKAGE_NAME "Gungnir")
set(CPACK_PACKAGE_VENDOR "Gungnir")
set(
    CPACK_PACKAGE_DESCRIPTION_SUMMARY
    "Expressive C++23 web framework"
)
set(
    CPACK_PACKAGE_HOMEPAGE_URL
    "https://github.com/justinangeloperez327/gungnir"
)
set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
set(CPACK_PACKAGE_INSTALL_DIRECTORY "Gungnir")
set(CPACK_PACKAGE_INSTALL_REGISTRY_KEY "Gungnir")
set(
    CPACK_RESOURCE_FILE_LICENSE
    "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE"
)
set(
    CPACK_RESOURCE_FILE_README
    "${CMAKE_CURRENT_SOURCE_DIR}/README.md"
)
set(CPACK_VERBATIM_VARIABLES ON)

if(WIN32)
    set(CPACK_GENERATOR "NSIS")
    set(
        CPACK_PACKAGE_FILE_NAME
        "gungnir-${GUNGNIR_VERSION_FULL}-windows-x86_64-setup"
    )
    set(CPACK_NSIS_INSTALL_ROOT "$PROGRAMFILES64")
    set(CPACK_NSIS_DISPLAY_NAME "Gungnir")
    set(CPACK_NSIS_PACKAGE_NAME "Gungnir")
    set(CPACK_NSIS_EXECUTABLES_DIRECTORY "bin")

    # CPack's built-in PATH page edits the machine PATH through NSIS string
    # handling, which can fail when an existing PATH is already long. Gungnir
    # instead updates the current user's PATH with PowerShell/.NET and keeps
    # the system PATH untouched.
    set(CPACK_NSIS_MODIFY_PATH OFF)
    set(
        CPACK_NSIS_EXTRA_INSTALL_COMMANDS
        [=[
nsExec::ExecToLog 'powershell.exe -NoProfile -NonInteractive -ExecutionPolicy Bypass -File "$INSTDIR\share\gungnir\installer\UpdateUserPath.ps1" -BinPath "$INSTDIR\bin" -Action Add'
Pop $0
DetailPrint "Gungnir user PATH update exit code: $0"
SendMessage 0xffff 0x001A 0 "STR:Environment" /TIMEOUT=5000
        ]=]
    )
    set(
        CPACK_NSIS_EXTRA_UNINSTALL_COMMANDS
        [=[
nsExec::ExecToLog 'powershell.exe -NoProfile -NonInteractive -ExecutionPolicy Bypass -File "$INSTDIR\share\gungnir\installer\UpdateUserPath.ps1" -BinPath "$INSTDIR\bin" -Action Remove'
Pop $0
DetailPrint "Gungnir user PATH removal exit code: $0"
SendMessage 0xffff 0x001A 0 "STR:Environment" /TIMEOUT=5000
        ]=]
    )
    set(CPACK_NSIS_ENABLE_UNINSTALL_BEFORE_INSTALL ON)
    set(CPACK_NSIS_MANIFEST_DPI_AWARE ON)
    set(CPACK_NSIS_UNINSTALL_NAME "Uninstall Gungnir")
    set(
        CPACK_NSIS_URL_INFO_ABOUT
        "https://github.com/justinangeloperez327/gungnir"
    )
    set(
        CPACK_NSIS_HELP_LINK
        "https://github.com/justinangeloperez327/gungnir/issues"
    )
endif()

include(CPack)
