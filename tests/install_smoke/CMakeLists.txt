cmake_minimum_required(VERSION 3.25)

project(gungnir_install_smoke LANGUAGES CXX)

find_package(Gungnir CONFIG REQUIRED)

if(NOT Gungnir_RELEASE_VERSION STREQUAL "development")
    message(
        FATAL_ERROR
        "Installed Gungnir release identity is not development"
    )
endif()

if(NOT Gungnir_NATIVE_API_CONTRACT STREQUAL "development")
    message(
        FATAL_ERROR
        "Installed Gungnir native API contract is not development"
    )
endif()

if(NOT Gungnir_NATIVE_ABI_EPOCH STREQUAL "0")
    message(
        FATAL_ERROR
        "Installed Gungnir native ABI epoch is not development epoch 0"
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

if(DEFINED GUNGNIR_STRUCTURED_CONTEXT_GENERATED)
    get_filename_component(context_generated_dir "${GUNGNIR_STRUCTURED_CONTEXT_GENERATED}" DIRECTORY)
    add_executable(gungnir_installed_context ../structured_request_context.cpp)
    target_include_directories(gungnir_installed_context PRIVATE "${context_generated_dir}")
    target_compile_features(gungnir_installed_context PRIVATE cxx_std_23)
    target_link_libraries(gungnir_installed_context PRIVATE gungnir::gungnir gungnir::orm gungnir::language)
endif()
