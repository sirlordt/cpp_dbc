cmake_minimum_required(VERSION 3.15)
project(cpp_dbc VERSION 0.1.0 LANGUAGES CXX)

# Options for conditional compilation
option(USE_MYSQL "Enable MySQL support" ON)
option(USE_POSTGRESQL "Enable PostgreSQL support" OFF)

# Set C++ standard
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Add the cmake directory to the module path
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

# Define the library
add_library(cpp_dbc
    cpp_dbc.hpp
    connection_pool.hpp
    connection_pool.cpp
    transaction_manager.hpp
    transaction_manager.cpp
    driver_manager.cpp
)

# Create an alias target for use in the same build tree
add_library(cpp_dbc::cpp_dbc ALIAS cpp_dbc)

# Add compile definitions based on options
target_compile_definitions(cpp_dbc PUBLIC
    $<$<BOOL:${USE_MYSQL}>:USE_MYSQL=1>
    $<$<NOT:$<BOOL:${USE_MYSQL}>>:USE_MYSQL=0>
    $<$<BOOL:${USE_POSTGRESQL}>:USE_POSTGRESQL=1>
    $<$<NOT:$<BOOL:${USE_POSTGRESQL}>>:USE_POSTGRESQL=0>
)

# Include directories
target_include_directories(cpp_dbc PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<INSTALL_INTERFACE:include>
)

# Conditionally add MySQL driver files and dependencies
if(USE_MYSQL)
    target_sources(cpp_dbc PRIVATE
        drivers/driver_mysql.hpp
        drivers/driver_mysql.cpp
    )

    # Find MySQL package
    find_package(MySQL REQUIRED)
    target_include_directories(cpp_dbc PUBLIC ${MYSQL_INCLUDE_DIR})
    target_link_libraries(cpp_dbc PUBLIC ${MYSQL_LIBRARIES})
endif()

# Conditionally add PostgreSQL driver files and dependencies
if(USE_POSTGRESQL)
    target_sources(cpp_dbc PRIVATE
        drivers/driver_postgresql.hpp
        drivers/driver_postgresql.cpp
    )

    # Find PostgreSQL package
    find_package(PostgreSQL REQUIRED)
    target_include_directories(cpp_dbc PUBLIC ${PostgreSQL_INCLUDE_DIRS})
    target_link_libraries(cpp_dbc PUBLIC ${PostgreSQL_LIBRARIES})
endif()

# Generate and install export targets
include(CMakePackageConfigHelpers)
include(GNUInstallDirs)

# Installation rules
install(TARGETS cpp_dbc
    EXPORT cpp_dbc-targets
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

# Install header files
install(FILES
    cpp_dbc.hpp
    connection_pool.hpp
    transaction_manager.hpp
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/cpp_dbc
)

if(USE_MYSQL)
    install(FILES drivers/driver_mysql.hpp
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/cpp_dbc/drivers)
endif()

if(USE_POSTGRESQL)
    install(FILES drivers/driver_postgresql.hpp
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/cpp_dbc/drivers)
endif()

# Install the export targets
install(EXPORT cpp_dbc-targets
    FILE cpp_dbc-targets.cmake
    NAMESPACE cpp_dbc::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/cpp_dbc
)

# Create the config file
configure_package_config_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/cmake/cpp_dbc-config.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/cpp_dbc-config.cmake
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/cpp_dbc
)

# Create the version file
write_basic_package_version_file(
    ${CMAKE_CURRENT_BINARY_DIR}/cpp_dbc-config-version.cmake
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
)

# Install the config and version files
install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/cpp_dbc-config.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/cpp_dbc-config-version.cmake
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/cpp_dbc
)

# Install the Find*.cmake modules
install(FILES
    ${CMAKE_CURRENT_SOURCE_DIR}/cmake/FindMySQL.cmake
    ${CMAKE_CURRENT_SOURCE_DIR}/cmake/FindPostgreSQL.cmake
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/cpp_dbc
)