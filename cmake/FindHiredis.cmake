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

mark_as_advanced(
    Hiredis_INCLUDE_DIR
    Hiredis_LIBRARY
)
