cmake_minimum_required(VERSION 3.25)

project(gungnir-sqlserver-package-consumer LANGUAGES CXX)

find_package(Gungnir CONFIG REQUIRED)

if(NOT TARGET gungnir::sqlserver)
    message(
        FATAL_ERROR
        "Installed Gungnir package does not export gungnir::sqlserver"
    )
endif()

add_executable(
    gungnir-sqlserver-package-consumer
    main.cpp
)

target_compile_features(
    gungnir-sqlserver-package-consumer
    PRIVATE
        cxx_std_23
)

target_link_libraries(
    gungnir-sqlserver-package-consumer
    PRIVATE
        gungnir::sqlserver
)
