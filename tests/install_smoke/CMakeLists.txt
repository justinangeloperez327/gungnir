cmake_minimum_required(VERSION 3.25)

project(gungnir_install_smoke LANGUAGES CXX)

find_package(Gungnir CONFIG REQUIRED)

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
