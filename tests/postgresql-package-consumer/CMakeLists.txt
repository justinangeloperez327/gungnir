cmake_minimum_required(VERSION 3.25)

project(gungnir-postgresql-package-consumer LANGUAGES CXX)

find_package(Gungnir CONFIG REQUIRED)

if(NOT TARGET gungnir::postgresql)
    message(
        FATAL_ERROR
        "Installed Gungnir package does not export gungnir::postgresql"
    )
endif()

add_executable(
    gungnir-postgresql-package-consumer
    main.cpp
)

target_compile_features(
    gungnir-postgresql-package-consumer
    PRIVATE
        cxx_std_23
)

target_link_libraries(
    gungnir-postgresql-package-consumer
    PRIVATE
        gungnir::postgresql
)
