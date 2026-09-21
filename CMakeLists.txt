cmake_minimum_required(VERSION 3.25)

project(gungnir VERSION 0.1.0 LANGUAGES CXX)

option(GUNGNIR_BUILD_TESTS "Build Gungnir tests" ON)

add_library(gungnir
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
    src/http/request.cpp
    src/http/response.cpp
    src/migration/column.cpp
    src/migration/context.cpp
    src/migration/definition.cpp
    src/migration/migration.cpp
    src/migration/runner.cpp
    src/migration/table.cpp
    src/routing/router.cpp
)

add_library(gungnir::gungnir ALIAS gungnir)

add_library(gungnir-orm
    src/orm/compiler.cpp
    src/orm/mutation.cpp
)

add_library(gungnir::orm ALIAS gungnir-orm)

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
else()
    target_compile_options(gungnir PRIVATE -Wall -Wextra -Wpedantic -Wconversion -Wshadow)
    target_compile_options(gungnir-orm PRIVATE -Wall -Wextra -Wpedantic -Wconversion -Wshadow)
endif()

if(GUNGNIR_BUILD_TESTS)
    enable_testing()

    add_executable(gungnir_plumbing_test tests/plumbing.cpp)
    target_link_libraries(gungnir_plumbing_test PRIVATE gungnir::gungnir)
    target_compile_features(gungnir_plumbing_test PRIVATE cxx_std_23)
    add_test(NAME gungnir.plumbing COMMAND gungnir_plumbing_test)

    add_executable(gungnir_style_test tests/style.cpp)
    target_link_libraries(gungnir_style_test PRIVATE gungnir::gungnir)
    target_compile_features(gungnir_style_test PRIVATE cxx_std_23)
    add_test(NAME gungnir.style COMMAND gungnir_style_test)

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
endif()
