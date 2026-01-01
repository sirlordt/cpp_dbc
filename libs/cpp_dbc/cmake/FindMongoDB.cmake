# FindMongoDB.cmake
# Find the MongoDB C driver library (libmongoc and libbson)
#
# This module defines:
# MONGODB_FOUND - True if MongoDB C driver was found
# MONGODB_INCLUDE_DIRS - Include directories for MongoDB
# MONGODB_LIBRARIES - Libraries to link against
# MONGODB_VERSION - Version of MongoDB C driver found (if available)
#
# Required system packages:
# Debian/Ubuntu: sudo apt-get install libmongoc-dev libbson-dev
# RHEL/CentOS/Fedora: sudo dnf install mongo-c-driver-devel libbson-devel
# Arch Linux: sudo pacman -S mongo-c-driver
# macOS: brew install mongo-c-driver
#
# The following variables can be set to customize the search:
# MONGODB_ROOT - Root directory of MongoDB C driver installation
# MONGODB_INCLUDE_DIR - Directory containing mongoc.h
# MONGODB_LIBRARY - Path to the mongoc library
# BSON_INCLUDE_DIR - Directory containing bson.h
# BSON_LIBRARY - Path to the bson library

# Try to find MongoDB using pkg-config first
find_package(PkgConfig QUIET)

if(PKG_CONFIG_FOUND)
    pkg_check_modules(PC_MONGOC QUIET libmongoc-1.0)
    pkg_check_modules(PC_BSON QUIET libbson-1.0)
endif()

# Find the mongoc include directory
find_path(MONGOC_INCLUDE_DIR
    NAMES mongoc/mongoc.h
    HINTS
    ${MONGODB_ROOT}
    ${PC_MONGOC_INCLUDEDIR}
    ${PC_MONGOC_INCLUDE_DIRS}
    PATH_SUFFIXES
    include
    include/libmongoc-1.0
    libmongoc-1.0
    PATHS
    /usr/include
    /usr/local/include
    /opt/mongo-c-driver/include
    /opt/local/include
    /usr/include/libmongoc-1.0
    /usr/local/include/libmongoc-1.0
    $ENV{MONGODB_HOME}/include
)

# Find the bson include directory
find_path(BSON_INCLUDE_DIR
    NAMES bson/bson.h
    HINTS
    ${MONGODB_ROOT}
    ${PC_BSON_INCLUDEDIR}
    ${PC_BSON_INCLUDE_DIRS}
    PATH_SUFFIXES
    include
    include/libbson-1.0
    libbson-1.0
    PATHS
    /usr/include
    /usr/local/include
    /opt/mongo-c-driver/include
    /opt/local/include
    /usr/include/libbson-1.0
    /usr/local/include/libbson-1.0
    $ENV{MONGODB_HOME}/include
)

# Find the mongoc library
find_library(MONGOC_LIBRARY
    NAMES mongoc-1.0 mongoc
    HINTS
    ${MONGODB_ROOT}
    ${PC_MONGOC_LIBDIR}
    ${PC_MONGOC_LIBRARY_DIRS}
    PATH_SUFFIXES
    lib
    lib64
    lib/x86_64-linux-gnu
    lib/i386-linux-gnu
    PATHS
    /usr/lib
    /usr/local/lib
    /opt/mongo-c-driver/lib
    /opt/local/lib
    /usr/lib/x86_64-linux-gnu
    /usr/lib/i386-linux-gnu
    $ENV{MONGODB_HOME}/lib
)

# Find the bson library
find_library(BSON_LIBRARY
    NAMES bson-1.0 bson
    HINTS
    ${MONGODB_ROOT}
    ${PC_BSON_LIBDIR}
    ${PC_BSON_LIBRARY_DIRS}
    PATH_SUFFIXES
    lib
    lib64
    lib/x86_64-linux-gnu
    lib/i386-linux-gnu
    PATHS
    /usr/lib
    /usr/local/lib
    /opt/mongo-c-driver/lib
    /opt/local/lib
    /usr/lib/x86_64-linux-gnu
    /usr/lib/i386-linux-gnu
    $ENV{MONGODB_HOME}/lib
)

# Get version from pkg-config if available
if(PC_MONGOC_VERSION)
    set(MONGODB_VERSION ${PC_MONGOC_VERSION})
else()
    # Try to extract version from header file
    if(MONGOC_INCLUDE_DIR AND EXISTS "${MONGOC_INCLUDE_DIR}/mongoc/mongoc-version.h")
        file(STRINGS "${MONGOC_INCLUDE_DIR}/mongoc/mongoc-version.h" MONGOC_VERSION_MAJOR_LINE
            REGEX "^#define[ \t]+MONGOC_MAJOR_VERSION[ \t]+[0-9]+")
        file(STRINGS "${MONGOC_INCLUDE_DIR}/mongoc/mongoc-version.h" MONGOC_VERSION_MINOR_LINE
            REGEX "^#define[ \t]+MONGOC_MINOR_VERSION[ \t]+[0-9]+")
        file(STRINGS "${MONGOC_INCLUDE_DIR}/mongoc/mongoc-version.h" MONGOC_VERSION_MICRO_LINE
            REGEX "^#define[ \t]+MONGOC_MICRO_VERSION[ \t]+[0-9]+")

        if(MONGOC_VERSION_MAJOR_LINE AND MONGOC_VERSION_MINOR_LINE AND MONGOC_VERSION_MICRO_LINE)
            string(REGEX REPLACE "^#define[ \t]+MONGOC_MAJOR_VERSION[ \t]+([0-9]+).*$" "\\1"
                MONGOC_VERSION_MAJOR "${MONGOC_VERSION_MAJOR_LINE}")
            string(REGEX REPLACE "^#define[ \t]+MONGOC_MINOR_VERSION[ \t]+([0-9]+).*$" "\\1"
                MONGOC_VERSION_MINOR "${MONGOC_VERSION_MINOR_LINE}")
            string(REGEX REPLACE "^#define[ \t]+MONGOC_MICRO_VERSION[ \t]+([0-9]+).*$" "\\1"
                MONGOC_VERSION_MICRO "${MONGOC_VERSION_MICRO_LINE}")
            set(MONGODB_VERSION "${MONGOC_VERSION_MAJOR}.${MONGOC_VERSION_MINOR}.${MONGOC_VERSION_MICRO}")
        endif()
    endif()
endif()

# Handle the QUIETLY and REQUIRED arguments
include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(MongoDB
    REQUIRED_VARS MONGOC_LIBRARY BSON_LIBRARY MONGOC_INCLUDE_DIR BSON_INCLUDE_DIR
    VERSION_VAR MONGODB_VERSION
)

# Set output variables
if(MONGODB_FOUND)
    set(MONGODB_LIBRARIES ${MONGOC_LIBRARY} ${BSON_LIBRARY})
    set(MONGODB_INCLUDE_DIRS ${MONGOC_INCLUDE_DIR} ${BSON_INCLUDE_DIR})

    # Create imported target for mongoc
    if(NOT TARGET MongoDB::mongoc)
        add_library(MongoDB::mongoc UNKNOWN IMPORTED)
        set_target_properties(MongoDB::mongoc PROPERTIES
            IMPORTED_LOCATION "${MONGOC_LIBRARY}"
            INTERFACE_INCLUDE_DIRECTORIES "${MONGOC_INCLUDE_DIR}"
        )
    endif()

    # Create imported target for bson
    if(NOT TARGET MongoDB::bson)
        add_library(MongoDB::bson UNKNOWN IMPORTED)
        set_target_properties(MongoDB::bson PROPERTIES
            IMPORTED_LOCATION "${BSON_LIBRARY}"
            INTERFACE_INCLUDE_DIRECTORIES "${BSON_INCLUDE_DIR}"
        )
    endif()

    # Create combined imported target
    if(NOT TARGET MongoDB::MongoDB)
        add_library(MongoDB::MongoDB INTERFACE IMPORTED)
        set_target_properties(MongoDB::MongoDB PROPERTIES
            INTERFACE_LINK_LIBRARIES "MongoDB::mongoc;MongoDB::bson"
        )
    endif()
endif()

# Mark as advanced
mark_as_advanced(MONGOC_INCLUDE_DIR MONGOC_LIBRARY BSON_INCLUDE_DIR BSON_LIBRARY)