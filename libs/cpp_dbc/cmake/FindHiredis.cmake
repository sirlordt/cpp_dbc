# FindHiredis.cmake
# Finds the hiredis library
#
# This will define the following variables:
# HIREDIS_FOUND        - True if the system has the hiredis library
# HIREDIS_INCLUDE_DIRS - The include directories for hiredis
# HIREDIS_LIBRARIES    - The libraries needed to use hiredis
#
# and the following imported targets:
# Hiredis::Hiredis - The hiredis library

# Find the include directory
find_path(HIREDIS_INCLUDE_DIR
    NAMES hiredis/hiredis.h
    PATHS /usr/include /usr/local/include
)

# Find the library
find_library(HIREDIS_LIBRARY
    NAMES hiredis
    PATHS /usr/lib /usr/local/lib
)

# Set the include dirs and libraries
set(HIREDIS_INCLUDE_DIRS ${HIREDIS_INCLUDE_DIR})
set(HIREDIS_LIBRARIES ${HIREDIS_LIBRARY})

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(Hiredis
    REQUIRED_VARS HIREDIS_LIBRARY HIREDIS_INCLUDE_DIR
)

# Create an imported target
if(HIREDIS_FOUND AND NOT TARGET Hiredis::Hiredis)
    add_library(Hiredis::Hiredis UNKNOWN IMPORTED)
    set_target_properties(Hiredis::Hiredis PROPERTIES
        IMPORTED_LOCATION "${HIREDIS_LIBRARY}"
        INTERFACE_INCLUDE_DIRECTORIES "${HIREDIS_INCLUDE_DIR}"
    )
endif()

mark_as_advanced(HIREDIS_INCLUDE_DIR HIREDIS_LIBRARY)