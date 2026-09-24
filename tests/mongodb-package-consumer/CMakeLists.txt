cmake_minimum_required(VERSION 3.25)

project(gungnir-mongodb-package-consumer LANGUAGES CXX)

find_package(Gungnir CONFIG REQUIRED)

if(NOT TARGET gungnir::mongodb)
    message(
        FATAL_ERROR
        "Installed Gungnir package does not export gungnir::mongodb"
    )
endif()

add_executable(
    gungnir-mongodb-package-consumer
    main.cpp
)

target_compile_features(
    gungnir-mongodb-package-consumer
    PRIVATE
        cxx_std_23
)

target_link_libraries(
    gungnir-mongodb-package-consumer
    PRIVATE
        gungnir::mongodb
)
