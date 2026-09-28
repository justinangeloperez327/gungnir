include(FindPackageHandleStandardArgs)

find_package(PkgConfig QUIET)

if(PkgConfig_FOUND)
    pkg_check_modules(
        PC_NGHTTP2
        QUIET
        libnghttp2
    )
endif()

find_path(
    NGHTTP2_INCLUDE_DIR
    NAMES
        nghttp2/nghttp2.h
    HINTS
        ${PC_NGHTTP2_INCLUDE_DIRS}
)

find_library(
    NGHTTP2_LIBRARY
    NAMES
        nghttp2
    HINTS
        ${PC_NGHTTP2_LIBRARY_DIRS}
)

set(
    NGHTTP2_VERSION
    "${PC_NGHTTP2_VERSION}"
)

find_package_handle_standard_args(
    Nghttp2
    REQUIRED_VARS
        NGHTTP2_LIBRARY
        NGHTTP2_INCLUDE_DIR
    VERSION_VAR
        NGHTTP2_VERSION
)

if(
    Nghttp2_FOUND AND
    NOT TARGET Nghttp2::Nghttp2
)
    add_library(
        Nghttp2::Nghttp2
        UNKNOWN
        IMPORTED
    )

    set_target_properties(
        Nghttp2::Nghttp2
        PROPERTIES
            IMPORTED_LOCATION
                "${NGHTTP2_LIBRARY}"
            INTERFACE_INCLUDE_DIRECTORIES
                "${NGHTTP2_INCLUDE_DIR}"
    )
endif()

mark_as_advanced(
    NGHTTP2_INCLUDE_DIR
    NGHTTP2_LIBRARY
)
