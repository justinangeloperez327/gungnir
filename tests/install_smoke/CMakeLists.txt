cmake_minimum_required(VERSION 3.25)

project(gungnir_install_smoke LANGUAGES CXX)

find_package(Gungnir 1.0 CONFIG REQUIRED)

if(NOT Gungnir_RELEASE_VERSION STREQUAL "1.0.0-rc.1")
    message(
        FATAL_ERROR
        "Installed Gungnir release version is not 1.0.0-rc.1"
    )
endif()

if(NOT Gungnir_NATIVE_API_CONTRACT STREQUAL "1.0")
    message(
        FATAL_ERROR
        "Installed Gungnir native API contract is not 1.0"
    )
endif()

if(NOT Gungnir_NATIVE_ABI_EPOCH STREQUAL "1")
    message(
        FATAL_ERROR
        "Installed Gungnir native ABI epoch is not 1"
    )
endif()

add_executable(gungnir_install_smoke main.cpp)

target_compile_features(
    gungnir_install_smoke
    PRIVATE
        cxx_std_23
)

target_link_libraries(
    gungnir_install_smoke
    PRIVATE
        gungnir::gungnir
        gungnir::orm
        gungnir::language
)
