cmake_minimum_required(VERSION 3.15)
project(cpp_dbc VERSION 0.1.0 LANGUAGES CXX)

# Options for conditional compilation
option(USE_MYSQL "Enable MySQL support" ON)
option(USE_POSTGRESQL "Enable PostgreSQL support" OFF)
option(USE_SQLITE "Enable SQLite support" OFF)
option(USE_CPP_YAML "Enable YAML configuration support" OFF)

# Debug options
option(DEBUG_CONNECTION_POOL "Enable debug output for ConnectionPool" OFF)
option(DEBUG_TRANSACTION_MANAGER "Enable debug output for TransactionManager" OFF)

# Set C++ standard
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Add the cmake directory to the module path
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

# Define the library
add_library(cpp_dbc
    src/connection_pool.cpp
    src/transaction_manager.cpp
    src/driver_manager.cpp
    src/config/database_config.cpp
    src/common/system_utils.cpp
)

# Conditionally add YAML config loader and database config implementation
if(USE_CPP_YAML)
    target_sources(cpp_dbc PRIVATE
        src/config/yaml_config_loader.cpp
    )
endif()

# Add database config implementation
# target_sources(cpp_dbc PRIVATE
# )

# Create an alias target for use in the same build tree
add_library(cpp_dbc::cpp_dbc ALIAS cpp_dbc)

# Add compile definitions based on options
target_compile_definitions(cpp_dbc PUBLIC
    $<$<BOOL:${USE_MYSQL}>:USE_MYSQL=1>
    $<$<NOT:$<BOOL:${USE_MYSQL}>>:USE_MYSQL=0>
    $<$<BOOL:${USE_POSTGRESQL}>:USE_POSTGRESQL=1>
    $<$<NOT:$<BOOL:${USE_POSTGRESQL}>>:USE_POSTGRESQL=0>
    $<$<BOOL:${USE_SQLITE}>:USE_SQLITE=1>
    $<$<NOT:$<BOOL:${USE_SQLITE}>>:USE_SQLITE=0>
    $<$<BOOL:${USE_CPP_YAML}>:USE_CPP_YAML=1>
    $<$<NOT:$<BOOL:${USE_CPP_YAML}>>:USE_CPP_YAML=0>
    $<$<BOOL:${DEBUG_CONNECTION_POOL}>:DEBUG_CONNECTION_POOL=1>
    $<$<BOOL:${DEBUG_TRANSACTION_MANAGER}>:DEBUG_TRANSACTION_MANAGER=1>
)

# Include directories
target_include_directories(cpp_dbc PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

# Add include directory for source files
target_include_directories(cpp_dbc PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# Conditionally add MySQL driver files and dependencies
if(USE_MYSQL)
    target_sources(cpp_dbc PRIVATE
        src/drivers/driver_mysql.cpp
    )

    # Find MySQL package
    find_package(MySQL REQUIRED)
    target_include_directories(cpp_dbc PUBLIC ${MYSQL_INCLUDE_DIR})
    target_link_libraries(cpp_dbc PUBLIC ${MYSQL_LIBRARIES})
endif()

# Conditionally add PostgreSQL driver files and dependencies
if(USE_POSTGRESQL)
    target_sources(cpp_dbc PRIVATE
        src/drivers/driver_postgresql.cpp
    )

    # Find PostgreSQL package
    find_package(PostgreSQL REQUIRED)
    target_include_directories(cpp_dbc PUBLIC ${PostgreSQL_INCLUDE_DIRS})
    target_link_libraries(cpp_dbc PUBLIC ${PostgreSQL_LIBRARIES})
endif()

# Conditionally add SQLite driver files and dependencies
if(USE_SQLITE)
    target_sources(cpp_dbc PRIVATE
        src/drivers/driver_sqlite.cpp
    )

    # Find SQLite package
    find_package(SQLite3 REQUIRED)
    target_include_directories(cpp_dbc PUBLIC ${SQLite3_INCLUDE_DIRS})
    target_link_libraries(cpp_dbc PUBLIC ${SQLite3_LIBRARIES})
endif()

# Conditionally add YAML-CPP dependencies
if(USE_CPP_YAML)
    # Find YAML-CPP package
    # First try to find it using the config file provided by Conan
    if(EXISTS "${CMAKE_BINARY_DIR}/generators/yaml-cpp-config.cmake")
        set(yaml-cpp_DIR "${CMAKE_BINARY_DIR}/generators")
    elseif(EXISTS "${CMAKE_BINARY_DIR}/../../${CMAKE_BUILD_TYPE}/generators/yaml-cpp-config.cmake")
        set(yaml-cpp_DIR "${CMAKE_BINARY_DIR}/../../${CMAKE_BUILD_TYPE}/generators")
    endif()

    find_package(yaml-cpp REQUIRED)
    target_link_libraries(cpp_dbc PUBLIC yaml-cpp::yaml-cpp)
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
install(DIRECTORY include/
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    FILES_MATCHING PATTERN "*.hpp"
)

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
    ${CMAKE_CURRENT_SOURCE_DIR}/cmake/FindSQLite3.cmake
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/cpp_dbc
)

# Add tests subdirectory if testing is enabled
option(CPP_DBC_BUILD_TESTS "Build cpp_dbc tests" OFF)

if(CPP_DBC_BUILD_TESTS)
    enable_testing()
    add_subdirectory(test)
endif()

# Add examples
option(CPP_DBC_BUILD_EXAMPLES "Build cpp_dbc examples" OFF)

if(CPP_DBC_BUILD_EXAMPLES)
    # Basic example
    add_executable(cpp_dbc_example examples/example.cpp)
    target_link_libraries(cpp_dbc_example PRIVATE cpp_dbc)

    # Connection pool example
    add_executable(connection_pool_example examples/connection_pool_example.cpp)
    target_link_libraries(connection_pool_example PRIVATE cpp_dbc)

    # Transaction manager example
    add_executable(transaction_manager_example examples/transaction_manager_example.cpp)
    target_link_libraries(transaction_manager_example PRIVATE cpp_dbc)

    # Configuration example
    add_executable(config_example examples/config_example.cpp)
    target_link_libraries(config_example PRIVATE cpp_dbc)

    # Configuration integration example
    add_executable(config_integration_example examples/config_integration_example.cpp)
    target_link_libraries(config_integration_example PRIVATE cpp_dbc)
endif()