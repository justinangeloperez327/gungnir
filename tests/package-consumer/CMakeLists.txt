cmake_minimum_required(VERSION 3.25)

project(gungnir-package-consumer LANGUAGES CXX)

if(NOT GUNGNIR_CONSUMER_SOURCE)
    message(FATAL_ERROR "GUNGNIR_CONSUMER_SOURCE must point to transpiled Gungnir C++")
endif()

find_package(Gungnir CONFIG REQUIRED)

add_executable(
    gungnir-package-consumer
    "${GUNGNIR_CONSUMER_SOURCE}"
)

target_compile_features(
    gungnir-package-consumer
    PRIVATE
        cxx_std_23
)

target_link_libraries(
    gungnir-package-consumer
    PRIVATE
        gungnir::gungnir
        gungnir::orm
        gungnir::language
)
