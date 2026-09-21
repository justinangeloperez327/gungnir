cmake_minimum_required(VERSION 3.25)

project(gungnir VERSION 0.1.0 LANGUAGES CXX)

option(GUNGNIR_BUILD_TESTS "Build Gungnir tests" ON)
option(GUNGNIR_BUILD_TOOLS "Build Gungnir command-line tools" ON)

find_package(Threads REQUIRED)

add_library(gungnir
    src/config/environment.cpp
    src/config/repository.cpp
    src/core/application.cpp
    src/core/container.cpp
    src/controller/controller.cpp
    src/database/compiler.cpp
    src/database/connection.cpp
    src/database/driver.cpp
    src/database/manager.cpp
    src/database/pool.cpp
    src/database/runtime.cpp
    src/database/transaction.cpp
    src/http/exception_handler.cpp
    src/http/json.cpp
    src/http/message.cpp
    src/http/request.cpp
    src/http/response.cpp
    src/http/server.cpp
    src/migration/column.cpp
    src/migration/context.cpp
    src/migration/definition.cpp
    src/migration/migration.cpp
    src/migration/runner.cpp
    src/migration/table.cpp
    src/routing/route.cpp
    src/routing/router.cpp
    src/validation/validator.cpp
    src/view/engine.cpp
    src/view/runtime.cpp
    src/view/value.cpp
)

add_library(gungnir::gungnir ALIAS gungnir)

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
    src/language/lexer.cpp
    src/language/middleware_lowering.cpp
    src/language/model_lowering.cpp
    src/language/parser.cpp
    src/language/transpiler.cpp
    src/language/validation_lowering.cpp
    src/language/view_lowering.cpp
)

add_library(gungnir::language ALIAS gungnir-language)

target_compile_features(gungnir-language PUBLIC cxx_std_23)
target_include_directories(gungnir-language
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

set_target_properties(gungnir-language PROPERTIES
    CXX_EXTENSIONS OFF
    POSITION_INDEPENDENT_CODE ON
)

target_compile_features(gungnir-orm PUBLIC cxx_std_23)
target_link_libraries(gungnir-orm PUBLIC gungnir::gungnir)
target_include_directories(gungnir-orm
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

set_target_properties(gungnir-orm PROPERTIES
    CXX_EXTENSIONS OFF
    POSITION_INDEPENDENT_CODE ON
)

target_compile_features(gungnir PUBLIC cxx_std_23)
target_link_libraries(gungnir PRIVATE Threads::Threads)

if(WIN32)
    target_link_libraries(gungnir PRIVATE ws2_32)
endif()

target_include_directories(gungnir
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

set_target_properties(gungnir PROPERTIES
    CXX_EXTENSIONS OFF
    POSITION_INDEPENDENT_CODE ON
)

if(MSVC)
    target_compile_options(gungnir PRIVATE /W4 /permissive-)
    target_compile_options(gungnir-orm PRIVATE /W4 /permissive-)
    target_compile_options(gungnir-language PRIVATE /W4 /permissive-)
else()
    target_compile_options(gungnir PRIVATE -Wall -Wextra -Wpedantic -Wconversion -Wshadow)
    target_compile_options(gungnir-orm PRIVATE -Wall -Wextra -Wpedantic -Wconversion -Wshadow)
    target_compile_options(gungnir-language PRIVATE -Wall -Wextra -Wpedantic -Wconversion -Wshadow)
endif()

if(GUNGNIR_BUILD_TOOLS)
    add_executable(gungnirc tools/gungnirc.cpp)
    target_link_libraries(gungnirc PRIVATE gungnir::language)
    target_compile_features(gungnirc PRIVATE cxx_std_23)
endif()

if(GUNGNIR_BUILD_TESTS)
    enable_testing()

    add_executable(gungnir_language_test tests/language.cpp)
    target_link_libraries(gungnir_language_test PRIVATE gungnir::language)
    target_compile_features(gungnir_language_test PRIVATE cxx_std_23)
    add_test(NAME gungnir.language COMMAND gungnir_language_test)

    if(GUNGNIR_BUILD_TOOLS)
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

    add_executable(gungnir_controller_routing_test tests/controller_routing.cpp)
    target_link_libraries(gungnir_controller_routing_test PRIVATE gungnir::gungnir)
    target_compile_features(gungnir_controller_routing_test PRIVATE cxx_std_23)
    add_test(NAME gungnir.controller_routing COMMAND gungnir_controller_routing_test)

    add_executable(gungnir_ioc_test tests/ioc.cpp)
    target_link_libraries(gungnir_ioc_test PRIVATE gungnir::gungnir)
    target_compile_features(gungnir_ioc_test PRIVATE cxx_std_23)
    add_test(NAME gungnir.ioc COMMAND gungnir_ioc_test)

    add_executable(gungnir_migration_test tests/migration.cpp)
    target_link_libraries(gungnir_migration_test PRIVATE gungnir::gungnir)
    target_compile_features(gungnir_migration_test PRIVATE cxx_std_23)
    add_test(NAME gungnir.migration COMMAND gungnir_migration_test)

    add_executable(gungnir_database_test tests/database.cpp)
    target_link_libraries(gungnir_database_test PRIVATE gungnir::gungnir)
    target_compile_features(gungnir_database_test PRIVATE cxx_std_23)
    add_test(NAME gungnir.database COMMAND gungnir_database_test)

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

    add_executable(gungnir_database_runtime_test tests/database_runtime.cpp)
    target_link_libraries(gungnir_database_runtime_test PRIVATE gungnir::gungnir)
    target_compile_features(gungnir_database_runtime_test PRIVATE cxx_std_23)
    add_test(NAME gungnir.database_runtime COMMAND gungnir_database_runtime_test)

    add_executable(gungnir_orm_runtime_test tests/orm_runtime.cpp)
    target_link_libraries(gungnir_orm_runtime_test PRIVATE gungnir::orm)
    target_compile_features(gungnir_orm_runtime_test PRIVATE cxx_std_23)
    add_test(NAME gungnir.orm_runtime COMMAND gungnir_orm_runtime_test)

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
endif()
