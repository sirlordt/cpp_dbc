# Using cpp_dbc with CMake

This document explains how to use the cpp_dbc library in your CMake project.

## Requirements

- CMake 3.15 or higher
- C++ compiler with C++23 support
- Dependencies based on enabled options (MySQL, PostgreSQL, SQLite, Firebird, MongoDB, yaml-cpp)

## Basic Usage with find_package

After installing the cpp_dbc library via the .deb package (Debian/Ubuntu) or .rpm package (Fedora), you can use it in your CMake project as follows:

```cmake
cmake_minimum_required(VERSION 3.15)
project(my_project VERSION 0.1.0 LANGUAGES CXX)

# Configure C++ standard
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Find the cpp_dbc library
find_package(cpp_dbc REQUIRED)

# Create an executable and link it with cpp_dbc
add_executable(my_application main.cpp)
target_link_libraries(my_application PRIVATE cpp_dbc::cpp_dbc)
```

The `find_package(cpp_dbc REQUIRED)` command will automatically find the cpp_dbc library and its dependencies. The library is installed in the standard locations:

- Library file: `/usr/lib/libcpp_dbc.a`
- Header files: `/usr/include/cpp_dbc/*.hpp`
- CMake configuration files: `/usr/lib/cmake/cpp_dbc/`

## C++ Code Example

```cpp
#include <cpp_dbc/cpp_dbc.hpp>
#include <iostream>

int main() {
    try {
        // Create a ConnectionPool instance
        cpp_dbc::ConnectionPool pool;
        
        // Configure and get a connection
        // (adjust parameters according to your database)
        auto conn = pool.getConnection("mysql", "localhost", "username", "password", "database");
        
        // Execute a query
        auto result = conn->executeQuery("SELECT * FROM my_table");
        
        // Process the results
        while (result->next()) {
            std::cout << "ID: " << result->getInt("id") << ", "
                      << "Name: " << result->getString("name") << std::endl;
        }
        
        // The connection is automatically returned to the pool when it goes out of scope
    }
    catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}
```

## Configuration Options

The cpp_dbc library may have been compiled with different options. Depending on the enabled options, you may need to install additional dependencies:

- MySQL: Requires libmysqlclient
- PostgreSQL: Requires libpq
- SQLite: Requires libsqlite3
- Firebird: Requires libfbclient
- MongoDB: Requires libmongoc, libbson, libmongocxx, libbsoncxx
- YAML: Requires libyaml-cpp

The CMake configuration file will automatically handle these dependencies.

## Firebird Connection Example

```cpp
#include <cpp_dbc/cpp_dbc.hpp>
#if USE_FIREBIRD
#include <cpp_dbc/drivers/relational/driver_firebird.hpp>
#endif
#include <iostream>

int main() {
    try {
#if USE_FIREBIRD
        // Register the Firebird driver
        cpp_dbc::DriverManager::registerDriver("firebird", std::make_shared<cpp_dbc::FirebirdDBDriver>());
        
        // Connect to a Firebird database
        auto conn = cpp_dbc::DriverManager::getDBConnection(
            "cpp_dbc:firebird://localhost:3050/path/to/database.fdb",
            "SYSDBA",
            "masterkey"
        );
        
        // Execute a query
        auto result = conn->executeQuery("SELECT * FROM my_table");
        
        // Process the results
        while (result->next()) {
            std::cout << "ID: " << result->getInt("id") << ", "
                      << "Name: " << result->getString("name") << std::endl;
        }
        
        conn->close();
#else
        std::cerr << "Firebird support not enabled" << std::endl;
#endif
    }
    catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}
```

## MongoDB Connection Example

```cpp
#include <cpp_dbc/cpp_dbc.hpp>
#if USE_MONGODB
#include <cpp_dbc/drivers/document/driver_mongodb.hpp>
#endif
#include <iostream>

int main() {
    try {
#if USE_MONGODB
        // Register the MongoDB driver
        cpp_dbc::DriverManager::registerDriver("mongodb",
            std::make_shared<cpp_dbc::MongoDB::MongoDBDriver>());
        
        // Connect to a MongoDB database
        auto conn = cpp_dbc::DriverManager::getDocumentDBConnection(
            "mongodb://localhost:27017/mydatabase",
            "username",
            "password"
        );
        
        // Get a collection
        auto collection = conn->getCollection("users");
        
        // Insert a document
        collection->insertOne(R"({"name": "John Doe", "age": 30})");
        
        // Find documents
        auto cursor = collection->find(R"({"age": {"$gte": 25}})");
        while (cursor->next()) {
            auto data = cursor->getData();
            std::cout << "Document: " << data->toJson() << std::endl;
        }
        
        conn->close();
#else
        std::cerr << "MongoDB support not enabled" << std::endl;
#endif
    }
    catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}
```

## Compiler Warnings and Code Quality

The cpp_dbc library is built with comprehensive warning flags and compile-time checks to ensure high code quality:

```cmake
# Example of adding the same warning flags to your project
target_compile_options(my_application PRIVATE
    -Wall -Wextra -Wpedantic -Wconversion -Wshadow -Wcast-qual
    -Wformat=2 -Wunused -Werror=return-type -Werror=switch
    -Wdouble-promotion -Wfloat-equal -Wundef -Wpointer-arith -Wcast-align
)
```

These warning flags help catch potential issues:

- `-Wall -Wextra -Wpedantic`: Standard warning flags
- `-Wconversion`: Warns about implicit conversions that may change a value
- `-Wshadow`: Warns when a variable declaration shadows another variable
- `-Wcast-qual`: Warns about casts that remove type qualifiers
- `-Wformat=2`: Enables additional format string checks
- `-Wunused`: Warns about unused variables and functions
- `-Werror=return-type`: Treats missing return statements as errors
- `-Werror=switch`: Treats switch statement issues as errors
- `-Wdouble-promotion`: Warns about implicit float to double promotions
- `-Wfloat-equal`: Warns about floating-point equality comparisons
- `-Wundef`: Warns about undefined identifiers in preprocessor expressions
- `-Wpointer-arith`: Warns about suspicious pointer arithmetic
- `-Wcast-align`: Warns about pointer casts that increase alignment requirements

When using the cpp_dbc library in your project, it's recommended to enable similar warning flags to maintain code quality and compatibility.

## Troubleshooting

If you encounter any issues with finding the cpp_dbc library, make sure that:

1. The cpp_dbc package is installed correctly
2. The CMake configuration files are in `/usr/lib/cmake/cpp_dbc/`
3. Your CMake version is 3.15 or higher

If you're still having issues, you can try one of these approaches:

1. Manually specify the path to the cpp_dbc CMake configuration files:

```cmake
# Only needed if CMake can't find cpp_dbc automatically
set(cpp_dbc_DIR "/usr/lib/cmake/cpp_dbc")
find_package(cpp_dbc REQUIRED)
```

2. Use the FindCPPDBC.cmake module:

```cmake
# Add the path to FindCPPDBC.cmake if it's not in a standard location
list(APPEND CMAKE_MODULE_PATH "/usr/lib/cmake/cpp_dbc")
find_package(CPPDBC REQUIRED)
target_link_libraries(my_application PRIVATE cpp_dbc::cpp_dbc)
```

The FindCPPDBC.cmake module provides an alternative way to find the library when the config mode doesn't work. It will search for the library and headers in standard locations and create the same cpp_dbc::cpp_dbc imported target.