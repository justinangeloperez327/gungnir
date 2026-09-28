find_path(
    Hiredis_INCLUDE_DIR
    NAMES
        hiredis/hiredis.h
)

find_library(
    Hiredis_LIBRARY
    NAMES
        hiredis
)

find_path(
    Hiredis_SSL_INCLUDE_DIR
    NAMES
        hiredis/hiredis_ssl.h
)

find_library(
    Hiredis_SSL_LIBRARY
    NAMES
        hiredis_ssl
)

include(FindPackageHandleStandardArgs)

find_package_handle_standard_args(
    Hiredis
    REQUIRED_VARS
        Hiredis_LIBRARY
        Hiredis_INCLUDE_DIR
)

if(
    Hiredis_FOUND AND
    NOT TARGET Hiredis::Hiredis
)
    add_library(
        Hiredis::Hiredis
        UNKNOWN
        IMPORTED
    )

    set_target_properties(
        Hiredis::Hiredis
        PROPERTIES
            IMPORTED_LOCATION
                "${Hiredis_LIBRARY}"
            INTERFACE_INCLUDE_DIRECTORIES
                "${Hiredis_INCLUDE_DIR}"
    )
endif()

if(
    Hiredis_FOUND AND
    Hiredis_SSL_LIBRARY AND
    Hiredis_SSL_INCLUDE_DIR AND
    NOT TARGET Hiredis::SSL
)
    add_library(
        Hiredis::SSL
        UNKNOWN
        IMPORTED
    )

    set_target_properties(
        Hiredis::SSL
        PROPERTIES
            IMPORTED_LOCATION
                "${Hiredis_SSL_LIBRARY}"
            INTERFACE_INCLUDE_DIRECTORIES
                "${Hiredis_SSL_INCLUDE_DIR}"
            INTERFACE_LINK_LIBRARIES
                "Hiredis::Hiredis"
    )

    set(
        Hiredis_SSL_FOUND
        TRUE
    )
endif()

mark_as_advanced(
    Hiredis_INCLUDE_DIR
    Hiredis_LIBRARY
    Hiredis_SSL_INCLUDE_DIR
    Hiredis_SSL_LIBRARY
)
