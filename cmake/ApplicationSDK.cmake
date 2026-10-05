# Install only the dependencies used by the application profile. Imported SDK
# targets have their own names so a consumer's other OpenSSL/SQLite targets do
# not replace the libraries that this Gungnir build was linked against.
set(GUNGNIR_SDK_PROFILE "core")
foreach(feature S3 SQLITE PASSWORD POSTGRESQL MYSQL SQLSERVER MONGODB REDIS SMTP OTLP TLS HTTP2)
    if(GUNGNIR_WITH_${feature})
        set(GUNGNIR_SDK_PROFILE "custom")
    endif()
endforeach()
if(GUNGNIR_APPLICATION_SDK)
    set(GUNGNIR_SDK_PROFILE "application")
    if(BUILD_SHARED_LIBS)
        message(FATAL_ERROR "The application SDK packages static Gungnir libraries. Set BUILD_SHARED_LIBS=OFF.")
    endif()
    if(NOT WIN32 AND NOT CMAKE_SYSTEM_NAME STREQUAL "Linux")
        message(FATAL_ERROR "Bundled application SDKs currently support Windows and Linux. Build a custom SDK on other platforms.")
    endif()

    get_target_property(gungnir_crypto_library OpenSSL::Crypto IMPORTED_LOCATION_RELEASE)
    if(NOT gungnir_crypto_library)
        get_target_property(gungnir_crypto_library OpenSSL::Crypto IMPORTED_LOCATION)
    endif()
    foreach(library IN ITEMS "${SQLite3_LIBRARY}" "${gungnir_crypto_library}")
        if(NOT EXISTS "${library}" OR NOT library MATCHES "\\${CMAKE_STATIC_LIBRARY_SUFFIX}$")
            message(FATAL_ERROR "Application SDK requires static SQLite and OpenSSL Crypto libraries. Use a fresh build directory and the application dependency recipe in docs/sdk-packages.md.")
        endif()
    endforeach()
    get_target_property(GUNGNIR_SDK_CRYPTO_LINK_LIBRARIES OpenSSL::Crypto INTERFACE_LINK_LIBRARIES)
    if(NOT GUNGNIR_SDK_CRYPTO_LINK_LIBRARIES)
        set(GUNGNIR_SDK_CRYPTO_LINK_LIBRARIES "")
    endif()
    foreach(dependency IN LISTS GUNGNIR_SDK_CRYPTO_LINK_LIBRARIES)
        if(NOT dependency MATCHES "^(Threads::Threads|dl|pthread|ws2_32|crypt32|bcrypt|advapi32|user32|gdi32|m|atomic|-pthread)$")
            message(FATAL_ERROR "OpenSSL Crypto requires an unbundled dependency '${dependency}'. Use the pinned application dependency recipe or GUNGNIR_APPLICATION_SDK=OFF.")
        endif()
    endforeach()
    # SQLite's static Unix build can use dlopen and pthreads. These are host
    # platform dependencies, not files from the SDK producer's machine.
    set(GUNGNIR_SDK_SQLITE_LINK_LIBRARIES "Threads::Threads;${CMAKE_DL_LIBS}")

    file(REAL_PATH "${SQLite3_LIBRARY}" gungnir_sqlite_library)
    file(REAL_PATH "${gungnir_crypto_library}" gungnir_crypto_library)
    get_filename_component(GUNGNIR_SDK_SQLITE_LIBRARY_NAME "${gungnir_sqlite_library}" NAME)
    get_filename_component(GUNGNIR_SDK_CRYPTO_LIBRARY_NAME "${gungnir_crypto_library}" NAME)
    set(GUNGNIR_SDK_INCLUDE_DIR "${CMAKE_INSTALL_INCLUDEDIR}/gungnir/vendor")
    set(GUNGNIR_SDK_LIBRARY_DIR "${CMAKE_INSTALL_LIBDIR}/gungnir/vendor")
    install(FILES "${gungnir_sqlite_library}" "${gungnir_crypto_library}" DESTINATION "${GUNGNIR_SDK_LIBRARY_DIR}")
    install(FILES "${SQLite3_INCLUDE_DIR}/sqlite3.h" "${SQLite3_INCLUDE_DIR}/sqlite3ext.h" DESTINATION "${GUNGNIR_SDK_INCLUDE_DIR}")
    install(DIRECTORY "${OPENSSL_INCLUDE_DIR}/openssl" DESTINATION "${GUNGNIR_SDK_INCLUDE_DIR}")
    # Debian/Ubuntu split generated configuration headers into a multiarch
    # include directory. Install them explicitly so consumers need no host
    # OpenSSL development headers, including for native interoperability.
    foreach(header opensslconf.h configuration.h)
        string(MAKE_C_IDENTIFIER "gungnir_openssl_${header}_include" include_variable)
        find_path(${include_variable} NAMES "openssl/${header}"
            HINTS "${OPENSSL_INCLUDE_DIR}" ${CMAKE_CXX_IMPLICIT_INCLUDE_DIRECTORIES})
        if(NOT ${include_variable})
            message(FATAL_ERROR "Application SDK requires OpenSSL's generated ${header} header.")
        endif()
        file(REAL_PATH "${${include_variable}}/openssl/${header}" generated_header)
        install(FILES "${generated_header}" DESTINATION "${GUNGNIR_SDK_INCLUDE_DIR}/openssl" RENAME "${header}")
    endforeach()

    set(GUNGNIR_OPENSSL_NOTICE "" CACHE FILEPATH "License/notice file for the packaged OpenSSL build")
    set(GUNGNIR_SQLITE_NOTICE "" CACHE FILEPATH "License/notice file for the packaged SQLite build")
    if(NOT GUNGNIR_OPENSSL_NOTICE)
        find_file(gungnir_openssl_notice NAMES copyright LICENSE.txt LICENSE
            HINTS "${OPENSSL_INCLUDE_DIR}/../share/openssl" "/usr/share/doc/libssl-dev" NO_DEFAULT_PATH)
        set(GUNGNIR_OPENSSL_NOTICE "${gungnir_openssl_notice}")
    endif()
    if(NOT GUNGNIR_SQLITE_NOTICE)
        find_file(gungnir_sqlite_notice NAMES copyright LICENSE.txt LICENSE
            HINTS "${SQLite3_INCLUDE_DIR}/../share/sqlite3" "${SQLite3_INCLUDE_DIR}/../share/doc/libsqlite3-dev" "/usr/share/doc/libsqlite3-dev" NO_DEFAULT_PATH)
        set(GUNGNIR_SQLITE_NOTICE "${gungnir_sqlite_notice}")
    endif()
    foreach(notice IN ITEMS OPENSSL SQLITE)
        if(NOT EXISTS "${GUNGNIR_${notice}_NOTICE}")
            message(FATAL_ERROR "Set GUNGNIR_${notice}_NOTICE to the license/notice file for the static dependency being packaged.")
        endif()
        file(REAL_PATH "${GUNGNIR_${notice}_NOTICE}" GUNGNIR_${notice}_NOTICE)
        install(FILES "${GUNGNIR_${notice}_NOTICE}" DESTINATION "${CMAKE_INSTALL_DATADIR}/gungnir/licenses" RENAME "${notice}.txt")
    endforeach()
    configure_package_config_file(
        "${CMAKE_CURRENT_SOURCE_DIR}/cmake/GungnirApplicationDependencies.cmake.in"
        "${CMAKE_CURRENT_BINARY_DIR}/GungnirApplicationDependencies.cmake"
        INSTALL_DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/Gungnir"
        PATH_VARS GUNGNIR_SDK_INCLUDE_DIR GUNGNIR_SDK_LIBRARY_DIR)
    install(FILES "${CMAKE_CURRENT_BINARY_DIR}/GungnirApplicationDependencies.cmake"
        DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/Gungnir")
endif()

configure_file("${CMAKE_CURRENT_SOURCE_DIR}/cmake/sdk.txt.in" "${CMAKE_CURRENT_BINARY_DIR}/sdk.txt" @ONLY)
install(FILES "${CMAKE_CURRENT_BINARY_DIR}/sdk.txt" DESTINATION "${CMAKE_INSTALL_DATADIR}/gungnir")
