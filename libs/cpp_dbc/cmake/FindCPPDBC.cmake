# FindCPPDBC.cmake
# Find the CPPDBC library
#
# This module defines the following variables:
# CPPDBC_FOUND - True if CPPDBC was found
# CPPDBC_INCLUDE_DIRS - CPPDBC include directories
# CPPDBC_LIBRARIES - CPPDBC libraries
#
# This module also defines the following imported targets:
# cpp_dbc::cpp_dbc - The CPPDBC library

# Try to find the package using the Config mode first
find_package(cpp_dbc QUIET CONFIG)

if(cpp_dbc_FOUND)
    # If found via config mode, set the variables for compatibility
    set(CPPDBC_FOUND TRUE)

    # We don't need to set CPPDBC_INCLUDE_DIRS or CPPDBC_LIBRARIES
    # as the imported target is already defined
    return()
endif()

# Find the include directory
find_path(CPPDBC_INCLUDE_DIR
    NAMES cpp_dbc/cpp_dbc.hpp
    PATH_SUFFIXES include
    DOC "CPPDBC include directory"
)

# Find the library
find_library(CPPDBC_LIBRARY
    NAMES cpp_dbc
    DOC "CPPDBC library"
)

# Set the CPPDBC_LIBRARIES variable
set(CPPDBC_LIBRARIES ${CPPDBC_LIBRARY})
set(CPPDBC_INCLUDE_DIRS ${CPPDBC_INCLUDE_DIR})

# Handle the QUIETLY and REQUIRED arguments and set CPPDBC_FOUND
include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(CPPDBC
    DEFAULT_MSG
    CPPDBC_LIBRARY CPPDBC_INCLUDE_DIR
)

# Create an imported target
if(CPPDBC_FOUND AND NOT TARGET cpp_dbc::cpp_dbc)
    add_library(cpp_dbc::cpp_dbc STATIC IMPORTED)
    set_target_properties(cpp_dbc::cpp_dbc PROPERTIES
        IMPORTED_LOCATION "${CPPDBC_LIBRARY}"
        INTERFACE_INCLUDE_DIRECTORIES "${CPPDBC_INCLUDE_DIR}"
    )

    # Try to determine which database drivers are enabled
    # This is a best-effort approach since we don't have the config file

    # Check for MySQL
    find_package(MySQL QUIET)

    if(MySQL_FOUND)
        target_link_libraries(cpp_dbc::cpp_dbc INTERFACE ${MYSQL_LIBRARIES})
        target_include_directories(cpp_dbc::cpp_dbc INTERFACE ${MYSQL_INCLUDE_DIR})
        target_compile_definitions(cpp_dbc::cpp_dbc INTERFACE USE_MYSQL=1)
    else()
        target_compile_definitions(cpp_dbc::cpp_dbc INTERFACE USE_MYSQL=0)
    endif()

    # Check for PostgreSQL
    find_package(PostgreSQL QUIET)

    if(PostgreSQL_FOUND)
        target_link_libraries(cpp_dbc::cpp_dbc INTERFACE ${PostgreSQL_LIBRARIES})
        target_include_directories(cpp_dbc::cpp_dbc INTERFACE ${PostgreSQL_INCLUDE_DIRS})
        target_compile_definitions(cpp_dbc::cpp_dbc INTERFACE USE_POSTGRESQL=1)
    else()
        target_compile_definitions(cpp_dbc::cpp_dbc INTERFACE USE_POSTGRESQL=0)
    endif()

    # Check for SQLite
    find_package(SQLite3 QUIET)

    if(SQLite3_FOUND)
        target_link_libraries(cpp_dbc::cpp_dbc INTERFACE ${SQLite3_LIBRARIES})
        target_include_directories(cpp_dbc::cpp_dbc INTERFACE ${SQLite3_INCLUDE_DIRS})
        target_compile_definitions(cpp_dbc::cpp_dbc INTERFACE USE_SQLITE=1)
    else()
        target_compile_definitions(cpp_dbc::cpp_dbc INTERFACE USE_SQLITE=0)
    endif()

    # Check for yaml-cpp
    find_package(yaml-cpp QUIET)

    if(yaml-cpp_FOUND)
        target_link_libraries(cpp_dbc::cpp_dbc INTERFACE yaml-cpp::yaml-cpp)
        target_compile_definitions(cpp_dbc::cpp_dbc INTERFACE USE_CPP_YAML=1)
    else()
        target_compile_definitions(cpp_dbc::cpp_dbc INTERFACE USE_CPP_YAML=0)
    endif()

    # Check for libdw
    find_library(LIBDW_LIBRARY NAMES dw)

    if(LIBDW_LIBRARY)
        target_link_libraries(cpp_dbc::cpp_dbc INTERFACE ${LIBDW_LIBRARY})
        target_compile_definitions(cpp_dbc::cpp_dbc INTERFACE BACKWARD_HAS_DW=1)
    else()
        target_compile_definitions(cpp_dbc::cpp_dbc INTERFACE BACKWARD_HAS_DW=0)
    endif()
endif()

# Mark variables as advanced
mark_as_advanced(CPPDBC_INCLUDE_DIR CPPDBC_LIBRARY)