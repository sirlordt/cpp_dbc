# FindFirebird.cmake
# Find the Firebird SQL database client library
#
# This module defines:
# FIREBIRD_FOUND - True if Firebird was found
# FIREBIRD_INCLUDE_DIRS - Include directories for Firebird
# FIREBIRD_LIBRARIES - Libraries to link against
# FIREBIRD_VERSION - Version of Firebird found (if available)
#
# Required system packages:
# Debian/Ubuntu: sudo apt-get install firebird-dev libfbclient2
# RHEL/CentOS/Fedora: sudo dnf install firebird-devel libfbclient2
# Arch Linux: sudo pacman -S firebird
# macOS: brew install firebird
#
# The following variables can be set to customize the search:
# FIREBIRD_ROOT - Root directory of Firebird installation
# FIREBIRD_INCLUDE_DIR - Directory containing ibase.h
# FIREBIRD_LIBRARY - Path to the fbclient library

# Try to find Firebird using pkg-config first
find_package(PkgConfig QUIET)

if(PKG_CONFIG_FOUND)
    pkg_check_modules(PC_FIREBIRD QUIET fbclient)
endif()

# Find the include directory
find_path(FIREBIRD_INCLUDE_DIR
    NAMES ibase.h
    HINTS
    ${FIREBIRD_ROOT}
    ${PC_FIREBIRD_INCLUDEDIR}
    ${PC_FIREBIRD_INCLUDE_DIRS}
    PATH_SUFFIXES
    include
    include/firebird
    firebird
    PATHS
    /usr/include
    /usr/local/include
    /opt/firebird/include
    /opt/local/include
    /usr/include/firebird
    /usr/local/include/firebird
    $ENV{FIREBIRD_HOME}/include
)

# Find the library
find_library(FIREBIRD_LIBRARY
    NAMES fbclient fbclient_ms gds
    HINTS
    ${FIREBIRD_ROOT}
    ${PC_FIREBIRD_LIBDIR}
    ${PC_FIREBIRD_LIBRARY_DIRS}
    PATH_SUFFIXES
    lib
    lib64
    lib/x86_64-linux-gnu
    lib/i386-linux-gnu
    PATHS
    /usr/lib
    /usr/local/lib
    /opt/firebird/lib
    /opt/local/lib
    /usr/lib/x86_64-linux-gnu
    /usr/lib/i386-linux-gnu
    $ENV{FIREBIRD_HOME}/lib
)

# Get version from pkg-config if available
if(PC_FIREBIRD_VERSION)
    set(FIREBIRD_VERSION ${PC_FIREBIRD_VERSION})
else()
    # Try to extract version from header file
    if(FIREBIRD_INCLUDE_DIR AND EXISTS "${FIREBIRD_INCLUDE_DIR}/ibase.h")
        file(STRINGS "${FIREBIRD_INCLUDE_DIR}/ibase.h" FIREBIRD_VERSION_LINE
            REGEX "^#define[ \t]+FB_API_VER[ \t]+[0-9]+")

        if(FIREBIRD_VERSION_LINE)
            string(REGEX REPLACE "^#define[ \t]+FB_API_VER[ \t]+([0-9]+).*$" "\\1"
                FIREBIRD_VERSION_NUMBER "${FIREBIRD_VERSION_LINE}")

            # Convert API version to human-readable version
            if(FIREBIRD_VERSION_NUMBER GREATER_EQUAL 40)
                set(FIREBIRD_VERSION "4.0")
            elseif(FIREBIRD_VERSION_NUMBER GREATER_EQUAL 30)
                set(FIREBIRD_VERSION "3.0")
            elseif(FIREBIRD_VERSION_NUMBER GREATER_EQUAL 25)
                set(FIREBIRD_VERSION "2.5")
            else()
                set(FIREBIRD_VERSION "2.x")
            endif()
        endif()
    endif()
endif()

# Handle the QUIETLY and REQUIRED arguments
include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(Firebird
    REQUIRED_VARS FIREBIRD_LIBRARY FIREBIRD_INCLUDE_DIR
    VERSION_VAR FIREBIRD_VERSION
)

# Set output variables
if(FIREBIRD_FOUND)
    set(FIREBIRD_LIBRARIES ${FIREBIRD_LIBRARY})
    set(FIREBIRD_INCLUDE_DIRS ${FIREBIRD_INCLUDE_DIR})

    # Create imported target
    if(NOT TARGET Firebird::Firebird)
        add_library(Firebird::Firebird UNKNOWN IMPORTED)
        set_target_properties(Firebird::Firebird PROPERTIES
            IMPORTED_LOCATION "${FIREBIRD_LIBRARY}"
            INTERFACE_INCLUDE_DIRECTORIES "${FIREBIRD_INCLUDE_DIR}"
        )
    endif()
endif()

# Mark as advanced
mark_as_advanced(FIREBIRD_INCLUDE_DIR FIREBIRD_LIBRARY)