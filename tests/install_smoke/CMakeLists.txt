cmake_minimum_required(VERSION 3.25)

project(gungnir_install_smoke LANGUAGES CXX)

if(DEFINED GUNGNIR_STRUCTURED_DELIVERY_GENERATED)
    get_filename_component(delivery_generated_dir "${GUNGNIR_STRUCTURED_DELIVERY_GENERATED}" DIRECTORY)
    add_executable(gungnir_installed_delivery ../structured_delivery.cpp)
    target_include_directories(gungnir_installed_delivery PRIVATE "${delivery_generated_dir}")
    target_compile_features(gungnir_installed_delivery PRIVATE cxx_std_23)
    target_link_libraries(gungnir_installed_delivery PRIVATE gungnir::gungnir gungnir::orm gungnir::language)
endif()

find_package(Gungnir CONFIG REQUIRED)

if(DEFINED GUNGNIR_STRUCTURED_APPLICATION_GENERATED)
    get_filename_component(application_generated_dir "${GUNGNIR_STRUCTURED_APPLICATION_GENERATED}" DIRECTORY)
    add_executable(gungnir_installed_application ../structured_application.cpp)
    target_include_directories(gungnir_installed_application PRIVATE "${application_generated_dir}")
    target_compile_features(gungnir_installed_application PRIVATE cxx_std_23)
    target_link_libraries(gungnir_installed_application PRIVATE gungnir::gungnir gungnir::orm gungnir::language)
endif()

if(NOT Gungnir_RELEASE_VERSION STREQUAL "1.0.0")
    message(
        FATAL_ERROR
        "Installed Gungnir release identity is not the v1 release contract"
    )
endif()

if(NOT Gungnir_NATIVE_API_CONTRACT STREQUAL "1.0")
    message(
        FATAL_ERROR
        "Installed Gungnir native API contract is not the v1 release contract"
    )
endif()

if(NOT Gungnir_NATIVE_ABI_EPOCH STREQUAL "1")
    message(
        FATAL_ERROR
        "Installed Gungnir native ABI epoch is not epoch 1"
    )
endif()

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

if(DEFINED GUNGNIR_STRUCTURED_CONTEXT_GENERATED)
    get_filename_component(context_generated_dir "${GUNGNIR_STRUCTURED_CONTEXT_GENERATED}" DIRECTORY)
    add_executable(gungnir_installed_context ../structured_request_context.cpp)
    target_include_directories(gungnir_installed_context PRIVATE "${context_generated_dir}")
    target_compile_features(gungnir_installed_context PRIVATE cxx_std_23)
    target_link_libraries(gungnir_installed_context PRIVATE gungnir::gungnir gungnir::orm gungnir::language)
endif()

if(DEFINED GUNGNIR_STRUCTURED_HTTP_GENERATED)
    get_filename_component(http_generated_dir "${GUNGNIR_STRUCTURED_HTTP_GENERATED}" DIRECTORY)
    add_executable(gungnir_installed_http ../structured_http.cpp)
    target_include_directories(gungnir_installed_http PRIVATE "${http_generated_dir}")
    target_compile_features(gungnir_installed_http PRIVATE cxx_std_23)
    target_link_libraries(gungnir_installed_http PRIVATE gungnir::gungnir gungnir::orm gungnir::language)
endif()

if(DEFINED GUNGNIR_STRUCTURED_ORM_GENERATED)
    get_filename_component(orm_generated_dir "${GUNGNIR_STRUCTURED_ORM_GENERATED}" DIRECTORY)
    add_executable(gungnir_installed_orm_contract ../structured_orm_contract.cpp)
    target_include_directories(gungnir_installed_orm_contract PRIVATE "${orm_generated_dir}")
    target_compile_features(gungnir_installed_orm_contract PRIVATE cxx_std_23)
    target_link_libraries(gungnir_installed_orm_contract PRIVATE gungnir::gungnir gungnir::orm)
    if(TARGET SQLite::SQLite3 OR TARGET SQLite3::SQLite3)
        add_executable(gungnir_installed_orm ../structured_orm.cpp)
        target_include_directories(gungnir_installed_orm PRIVATE "${orm_generated_dir}")
        target_compile_features(gungnir_installed_orm PRIVATE cxx_std_23)
        target_link_libraries(gungnir_installed_orm PRIVATE gungnir::gungnir gungnir::orm)
    endif()
endif()

if(DEFINED GUNGNIR_STRUCTURED_AUTH_GENERATED)
    get_filename_component(authentication_generated_dir "${GUNGNIR_STRUCTURED_AUTH_GENERATED}" DIRECTORY)
    add_executable(gungnir_installed_authentication ../structured_authentication.cpp)
    target_include_directories(gungnir_installed_authentication PRIVATE "${authentication_generated_dir}")
    target_compile_features(gungnir_installed_authentication PRIVATE cxx_std_23)
    target_link_libraries(gungnir_installed_authentication PRIVATE gungnir::gungnir gungnir::orm gungnir::language)
endif()

if(DEFINED GUNGNIR_STRUCTURED_SERVICES_GENERATED)
    get_filename_component(services_generated_dir "${GUNGNIR_STRUCTURED_SERVICES_GENERATED}" DIRECTORY)
    add_executable(gungnir_installed_services ../structured_services.cpp)
    target_include_directories(gungnir_installed_services PRIVATE "${services_generated_dir}")
    target_compile_features(gungnir_installed_services PRIVATE cxx_std_23)
    target_link_libraries(gungnir_installed_services PRIVATE gungnir::gungnir gungnir::orm gungnir::language)
endif()

if(DEFINED GUNGNIR_STRUCTURED_ROUTING_GENERATED)
    get_filename_component(routing_generated_dir "${GUNGNIR_STRUCTURED_ROUTING_GENERATED}" DIRECTORY)
    add_executable(gungnir_installed_routing ../structured_routing.cpp)
    target_include_directories(gungnir_installed_routing PRIVATE "${routing_generated_dir}")
    target_compile_definitions(gungnir_installed_routing PRIVATE GUNGNIR_ROUTING_FIXTURE="${CMAKE_CURRENT_SOURCE_DIR}/../fixtures/structured/routing.gnr")
    target_compile_features(gungnir_installed_routing PRIVATE cxx_std_23)
    target_link_libraries(gungnir_installed_routing PRIVATE gungnir::gungnir gungnir::orm gungnir::language)
endif()

if(DEFINED GUNGNIR_STRUCTURED_VALIDATION_GENERATED)
    get_filename_component(validation_generated_dir "${GUNGNIR_STRUCTURED_VALIDATION_GENERATED}" DIRECTORY)
    add_executable(gungnir_installed_validation ../structured_validation.cpp)
    target_include_directories(gungnir_installed_validation PRIVATE "${validation_generated_dir}")
    target_compile_features(gungnir_installed_validation PRIVATE cxx_std_23)
    target_link_libraries(gungnir_installed_validation PRIVATE gungnir::gungnir gungnir::orm gungnir::language)
endif()

if(DEFINED GUNGNIR_STRUCTURED_BACKGROUND_GENERATED)
    get_filename_component(background_generated_dir "${GUNGNIR_STRUCTURED_BACKGROUND_GENERATED}" DIRECTORY)
    add_executable(gungnir_installed_background ../structured_background.cpp)
    target_include_directories(gungnir_installed_background PRIVATE "${background_generated_dir}")
    target_compile_features(gungnir_installed_background PRIVATE cxx_std_23)
    target_link_libraries(gungnir_installed_background PRIVATE gungnir::gungnir gungnir::orm gungnir::language)
endif()
