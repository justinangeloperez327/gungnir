find_path(
    MariaDB_INCLUDE_DIR
    NAMES
        mysql.h
    PATH_SUFFIXES
        mariadb
        mysql
)

find_library(
    MariaDB_LIBRARY
    NAMES
        mariadb
        libmariadb
        mysqlclient
)

include(FindPackageHandleStandardArgs)

find_package_handle_standard_args(
    MariaDB
    REQUIRED_VARS
        MariaDB_LIBRARY
        MariaDB_INCLUDE_DIR
)

if(
    MariaDB_FOUND AND
    NOT TARGET MariaDB::MariaDB
)
    add_library(
        MariaDB::MariaDB
        UNKNOWN
        IMPORTED
    )

    set_target_properties(
        MariaDB::MariaDB
        PROPERTIES
            IMPORTED_LOCATION
                "${MariaDB_LIBRARY}"
            INTERFACE_INCLUDE_DIRECTORIES
                "${MariaDB_INCLUDE_DIR}"
    )
endif()

mark_as_advanced(
    MariaDB_INCLUDE_DIR
    MariaDB_LIBRARY
)
