# FindCassandra.cmake
# Find the Cassandra/ScyllaDB C driver libraries
#
# This module defines the following variables:
# CASSANDRA_FOUND - True if Cassandra driver was found
# CASSANDRA_INCLUDE_DIR - Cassandra include directories
# CASSANDRA_LIBRARIES - Cassandra libraries
#
# This module also defines the following imported targets:
# Cassandra::Cassandra - The Cassandra library

# Find the include directory
find_path(CASSANDRA_INCLUDE_DIR
    NAMES cassandra.h
    PATH_SUFFIXES cassandra
    DOC "Cassandra include directory"
)

# Find the library
find_library(CASSANDRA_LIBRARY
    NAMES cassandra
    DOC "Cassandra client library"
)

# Set the CASSANDRA_LIBRARIES variable
set(CASSANDRA_LIBRARIES ${CASSANDRA_LIBRARY})

# Handle the QUIETLY and REQUIRED arguments and set CASSANDRA_FOUND
include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(Cassandra
    DEFAULT_MSG
    CASSANDRA_LIBRARY CASSANDRA_INCLUDE_DIR
)

# Create an imported target
if(CASSANDRA_FOUND AND NOT TARGET Cassandra::Cassandra)
    add_library(Cassandra::Cassandra UNKNOWN IMPORTED)
    set_target_properties(Cassandra::Cassandra PROPERTIES
        IMPORTED_LOCATION "${CASSANDRA_LIBRARY}"
        INTERFACE_INCLUDE_DIRECTORIES "${CASSANDRA_INCLUDE_DIR}"
    )
endif()

# Mark variables as advanced
mark_as_advanced(CASSANDRA_INCLUDE_DIR CASSANDRA_LIBRARY)
