# FindSQLite3.cmake
# Find the SQLite3 includes and library
#
# This module defines:
# SQLite3_INCLUDE_DIRS - where to find sqlite3.h
# SQLite3_LIBRARIES    - the libraries to link against to use SQLite3
# SQLite3_FOUND        - if the library was successfully found
#
# To help locate the library and include file, you can define a
# variable called SQLITE3_ROOT which points to the root of the SQLite3
# library installation.
#

# Check if we can use pkg-config
find_package(PkgConfig QUIET)

# Use pkg-config to get hints about paths
if(PKG_CONFIG_FOUND)
    pkg_check_modules(SQLITE3_PKGCONF QUIET sqlite3)
endif()

# Include dir
find_path(SQLite3_INCLUDE_DIR
    NAMES sqlite3.h
    HINTS
    ${SQLITE3_PKGCONF_INCLUDE_DIRS}
    ${SQLITE3_ROOT}/include
    PATH_SUFFIXES
    sqlite3
)

# Library
find_library(SQLite3_LIBRARY
    NAMES sqlite3 sqlite3_i
    HINTS
    ${SQLITE3_PKGCONF_LIBRARY_DIRS}
    ${SQLITE3_ROOT}/lib
)

# Set the include dir variables and the libraries
include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(SQLite3
    REQUIRED_VARS SQLite3_INCLUDE_DIR SQLite3_LIBRARY
)

# Set the output variables
if(SQLite3_FOUND)
    set(SQLite3_INCLUDE_DIRS ${SQLite3_INCLUDE_DIR})
    set(SQLite3_LIBRARIES ${SQLite3_LIBRARY})
endif()

# Hide these variables in cmake GUIs
mark_as_advanced(
    SQLite3_INCLUDE_DIR
    SQLite3_LIBRARY
)