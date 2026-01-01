# FindPostgreSQL.cmake
# Find the PostgreSQL client libraries
#
# This module defines the following variables:
# PostgreSQL_FOUND - True if PostgreSQL was found
# PostgreSQL_INCLUDE_DIRS - PostgreSQL include directories
# PostgreSQL_LIBRARIES - PostgreSQL libraries
#
# This module also defines the following imported targets:
# PostgreSQL::PostgreSQL - The PostgreSQL library

# Find the include directory
find_path(PostgreSQL_INCLUDE_DIR
    NAMES libpq-fe.h
    PATH_SUFFIXES postgresql pgsql include
    DOC "PostgreSQL include directory"
)

# Find the library
find_library(PostgreSQL_LIBRARY
    NAMES pq libpq
    DOC "PostgreSQL client library"
)

# Set the PostgreSQL_LIBRARIES variable
set(PostgreSQL_LIBRARIES ${PostgreSQL_LIBRARY})
set(PostgreSQL_INCLUDE_DIRS ${PostgreSQL_INCLUDE_DIR})

# Handle the QUIETLY and REQUIRED arguments and set PostgreSQL_FOUND
include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(cpp_dbc
    DEFAULT_MSG
    PostgreSQL_LIBRARY PostgreSQL_INCLUDE_DIR
)

# Set PostgreSQL_FOUND for compatibility
set(PostgreSQL_FOUND ${cpp_dbc_FOUND})

# Create an imported target
if(PostgreSQL_FOUND AND NOT TARGET PostgreSQL::PostgreSQL)
    add_library(PostgreSQL::PostgreSQL UNKNOWN IMPORTED)
    set_target_properties(PostgreSQL::PostgreSQL PROPERTIES
        IMPORTED_LOCATION "${PostgreSQL_LIBRARY}"
        INTERFACE_INCLUDE_DIRECTORIES "${PostgreSQL_INCLUDE_DIR}"
    )
endif()

# Mark variables as advanced
mark_as_advanced(PostgreSQL_INCLUDE_DIR PostgreSQL_LIBRARY)