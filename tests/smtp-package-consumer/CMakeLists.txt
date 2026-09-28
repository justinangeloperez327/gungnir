cmake_minimum_required(VERSION 3.25)

project(
    gungnir_smtp_package_consumer
    LANGUAGES CXX
)

find_package(
    Gungnir
    CONFIG
    REQUIRED
)

add_executable(
    gungnir-smtp-package-consumer
    main.cpp
)

target_link_libraries(
    gungnir-smtp-package-consumer
    PRIVATE
        gungnir::smtp
)

target_compile_features(
    gungnir-smtp-package-consumer
    PRIVATE
        cxx_std_23
)
