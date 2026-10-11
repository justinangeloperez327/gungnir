cmake_minimum_required(VERSION 3.25)

project(
    gungnir_otlp_package_consumer
    LANGUAGES CXX
)

find_package(
    Gungnir
    CONFIG
    REQUIRED
)

# Compile the public bootstrap example against installed headers/libraries.
file(READ "${CMAKE_CURRENT_SOURCE_DIR}/../../docs/external-observability.md" guide)
string(REGEX MATCH "```cpp\n([^`]*)```" guide_code "${guide}")
if(NOT guide_code)
    message(FATAL_ERROR "The external observability guide has no C++ bootstrap example")
endif()
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/guide.hpp" "${CMAKE_MATCH_1}")

add_executable(
    gungnir-otlp-package-consumer
    main.cpp
)

target_link_libraries(
    gungnir-otlp-package-consumer
    PRIVATE
        gungnir::otlp
        gungnir::orm
)
target_include_directories(gungnir-otlp-package-consumer PRIVATE "${CMAKE_CURRENT_BINARY_DIR}")

target_compile_features(
    gungnir-otlp-package-consumer
    PRIVATE
        cxx_std_23
)

enable_testing()
add_test(NAME installed.otlp COMMAND gungnir-otlp-package-consumer)
