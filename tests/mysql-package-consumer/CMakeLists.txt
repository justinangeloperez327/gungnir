cmake_minimum_required(VERSION 3.25)

project(gungnir-mysql-package-consumer LANGUAGES CXX)

find_package(Gungnir CONFIG REQUIRED)

if(NOT TARGET gungnir::mysql)
    message(
        FATAL_ERROR
        "Installed Gungnir package does not export gungnir::mysql"
    )
endif()

add_executable(
    gungnir-mysql-package-consumer
    main.cpp
)

target_compile_features(
    gungnir-mysql-package-consumer
    PRIVATE
        cxx_std_23
)

target_link_libraries(
    gungnir-mysql-package-consumer
    PRIVATE
        gungnir::mysql
)
