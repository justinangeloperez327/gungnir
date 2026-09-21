cmake_minimum_required(VERSION 3.25)

project(gungnir VERSION 0.1.0 LANGUAGES CXX)

option(GUNGNIR_BUILD_TESTS "Build Gungnir tests" ON)

add_library(gungnir
    src/core/application.cpp
    src/core/container.cpp
    src/controller/controller.cpp
    src/database/compiler.cpp
    src/http/request.cpp
    src/http/response.cpp
    src/migration/column.cpp
    src/migration/context.cpp
    src/migration/definition.cpp
    src/migration/migration.cpp
    src/migration/table.cpp
    src/routing/router.cpp
)

add_library(gungnir::gungnir ALIAS gungnir)

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
else()
    target_compile_options(gungnir PRIVATE -Wall -Wextra -Wpedantic -Wconversion -Wshadow)
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
endif()
