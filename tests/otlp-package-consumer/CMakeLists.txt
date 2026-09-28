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

add_executable(
    gungnir-otlp-package-consumer
    main.cpp
)

target_link_libraries(
    gungnir-otlp-package-consumer
    PRIVATE
        gungnir::otlp
)

target_compile_features(
    gungnir-otlp-package-consumer
    PRIVATE
        cxx_std_23
)
