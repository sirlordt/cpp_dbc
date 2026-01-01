# FindMySQL.cmake
# Find the MySQL client libraries
#
# This module defines the following variables:
# MYSQL_FOUND - True if MySQL was found
# MYSQL_INCLUDE_DIR - MySQL include directories
# MYSQL_LIBRARIES - MySQL libraries
#
# This module also defines the following imported targets:
# MySQL::MySQL - The MySQL library

# Find the include directory
find_path(MYSQL_INCLUDE_DIR
    NAMES mysql.h
    PATH_SUFFIXES mysql
    DOC "MySQL include directory"
)

# Find the library
find_library(MYSQL_LIBRARY
    NAMES mysqlclient
    DOC "MySQL client library"
)

# Set the MYSQL_LIBRARIES variable
set(MYSQL_LIBRARIES ${MYSQL_LIBRARY})

# Handle the QUIETLY and REQUIRED arguments and set MYSQL_FOUND
include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(cpp_dbc
    DEFAULT_MSG
    MYSQL_LIBRARY MYSQL_INCLUDE_DIR
)

# Set MySQL_FOUND for compatibility
set(MySQL_FOUND ${cpp_dbc_FOUND})

# Create an imported target
if(MYSQL_FOUND AND NOT TARGET MySQL::MySQL)
    add_library(MySQL::MySQL UNKNOWN IMPORTED)
    set_target_properties(MySQL::MySQL PROPERTIES
        IMPORTED_LOCATION "${MYSQL_LIBRARY}"
        INTERFACE_INCLUDE_DIRECTORIES "${MYSQL_INCLUDE_DIR}"
    )
endif()

# Mark variables as advanced
mark_as_advanced(MYSQL_INCLUDE_DIR MYSQL_LIBRARY)