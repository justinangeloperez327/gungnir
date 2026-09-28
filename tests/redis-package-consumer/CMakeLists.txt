cmake_minimum_required(VERSION 3.25)

project(
    gungnir_redis_package_consumer
    LANGUAGES CXX
)

find_package(
    Gungnir
    CONFIG
    REQUIRED
)

add_executable(
    gungnir-redis-package-consumer
    main.cpp
)

target_link_libraries(
    gungnir-redis-package-consumer
    PRIVATE
        gungnir::redis
)

target_compile_features(
    gungnir-redis-package-consumer
    PRIVATE
        cxx_std_23
)
