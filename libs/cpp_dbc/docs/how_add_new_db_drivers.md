# How to Add New Database Drivers to cpp_dbc

This guide explains how to add support for a new database management system (DBMS) to the cpp_dbc library. It is designed for both human contributors and LLM assistants.

## Table of Contents

1. [Overview](#overview)
2. [Code Reference](#code-reference)
   - [Base Interfaces](#base-interfaces)
   - [Family-Specific Interfaces](#family-specific-interfaces)
   - [Utilities](#utilities)
   - [Thread Safety Macros](#thread-safety-macros)
   - [Reference Implementations](#reference-implementations)
3. [Phase 1: Create Driver Files](#phase-1-create-driver-files)
   - [Identify the Driver Family](#identify-the-driver-family)
   - [Create Header Files (.hpp)](#create-header-files-hpp)
   - [Create Source Files (.cpp)](#create-source-files-cpp)
   - [Update Main Header (cpp_dbc.hpp)](#update-main-header-cpp_dbchpp)
4. [Phase 2: Update Build Configuration](#phase-2-update-build-configuration)
   - [Update CMakeLists.txt](#update-cmakeliststxt)
   - [Create FindModule for CMake](#create-findmodule-for-cmake)
   - [Update CMake Config Files](#update-cmake-config-files)
   - [Update Benchmark Common Header](#update-benchmark-common-header)
   - [Update Shell Scripts](#update-shell-scripts)
   - [Update Distribution Packages](#update-distribution-packages)
   - [Source Compilation Fallback](#source-compilation-fallback)
5. [Phase 3: Create Tests](#phase-3-create-tests)
   - [Test File Naming Convention](#test-file-naming-convention)
   - [Test Patterns by Family](#test-patterns-by-family)
   - [Update test/CMakeLists.txt](#update-testcmakeliststxt)
6. [Phase 4: Create Benchmarks](#phase-4-create-benchmarks)
   - [Benchmark File Structure](#benchmark-file-structure)
   - [Update benchmark/CMakeLists.txt](#update-benchmarkcmakeliststxt)
7. [Phase 5: Create Examples](#phase-5-create-examples)
   - [Example Files by Family](#example-files-by-family)
   - [Update CMakeLists.txt for Examples](#update-cmakeliststxt-for-examples)
8. [Checklist Summary](#checklist-summary)
9. [Common Mistakes to Avoid](#common-mistakes-to-avoid)
10. [Related Documentation](#related-documentation)

---

## Related Documentation

This guide is a high-level map. For deeper details, see these companion documents:

| Document | Description |
|----------|-------------|
| [shell_script_dependencies.md](shell_script_dependencies.md) | Detailed call hierarchy and dependencies between all shell scripts |
| [error_handling_patterns.md](error_handling_patterns.md) | Complete guide to DBException usage, error codes, and nothrow API patterns |

---

## Overview

The cpp_dbc library organizes database drivers by **family**:

| Family     | Description                        | Examples                        |
|------------|------------------------------------|---------------------------------|
| relational | SQL-based relational databases     | MySQL, PostgreSQL, SQLite, Firebird |
| document   | Document-oriented NoSQL databases  | MongoDB                         |
| columnar   | Column-family/wide-column databases| ScyllaDB (Cassandra)            |
| kv         | Key-value stores                   | Redis                           |
| graph      | Graph databases                    | (Reserved for future)           |
| timeseries | Time-series databases              | (Reserved for future)           |

Each driver consists of:
- **Header files** in `libs/cpp_dbc/include/cpp_dbc/drivers/<family>/<driver_name>/`
- **Source files** in `libs/cpp_dbc/src/drivers/<family>/<driver_name>/`
- **An umbrella header** at `libs/cpp_dbc/include/cpp_dbc/drivers/<family>/driver_<driver_name>.hpp`

---

## Code Reference

Before implementing a new driver, study the core interfaces in `libs/cpp_dbc/include/cpp_dbc/`. This section provides a map of all relevant files.

### Base Interfaces

These are the fundamental classes that all drivers inherit from:

| File | Class | Description |
|------|-------|-------------|
| `core/db_driver.hpp` | `DBDriver` | Base class for all drivers. Defines `connect()`, `acceptsURL()`, `getDBType()`, `getName()` |
| `core/db_connection.hpp` | `DBConnection` | Base class for all connections. Defines `close()`, `isClosed()`, `returnToPool()`, `getURL()` |
| `core/db_result_set.hpp` | `DBResultSet` | Base class for result sets. Defines `close()`, `isEmpty()` |
| `core/db_exception.hpp` | `DBException` | Exception class with 12-char error codes and call stack capture |
| `core/db_types.hpp` | `DBType`, `Types`, `TransactionIsolationLevel` | Core enums for database types, SQL types, and isolation levels |
| `core/db_expected.hpp` | `expected<T,E>` | Error handling type for nothrow API pattern |
| `core/db_connection_pool.hpp` | `DBConnectionPool` | Base class for connection pools |
| `core/db_connection_pooled.hpp` | `DBConnectionPooled` | Wrapper for pooled connections |
| `core/streams.hpp` | `InputStream` | Stream interface for BLOB data |

### Family-Specific Interfaces

Each database family has abstract interfaces in `libs/cpp_dbc/include/cpp_dbc/core/<family>/`. **Read the .hpp files in these directories to see the virtual methods you must implement.**

| Family | Interface Directory | Status |
|--------|---------------------|--------|
| **relational** | `core/relational/` | ✅ Implemented |
| **document** | `core/document/` | ✅ Implemented |
| **kv** | `core/kv/` | ✅ Implemented |
| **columnar** | `core/columnar/` | ✅ Implemented |
| **graph** | `core/graph/` | ❌ Not yet implemented |
| **timeseries** | `core/timeseries/` | ❌ Not yet implemented |

> **Important**: For `graph` or `timeseries`, you must first create the base interfaces in the corresponding `core/` directory and add the type to `DBType` enum in `core/db_types.hpp`.

### Utilities

| File | Description |
|------|-------------|
| `common/system_utils.hpp` | `captureCallStack()`, `parseDBURL()`, `safePrint()`, logging functions |
| `config/database_config.hpp` | Database configuration structures |
| `config/yaml_config_loader.hpp` | YAML configuration file loader |
| `blob.hpp` | BLOB type definitions |
| `transaction_manager.hpp` | Transaction management utilities |
| `backward.hpp` | Stack trace support (third-party) |

### Thread Safety Macros

Each driver defines thread-safety macros in its internal header file at `src/drivers/<family>/<driver>/<driver>_internal.hpp`:

```cpp
// Thread-safety macros for conditional mutex locking
// Using recursive_mutex to allow the same thread to acquire the lock multiple times
#if DB_DRIVER_THREAD_SAFE
#define DB_DRIVER_MUTEX mutable std::recursive_mutex
#define DB_DRIVER_LOCK_GUARD(mutex) std::lock_guard<std::recursive_mutex> lock(mutex)
#define DB_DRIVER_UNIQUE_LOCK(mutex) std::unique_lock<std::recursive_mutex> lock(mutex)
#else
#define DB_DRIVER_MUTEX
#define DB_DRIVER_LOCK_GUARD(mutex) (void)0
#define DB_DRIVER_UNIQUE_LOCK(mutex) (void)0
#endif

// Debug output macro (controlled by -DDEBUG_<DRIVER>=1 or -DDEBUG_ALL=1)
#if (defined(DEBUG_<DRIVER>) && DEBUG_<DRIVER>) || (defined(DEBUG_ALL) && DEBUG_ALL)
#define <DRIVER>_DEBUG(x) std::cout << "[<Driver>] " << x << std::endl
#else
#define <DRIVER>_DEBUG(x)
#endif
```

Create a similar `<driver>_internal.hpp` file for your new driver in `src/drivers/<family>/<driver>/`.

### Reference Implementations

Study these existing drivers as templates for your implementation:

| Family | Driver | Directory | Notes |
|--------|--------|-----------|-------|
| **relational** | MySQL | `drivers/relational/mysql/` | Most complete example with all features |
| **relational** | PostgreSQL | `drivers/relational/postgresql/` | Good example of native type handling |
| **relational** | SQLite | `drivers/relational/sqlite/` | Example of embedded database |
| **relational** | Firebird | `drivers/relational/firebird/` | Example with special transaction handling |
| **document** | MongoDB | `drivers/document/mongodb/` | Complete document store implementation |
| **kv** | Redis | `drivers/kv/redis/` | Complete key-value store implementation |
| **columnar** | ScyllaDB | `drivers/columnar/scylladb/` | Complete columnar database implementation |

### External Resources

In addition to studying the codebase, you will need to consult **official documentation** for the target database:

| Resource Type | What to Look For | Examples |
|---------------|------------------|----------|
| **C/C++ Client Library Docs** | API functions, connection handling, query execution, result iteration | libmysqlclient docs, libpq docs, MongoDB C Driver docs |
| **Native Data Types** | Type mappings between the database and C/C++ types | `MYSQL_TYPE_*`, `Oid` in PostgreSQL, `BSON` types |
| **Error Handling** | Error codes, error retrieval functions, connection state | `mysql_errno()`, `PQerrorMessage()`, `mongoc_error_t` |
| **Thread Safety** | Whether the client library is thread-safe, connection sharing rules | Per-connection vs per-statement thread safety |
| **Memory Management** | Who owns allocated memory, when to free resources | Result set lifecycle, prepared statement cleanup |

> **Important for LLM Assistants**: This project cannot document every possible database API. If you are unsure about:
> - Which C/C++ client library to use for the target database
> - Where to find the official documentation
> - The correct GitHub repository or official project URL
>
> **You MUST ask the user for clarification** before proceeding. Request:
> 1. The name of the C/C++ client library to use (e.g., `libpq`, `libmysqlclient`, `hiredis`)
> 2. Official documentation URLs
> 3. GitHub repository or source code URLs
> 4. Any other relevant references the user can provide
>
> Do not guess or assume which library to use—different databases may have multiple client libraries with different APIs and trade-offs.

---

## Phase 1: Create Driver Files

### Identify the Driver Family

First, determine which family your new driver belongs to:

- **relational**: Traditional SQL databases (MySQL, PostgreSQL, SQLite, Firebird)
- **document**: Document stores (MongoDB)
- **columnar**: Wide-column stores (ScyllaDB/Cassandra)
- **kv**: Key-value stores (Redis)
- **graph**: Graph databases (future)
- **timeseries**: Time-series databases (future)

For this guide, we'll use `sqlserver` (Microsoft SQL Server) as an example relational driver.

### Create Header Files (.hpp)

#### Implementation Order

**Always implement in this order** (due to dependencies):

1. `handles.hpp` - RAII handles (no internal dependencies)
2. `*_internal.hpp` - Thread-safety macros (in `src/` directory)
3. `result_set.hpp` / `cursor.hpp` - Data retrieval (depends on handles)
4. `prepared_statement.hpp` - Query execution (depends on handles, result_set)
5. `connection.hpp` - Connection management (depends on all above)
6. `driver.hpp` - Factory class (depends on connection)

#### Directory Structure

```text
libs/cpp_dbc/include/cpp_dbc/drivers/relational/
├── driver_sqlserver.hpp          # Umbrella header (includes all components)
└── sqlserver/
    ├── handles.hpp               # RAII handles for native resources
    ├── input_stream.hpp          # Input stream for BLOB handling (optional)
    ├── blob.hpp                  # BLOB type definitions (optional)
    ├── result_set.hpp            # Result set class
    ├── prepared_statement.hpp    # Prepared statement class
    ├── connection.hpp            # Connection class
    └── driver.hpp                # Driver factory class
```

#### Umbrella Header Pattern

Create `driver_sqlserver.hpp`:

```cpp
/**
 * @file driver_sqlserver.hpp
 * @brief SQL Server database driver implementation
 */

#ifndef CPP_DBC_DRIVER_SQLSERVER_HPP
#define CPP_DBC_DRIVER_SQLSERVER_HPP

#include "sqlserver/handles.hpp"
#include "sqlserver/input_stream.hpp"
#include "sqlserver/blob.hpp"
#include "sqlserver/result_set.hpp"
#include "sqlserver/prepared_statement.hpp"
#include "sqlserver/connection.hpp"
#include "sqlserver/driver.hpp"

#endif // CPP_DBC_DRIVER_SQLSERVER_HPP
```

#### Component Classes by Family

Each family has different required components. **For exact `#include` dependencies and class inheritance, read the existing driver headers in the reference implementations.**

| Family | Reference Directory | Key Files |
|--------|---------------------|-----------|
| **relational** | `drivers/relational/mysql/` | `handles.hpp`, `result_set.hpp`, `prepared_statement.hpp`, `connection.hpp`, `driver.hpp` |
| **document** | `drivers/document/mongodb/` | `handles.hpp`, `document.hpp`, `cursor.hpp`, `collection.hpp`, `connection.hpp`, `driver.hpp` |
| **kv** | `drivers/kv/redis/` | `handles.hpp`, `reply_handle.hpp`, `connection.hpp`, `driver.hpp` |
| **columnar** | `drivers/columnar/scylladb/` | `handles.hpp`, `memory_input_stream.hpp`, `result_set.hpp`, `prepared_statement.hpp`, `connection.hpp`, `driver.hpp` |

> **Tip**: Open the reference driver's `driver_<name>.hpp` umbrella header to see the exact include order and dependencies.

### Create Source Files (.cpp)

#### Directory Structure

```
libs/cpp_dbc/src/drivers/relational/sqlserver/
├── driver_01.cpp                 # Driver factory implementation
├── connection_01.cpp             # Connection: constructor, destructor, basic ops
├── connection_02.cpp             # Connection: query execution
├── connection_03.cpp             # Connection: transaction handling
├── prepared_statement_01.cpp     # PreparedStatement: setup and binding
├── prepared_statement_02.cpp     # PreparedStatement: execution
├── prepared_statement_03.cpp     # PreparedStatement: result handling
├── result_set_01.cpp             # ResultSet: navigation
├── result_set_02.cpp             # ResultSet: data retrieval (basic types)
└── result_set_03.cpp             # ResultSet: data retrieval (complex types)
```

#### File Splitting Convention

Source files are split by functionality to:
1. Reduce compilation time (parallel builds)
2. Improve code organization
3. Allow incremental changes

**Naming Pattern**: `<component>_<number>.cpp`

| Component          | Files                                     |
|--------------------|-------------------------------------------|
| Driver             | `driver_01.cpp`                           |
| Connection         | `connection_01.cpp` to `connection_0N.cpp`|
| PreparedStatement  | `prepared_statement_01.cpp` to `..._0N.cpp`|
| ResultSet          | `result_set_01.cpp` to `result_set_0N.cpp`|

### Update Main Header (cpp_dbc.hpp)

**IMPORTANT**: Update `libs/cpp_dbc/include/cpp_dbc/cpp_dbc.hpp` to include the new driver.

#### 1. Add Default Macro Definition

Add the `USE_<DRIVER>` macro with the other driver macros (around line 25-51):

```cpp
#ifndef USE_SQLSERVER
#define USE_SQLSERVER 0 // NOSONAR - Macro required for conditional compilation
#endif
```

#### 2. Add Conditional Include

Add the conditional include for the new driver header (around line 80-107):

```cpp
#if USE_SQLSERVER
#include "cpp_dbc/drivers/relational/driver_sqlserver.hpp"
#endif
```

**Note**: The include path depends on the driver family:
- Relational: `cpp_dbc/drivers/relational/driver_<name>.hpp`
- Document: `cpp_dbc/drivers/document/driver_<name>.hpp`
- Key-Value: `cpp_dbc/drivers/kv/driver_<name>.hpp`
- Columnar: `cpp_dbc/drivers/columnar/driver_<name>.hpp`

---

## Phase 2: Update Build Configuration

### Update CMakeLists.txt

Edit `libs/cpp_dbc/CMakeLists.txt`:

#### 1. Add the option for the new driver

```cmake
option(USE_SQLSERVER "Enable SQL Server support" OFF)
```

#### 2. Add compile definitions

```cmake
target_compile_definitions(cpp_dbc PUBLIC
    # ... existing definitions ...
    $<$<BOOL:${USE_SQLSERVER}>:USE_SQLSERVER=1>
    $<$<NOT:$<BOOL:${USE_SQLSERVER}>>:USE_SQLSERVER=0>
)
```

#### 3. Add conditional source files

```cmake
# Conditionally add SQL Server driver files and dependencies
if(USE_SQLSERVER)
    target_sources(cpp_dbc PRIVATE
        # SQLServerDBResultSet
        src/drivers/relational/sqlserver/result_set_01.cpp
        src/drivers/relational/sqlserver/result_set_02.cpp
        src/drivers/relational/sqlserver/result_set_03.cpp

        # SQLServerDBPreparedStatement
        src/drivers/relational/sqlserver/prepared_statement_01.cpp
        src/drivers/relational/sqlserver/prepared_statement_02.cpp
        src/drivers/relational/sqlserver/prepared_statement_03.cpp

        # SQLServerDBConnection
        src/drivers/relational/sqlserver/connection_01.cpp
        src/drivers/relational/sqlserver/connection_02.cpp
        src/drivers/relational/sqlserver/connection_03.cpp

        # SQLServerDBDriver
        src/drivers/relational/sqlserver/driver_01.cpp
    )

    # Find SQL Server package
    find_package(SQLServer REQUIRED)
    target_include_directories(cpp_dbc PUBLIC ${SQLSERVER_INCLUDE_DIR})
    target_link_libraries(cpp_dbc PUBLIC ${SQLSERVER_LIBRARIES})
endif()
```

#### 4. Install the FindModule

```cmake
install(FILES
    # ... existing files ...
    ${CMAKE_CURRENT_SOURCE_DIR}/cmake/FindSQLServer.cmake
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/cpp_dbc
)
```

### Create FindModule for CMake

Create `libs/cpp_dbc/cmake/FindSQLServer.cmake`:

```cmake
# FindSQLServer.cmake
# Find the SQL Server client library
#
# This module defines:
#   SQLSERVER_FOUND        - True if SQL Server was found
#   SQLSERVER_INCLUDE_DIR  - Include directories
#   SQLSERVER_LIBRARIES    - Libraries to link

find_path(SQLSERVER_INCLUDE_DIR
    NAMES sql.h sqlext.h
    PATHS
        /usr/include
        /usr/local/include
        /opt/mssql-tools/include
)

find_library(SQLSERVER_LIBRARIES
    NAMES odbc
    PATHS
        /usr/lib
        /usr/local/lib
        /opt/mssql-tools/lib
)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(SQLServer
    DEFAULT_MSG
    SQLSERVER_LIBRARIES
    SQLSERVER_INCLUDE_DIR
)

mark_as_advanced(SQLSERVER_INCLUDE_DIR SQLSERVER_LIBRARIES)
```

### Update CMake Config Files

These files are used when cpp_dbc is installed as a package and consumed by other projects.

#### 1. Update `libs/cpp_dbc/cmake/cpp_dbc-config.cmake`

This file provides the installed package configuration:

```cmake
# Add variable for the new driver
set(CPP_DBC_USE_SQLSERVER @USE_SQLSERVER@)

# Add compile definitions
if(CPP_DBC_USE_SQLSERVER)
    target_compile_definitions(cpp_dbc::cpp_dbc INTERFACE USE_SQLSERVER=1)
else()
    target_compile_definitions(cpp_dbc::cpp_dbc INTERFACE USE_SQLSERVER=0)
endif()

# Add find_package and linking (if applicable)
if(CPP_DBC_USE_SQLSERVER)
    include("${CMAKE_CURRENT_LIST_DIR}/FindSQLServer.cmake")
    if(NOT SQLServer_FOUND)
        message(FATAL_ERROR "SQL Server library not found.")
    endif()
    target_include_directories(cpp_dbc::cpp_dbc INTERFACE ${SQLSERVER_INCLUDE_DIR})
    target_link_libraries(cpp_dbc::cpp_dbc INTERFACE ${SQLSERVER_LIBRARIES})
endif()
```

#### 2. Update `libs/cpp_dbc/cmake/FindCPPDBC.cmake`

This file auto-detects available drivers for fallback discovery:

```cmake
# Check for SQL Server
find_package(SQLServer QUIET)
if(SQLServer_FOUND)
    target_link_libraries(cpp_dbc::cpp_dbc INTERFACE ${SQLSERVER_LIBRARIES})
    target_include_directories(cpp_dbc::cpp_dbc INTERFACE ${SQLSERVER_INCLUDE_DIR})
    target_compile_definitions(cpp_dbc::cpp_dbc INTERFACE USE_SQLSERVER=1)
else()
    target_compile_definitions(cpp_dbc::cpp_dbc INTERFACE USE_SQLSERVER=0)
endif()
```

### Update Benchmark Common Header

Update `libs/cpp_dbc/benchmark/benchmark_common.hpp` to include the new driver:

```cpp
#if USE_SQLSERVER
#include <cpp_dbc/drivers/relational/driver_sqlserver.hpp>
#endif
```

### Update Shell Scripts

**CRITICAL**: This is where mistakes are most common. Update ALL of these files.

> **See also**: [shell_script_dependencies.md](shell_script_dependencies.md) for a detailed diagram showing how these scripts call each other.

#### 1. `libs/cpp_dbc/build_cpp_dbc.sh`

Add the new driver option:

```bash
# Default values section (around line 15-35)
USE_SQLSERVER=OFF

# Parse command line arguments section (add case)
--sqlserver|--sqlserver-on)
USE_SQLSERVER=ON
shift
;;
--sqlserver-off)
USE_SQLSERVER=OFF
shift
;;

# Help message section
echo "  --sqlserver, --sqlserver-on  Enable SQL Server support"
echo "  --sqlserver-off              Disable SQL Server support"

# Dependency check section
if [ "$USE_SQLSERVER" = "ON" ]; then
    echo "Checking for SQL Server ODBC driver..."
    # Add package manager detection logic
fi

# Echo configuration section
echo "  SQL Server support: $USE_SQLSERVER"

# CMake call section
-DUSE_SQLSERVER=$USE_SQLSERVER \
```

#### 2. `libs/cpp_dbc/build_test_cpp_dbc.sh`

Same pattern as above - add option parsing, help text, and CMake variable.

#### 3. `libs/cpp_dbc/run_test_cpp_dbc.sh`

Add test running support for the new driver.

#### 4. `libs/cpp_dbc/run_benchmarks_cpp_dbc.sh`

Add benchmark support for the new driver.

#### 5. `libs/cpp_dbc/build_dist_pkg.sh`

Add distro package building support:

```bash
# Add in options parsing
sqlserver)
    CMAKE_SQLSERVER_OPTION="-DCPP_DBC_WITH_SQLSERVER=ON"
    USE_SQLSERVER="ON"
    BUILD_FLAGS="$BUILD_FLAGS --sqlserver"
    ;;

# Add package dependency handling
SQLSERVER_DEV_PKG="unixodbc-dev"  # or appropriate package
```

#### 6. Root `helper.sh`

Add the driver to:
- `show_usage()` function
- `cmd_run_build()` function
- `cmd_run_test()` function
- `cmd_run_benchmarks()` function
- Various `--bk-combo-XX`, `--mc-combo-XX` shortcuts

#### 7. Root `run_test.sh`

Add option handling for the new driver.

### Update Distribution Packages

**CRITICAL**: Don't forget the distros folder!

#### For each distribution in `libs/cpp_dbc/distros/`:

1. **Update `Dockerfile`** - Add package placeholder:
```dockerfile
RUN apt-get install -y \
    # ... existing packages ...
    __SQLSERVER_DEV_PKG__
```

2. **Update `build_script.sh`** - Add parameter handling:
```bash
if [ "__USE_SQLSERVER__" = "ON" ]; then
    SQLSERVER_PARAM="--sqlserver"
else
    SQLSERVER_PARAM="--sqlserver-off"
fi
```

#### Supported distributions:

- `debian_12/`
- `debian_13/`
- `ubuntu_22_04/`
- `ubuntu_24_04/`
- `fedora_42/`
- `fedora_43/`

For each, update both `Dockerfile` and `build_script.sh`.

### Source Compilation Fallback

Some database drivers don't have official `.deb` or `.rpm` packages. Examples include:
- **ScyllaDB/Cassandra C++ Driver** - No official packages, only source code
- **MongoDB C++ Driver** (on some distributions) - May require source compilation

For these cases, create a `download_and_setup_<driver>.sh` script that:
1. Checks if the driver is already installed
2. Downloads and compiles from source if not found
3. Installs to `/usr/local`

#### When to Create a Setup Script

Create a `download_and_setup_<driver>.sh` script when:
- No official `.deb`/`.rpm` package exists for the driver
- The driver must be compiled from source on most distributions
- Different distributions require different build configurations

#### Script Location and Naming

Place the script at:
```
libs/cpp_dbc/download_and_setup_<driver>_driver.sh
```

Examples:
- `download_and_setup_cassandra_driver.sh` (for ScyllaDB/Cassandra)
- `download_and_setup_sqlserver_driver.sh` (hypothetical)

#### Script Structure

Follow this template based on `download_and_setup_cassandra_driver.sh`:

```bash
#!/bin/bash
set -e

# ============================================
# 1. UTILITY FUNCTIONS
# ============================================

command_exists() {
    command -v "$1" >/dev/null 2>&1
}

# ============================================
# 2. PRIVILEGE CHECK
# ============================================

if [ "$(id -u)" -eq 0 ]; then
    SUDO=""
else
    if command_exists sudo; then
        SUDO="sudo"
    else
        echo "Error: This script requires root privileges or sudo."
        exit 1
    fi
fi

# ============================================
# 3. CHECK IF DRIVER ALREADY INSTALLED
# ============================================

echo "Checking for existing <Driver> installation..."

DRIVER_FOUND=0

# Check pkg-config
if command_exists pkg-config; then
    if pkg-config --exists <driver_name>; then
        echo "<Driver> found via pkg-config."
        DRIVER_FOUND=1
    fi
fi

# Check headers
HEADER_FOUND=0
if [ -f "/usr/local/include/<driver>.h" ] || [ -f "/usr/include/<driver>.h" ]; then
    HEADER_FOUND=1
fi

# Check libraries
LIB_FOUND=0
if ls /usr/local/lib/lib<driver>.* 1> /dev/null 2>&1 || \
   ls /usr/lib/lib<driver>.* 1> /dev/null 2>&1 || \
   ls /usr/lib/x86_64-linux-gnu/lib<driver>.* 1> /dev/null 2>&1 || \
   ls /usr/local/lib64/lib<driver>.* 1> /dev/null 2>&1 || \
   ls /usr/lib64/lib<driver>.* 1> /dev/null 2>&1; then
    LIB_FOUND=1
fi

# Exit early if already installed
if [ $DRIVER_FOUND -eq 1 ] || ( [ $HEADER_FOUND -eq 1 ] && [ $LIB_FOUND -eq 1 ] ); then
    echo "<Driver> appears to be already installed."
    exit 0
fi

echo "<Driver> not found. Starting installation..."

# ============================================
# 4. INSTALL BUILD DEPENDENCIES
# ============================================

echo "Installing build dependencies..."
if command_exists apt-get; then
    export DEBIAN_FRONTEND=noninteractive
    $SUDO apt-get update
    $SUDO apt-get install -y cmake make g++ <dep1> <dep2> wget pkg-config
elif command_exists dnf; then
    $SUDO dnf install -y cmake make gcc-c++ <dep1-fedora> <dep2-fedora> wget pkgconf-pkg-config
elif command_exists yum; then
    $SUDO yum install -y cmake make gcc-c++ <dep1-rhel> <dep2-rhel> wget pkgconfig
else
    echo "Warning: Could not detect package manager."
fi

# ============================================
# 5. CREATE TEMPORARY BUILD DIRECTORY
# ============================================

TEMP_DIR=$(mktemp -d -p /tmp <driver>_build.XXXXXX)
echo "Using temporary directory: $TEMP_DIR"

cleanup() {
    if [ -d "$TEMP_DIR" ]; then
        echo "Cleaning up temporary directory..."
        rm -rf "$TEMP_DIR"
    fi
}
trap cleanup EXIT

cd "$TEMP_DIR"

# ============================================
# 6. DOWNLOAD SOURCE CODE
# ============================================

echo "Downloading <Driver> version X.Y.Z..."
wget https://github.com/<org>/<repo>/archive/refs/tags/X.Y.Z.tar.gz -O <driver>.tar.gz

echo "Extracting..."
tar xzf <driver>.tar.gz
cd <repo>-X.Y.Z

# ============================================
# 7. BUILD
# ============================================

echo "Building <Driver>..."
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/usr/local
make -j$(nproc)

# ============================================
# 8. INSTALL
# ============================================

echo "Installing <Driver>..."
$SUDO make install

if command_exists ldconfig; then
    $SUDO ldconfig
fi

echo "<Driver> installation complete."
```

#### Key Implementation Details

1. **Installation Detection**: Check multiple locations:
   - `/usr/local/include/` and `/usr/include/` for headers
   - `/usr/local/lib/`, `/usr/lib/`, `/usr/lib/x86_64-linux-gnu/`, `/usr/lib64/` for libraries
   - Use `pkg-config` when available

2. **Package Manager Detection**: Support all major package managers:
   - `apt-get` for Debian/Ubuntu
   - `dnf` for Fedora
   - `yum` for RHEL/CentOS

3. **Build Dependencies**: Map dependencies to each package manager:
   ```bash
   # Debian/Ubuntu
   apt-get install -y libuv1-dev libssl-dev

   # Fedora
   dnf install -y libuv-devel openssl-devel
   ```

4. **Temporary Directory**: Use `mktemp` with cleanup trap:
   ```bash
   TEMP_DIR=$(mktemp -d -p /tmp <driver>_build.XXXXXX)
   trap cleanup EXIT
   ```

5. **Install Location**: Always install to `/usr/local`:
   ```bash
   cmake .. -DCMAKE_INSTALL_PREFIX=/usr/local
   ```

#### Integrating with build_cpp_dbc.sh

In `build_cpp_dbc.sh`, call the setup script before CMake configuration:

```bash
# In the dependency check section of build_cpp_dbc.sh
if [ "$USE_SCYLLADB" = "ON" ]; then
    echo "Checking for ScyllaDB/Cassandra driver..."

    if ! pkg-config --exists cassandra 2>/dev/null; then
        SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
        if [ -f "$SCRIPT_DIR/download_and_setup_cassandra_driver.sh" ]; then
            echo "ScyllaDB/Cassandra driver not found. Running setup script..."
            bash "$SCRIPT_DIR/download_and_setup_cassandra_driver.sh"
        else
            echo "Error: ScyllaDB driver not found and setup script missing."
            exit 1
        fi
    fi
fi
```

#### Distribution Package Considerations

When creating distro packages for drivers that require source compilation:

1. **In Dockerfile**: Include build dependencies but not the driver itself:
   ```dockerfile
   RUN apt-get install -y cmake make g++ libuv1-dev libssl-dev wget
   ```

2. **In build_script.sh**: Run the setup script before building cpp_dbc:
   ```bash
   if [ "__USE_SCYLLADB__" = "ON" ]; then
       /app/libs/cpp_dbc/download_and_setup_cassandra_driver.sh
   fi
   ```

#### Reference Implementation

See `libs/cpp_dbc/download_and_setup_cassandra_driver.sh` for a complete working example.

---

## Phase 3: Create Tests

### Test Directory Structure

**CRITICAL**: Tests are organized by database family following the same pattern as examples:

```text
libs/cpp_dbc/test/
├── common/                           # Core tests (10_*)
│   ├── 10_000_test_main.cpp
│   ├── 10_001_test_database_config.cpp
│   ├── 10_011_test_db_config.cpp
│   ├── 10_021_test_db_connection_pool_config.cpp
│   ├── 10_031_test_db_exception.cpp
│   ├── 10_041_test_expected.cpp
│   ├── 10_051_test_yaml.cpp
│   ├── 10_061_test_integration.cpp
│   ├── test_db_connections.yml
│   └── test.jpg
├── relational/
│   ├── mysql/                        # MySQL tests (20_*)
│   ├── postgresql/                   # PostgreSQL tests (21_*)
│   ├── sqlite/                       # SQLite tests (22_*)
│   └── firebird/                     # Firebird tests (23_*)
├── kv/
│   └── redis/                        # Redis tests (24_*)
├── document/
│   └── mongodb/                      # MongoDB tests (25_*)
├── columnar/
│   └── scylladb/                     # ScyllaDB tests (26_*)
├── graph/
│   └── .git_keep                     # Reserved for future
└── timeseries/
    └── .git_keep                     # Reserved for future
```

### Test File Naming Convention

Test files follow this pattern: `XX_YYY_test_<driver>_<category>.cpp`

Where:
- `XX` = Driver prefix (see table below)
- `YYY` = Sequential number within driver
- `<driver>` = Driver name (mysql, postgresql, sqlite, firebird, redis, mongodb, scylladb)
- `<category>` = Test category

#### Driver Prefix Assignment

| Prefix | Driver     | Family     | Directory                        |
|--------|------------|------------|----------------------------------|
| 10     | Core       | N/A        | `test/common/`                   |
| 20     | MySQL      | relational | `test/relational/mysql/`         |
| 21     | PostgreSQL | relational | `test/relational/postgresql/`    |
| 22     | SQLite     | relational | `test/relational/sqlite/`        |
| 23     | Firebird   | relational | `test/relational/firebird/`      |
| 24     | Redis      | kv         | `test/kv/redis/`                 |
| 25     | MongoDB    | document   | `test/document/mongodb/`         |
| 26     | ScyllaDB   | columnar   | `test/columnar/scylladb/`        |
| 27     | (Next)     | (Next)     | `test/<family>/<driver>/`        |

For a new relational driver like SQL Server, you would use prefix `27_` and place files in `test/relational/sqlserver/`.

#### Test Categories

**Standard Test Categories (XX_0YY):**

| Number | Category                    |
|--------|----------------------------|
| 001    | common (test helpers)      |
| 011    | db_config                  |
| 021    | driver                     |
| 031    | basic operations           |
| 041    | connection                 |
| 051    | json                       |
| 061    | blob                       |
| 071    | inner_join                 |
| 081    | left_join                  |
| 091    | right_join                 |
| 101    | full_join                  |
| 111    | thread_safe                |
| 121    | transaction_isolation      |
| 131    | transaction_manager        |
| 141    | connection_pool            |

**Exclusive Test Categories (XX_5YY):**

For tests that are specific to a database family and cannot be shared with other drivers, use the `5YY` range:

| Number | Category                       | Notes                                    |
|--------|-------------------------------|------------------------------------------|
| 521    | Driver-exclusive API tests    | Tests for family-specific APIs (e.g., MongoDB's `hasNext()`, `nextDocument()`) |
| 522-599| Reserved for future exclusive | Additional exclusive test categories     |

**Examples:**
- `25_521_test_mongodb_real_cursor_api.cpp` - Tests MongoDB-specific cursor methods
- `24_521_test_redis_real_pipeline_api.cpp` - Tests Redis-specific pipelining (hypothetical)

### Test Patterns by Family

#### Relational Database Tests

Required files in `test/relational/sqlserver/`:
```text
test/relational/sqlserver/27_001_test_sqlserver_real_common.cpp     # Test helpers
test/relational/sqlserver/27_001_test_sqlserver_real_common.hpp     # Test helpers header
test/relational/sqlserver/27_011_test_sqlserver_real_db_config.cpp
test/relational/sqlserver/27_021_test_sqlserver_real_driver.cpp
test/relational/sqlserver/27_031_test_sqlserver_real.cpp
test/relational/sqlserver/27_041_test_sqlserver_real_connection.cpp
test/relational/sqlserver/27_051_test_sqlserver_real_json.cpp
test/relational/sqlserver/27_061_test_sqlserver_real_blob.cpp
test/relational/sqlserver/27_071_test_sqlserver_real_inner_join.cpp
test/relational/sqlserver/27_081_test_sqlserver_real_left_join.cpp
test/relational/sqlserver/27_091_test_sqlserver_real_right_join.cpp
test/relational/sqlserver/27_101_test_sqlserver_real_full_join.cpp
test/relational/sqlserver/27_111_test_sqlserver_real_thread_safe.cpp
test/relational/sqlserver/27_121_test_sqlserver_real_transaction_isolation.cpp
test/relational/sqlserver/27_131_test_sqlserver_real_transaction_manager.cpp
test/relational/sqlserver/27_141_test_sqlserver_real_connection_pool.cpp
```

#### Key-Value Database Tests

Redis-style tests (simpler, no JOINs):
```
24_001_test_redis_real_common.cpp
24_021_test_redis_real_driver.cpp
24_041_test_redis_real_connection.cpp
24_141_test_redis_real_connection_pool.cpp
```

#### Document Database Tests

MongoDB-style tests:
```
25_001_test_mongodb_real_common.cpp
25_011_test_mongodb_real_db_config.cpp
25_021_test_mongodb_real_driver.cpp
25_031_test_mongodb_real.cpp
25_041_test_mongodb_real_connection.cpp
25_051_test_mongodb_real_json.cpp
25_061_test_mongodb_real_blob.cpp
25_071_test_mongodb_real_inner_join.cpp  # Aggregation pipeline
25_081_test_mongodb_real_left_join.cpp
25_091_test_mongodb_real_right_join.cpp
25_101_test_mongodb_real_full_join.cpp
25_111_test_mongodb_real_thread_safe.cpp
25_141_test_mongodb_real_connection_pool.cpp
```

#### Columnar Database Tests

ScyllaDB-style tests:
```
26_001_test_scylladb_real_common.cpp
26_011_test_scylladb_real_db_config.cpp
26_021_test_scylladb_real_driver.cpp
26_031_test_scylladb_real.cpp
26_041_test_scylladb_real_connection.cpp
26_051_test_scylladb_real_json.cpp
26_061_test_scylladb_real_blob.cpp
26_071_test_scylladb_real_inner_join.cpp
26_081_test_scylladb_real_left_join.cpp
26_091_test_scylladb_real_right_join.cpp
26_101_test_scylladb_real_full_join.cpp
26_111_test_scylladb_real_thread_safe.cpp
26_141_test_scylladb_real_connection_pool.cpp
```

### Update Test Configuration YAML

**Single file for all drivers**: `libs/cpp_dbc/test/test_db_connections.yml`

Add `dev_<driver>` and `test_<driver>` entries following the existing patterns in that file.

### Update test/CMakeLists.txt

**IMPORTANT**: Test files must use family-prefixed paths in CMakeLists.txt.

#### 1. Add the option:
```cmake
option(USE_SQLSERVER "Enable SQL Server support" OFF)
```

#### 2. Add the test files with family paths:
```cmake
# SQL Server tests (relational/sqlserver/)
relational/sqlserver/27_001_test_sqlserver_real_common.cpp
relational/sqlserver/27_011_test_sqlserver_real_db_config.cpp
relational/sqlserver/27_021_test_sqlserver_real_driver.cpp
relational/sqlserver/27_031_test_sqlserver_real.cpp
relational/sqlserver/27_041_test_sqlserver_real_connection.cpp
relational/sqlserver/27_051_test_sqlserver_real_json.cpp
relational/sqlserver/27_061_test_sqlserver_real_blob.cpp
relational/sqlserver/27_071_test_sqlserver_real_inner_join.cpp
relational/sqlserver/27_081_test_sqlserver_real_left_join.cpp
relational/sqlserver/27_091_test_sqlserver_real_right_join.cpp
relational/sqlserver/27_101_test_sqlserver_real_full_join.cpp
relational/sqlserver/27_111_test_sqlserver_real_thread_safe.cpp
relational/sqlserver/27_121_test_sqlserver_real_transaction_isolation.cpp
relational/sqlserver/27_131_test_sqlserver_real_transaction_manager.cpp
relational/sqlserver/27_141_test_sqlserver_real_connection_pool.cpp
```

#### 3. Add compile definition:
```cmake
$<$<BOOL:${USE_SQLSERVER}>:USE_SQLSERVER=1>
$<$<NOT:$<BOOL:${USE_SQLSERVER}>>:USE_SQLSERVER=0>
```

#### 4. Add include directories for test headers:
```cmake
# Add include directories (add after existing target_include_directories)
target_include_directories(cpp_dbc_tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../include
    ${CMAKE_CURRENT_SOURCE_DIR}/common
    ${CMAKE_CURRENT_SOURCE_DIR}/relational/mysql
    ${CMAKE_CURRENT_SOURCE_DIR}/relational/postgresql
    ${CMAKE_CURRENT_SOURCE_DIR}/relational/sqlite
    ${CMAKE_CURRENT_SOURCE_DIR}/relational/firebird
    ${CMAKE_CURRENT_SOURCE_DIR}/relational/sqlserver    # Add your new driver here
    ${CMAKE_CURRENT_SOURCE_DIR}/kv/redis
    ${CMAKE_CURRENT_SOURCE_DIR}/document/mongodb
    ${CMAKE_CURRENT_SOURCE_DIR}/columnar/scylladb
)
```

This allows test files to `#include "XX_001_test_<driver>_real_common.hpp"` without specifying the full path.

#### 5. Update file copy operations for common resources:
```cmake
# Copy the test_db_connections.yml file from common/
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/common/test_db_connections.yml
    ${CMAKE_CURRENT_BINARY_DIR}/test_db_connections.yml
    COPYONLY)

# Copy the test.jpg file from common/
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/common/test.jpg
    ${CMAKE_CURRENT_BINARY_DIR}/test.jpg
    COPYONLY)
```

#### 6. Update common integration tests (if applicable):

If you need to add support for the new driver in `common/10_061_test_integration.cpp`, wrap it with `USE_<DRIVER>`:

```cpp
#if USE_SQLSERVER
#include "27_001_test_sqlserver_real_common.hpp"
#endif

// Later in the test file:
#if USE_SQLSERVER
TEST_CASE("SQL Server Integration", "[10_061_07_sqlserver_integration]")
{
    auto driver = std::make_shared<cpp_dbc::SQLServerDBDriver>();
    // ... test code ...
}
#endif
```

---

## Phase 4: Create Benchmarks

### Benchmark File Structure

Benchmarks test CRUD operations:
```
benchmark_sqlserver_select.cpp
benchmark_sqlserver_insert.cpp
benchmark_sqlserver_update.cpp
benchmark_sqlserver_delete.cpp
```

#### Benchmark Naming Convention

Benchmark functions follow the pattern:
```cpp
static void BM_SQLServer_Select_Small(benchmark::State& state) { ... }
static void BM_SQLServer_Select_Medium(benchmark::State& state) { ... }
static void BM_SQLServer_Select_Large(benchmark::State& state) { ... }

BENCHMARK(BM_SQLServer_Select_Small);
BENCHMARK(BM_SQLServer_Select_Medium);
BENCHMARK(BM_SQLServer_Select_Large);
```

### Update Benchmark Configuration YAML

**Single file for all drivers**: `libs/cpp_dbc/benchmark/benchmark_db_connections.yml`

Add entries for your driver following the existing patterns.

### Update benchmark/CMakeLists.txt

Add the benchmark files:
```cmake
# SQL Server benchmarks
benchmark_sqlserver_select.cpp
benchmark_sqlserver_insert.cpp
benchmark_sqlserver_update.cpp
benchmark_sqlserver_delete.cpp
```

Add compile definition:
```cmake
$<$<BOOL:${USE_SQLSERVER}>:USE_SQLSERVER=1>
$<$<NOT:$<BOOL:${USE_SQLSERVER}>>:USE_SQLSERVER=0>
```

---

## Phase 5: Create Examples

Examples demonstrate how to use the new driver and serve as documentation for users. The examples vary significantly based on the driver family since each has a different API.

### Example Files by Family

#### Relational Database Examples

For relational databases (MySQL, PostgreSQL, SQLite, Firebird, SQL Server), create examples following the numeric naming convention `XX_YZZ_example_<db>_<feature>.cpp`:

```text
# Example structure for a new relational driver (e.g., SQL Server = family 27)
libs/cpp_dbc/examples/relational/sqlserver/27_001_example_sqlserver_basic.cpp
libs/cpp_dbc/examples/relational/sqlserver/27_021_example_sqlserver_connection_info.cpp
libs/cpp_dbc/examples/relational/sqlserver/27_031_example_sqlserver_connection_pool.cpp
libs/cpp_dbc/examples/relational/sqlserver/27_041_example_sqlserver_transaction_manager.cpp
libs/cpp_dbc/examples/relational/sqlserver/27_051_example_sqlserver_json.cpp
libs/cpp_dbc/examples/relational/sqlserver/27_061_example_sqlserver_blob.cpp
libs/cpp_dbc/examples/relational/sqlserver/27_071_example_sqlserver_join.cpp
libs/cpp_dbc/examples/relational/sqlserver/27_081_example_sqlserver_batch.cpp
libs/cpp_dbc/examples/relational/sqlserver/27_091_example_sqlserver_error_handling.cpp
```

**Naming Convention:**
- **XX**: Database family number (20=MySQL, 21=PostgreSQL, 22=SQLite, 23=Firebird, 27=new driver)
- **YZZ**: Feature category (001=basic, 021=connection_info, 031=pool, 041=transaction, 051=json, 061=blob, 071=join, 081=batch, 091=error_handling)

**Key operations to demonstrate:**
- Connection and configuration
- Basic CRUD (SELECT, INSERT, UPDATE, DELETE)
- Prepared statements with parameter binding
- Transaction management (BEGIN, COMMIT, ROLLBACK)
- JOIN operations
- BLOB/binary data handling
- JSON operations (if supported)

**Example structure:**
```cpp
#include <cpp_dbc/cpp_dbc.hpp>
#if USE_SQLSERVER
#include <cpp_dbc/drivers/relational/driver_sqlserver.hpp>
#endif

#if USE_SQLSERVER
void demonstrateSQLServer()
{
    // 1. Create driver and connect
    // 2. Create table
    // 3. Insert data
    // 4. Query data
    // 5. Update data
    // 6. Delete data
    // 7. Transaction example
    // 8. Prepared statement example
}
#endif

int main()
{
#if USE_SQLSERVER
    demonstrateSQLServer();
#else
    std::cout << "SQL Server support not enabled" << std::endl;
#endif
    return 0;
}
```

#### Key-Value Database Examples

For key-value stores (Redis), create examples following the numeric naming convention:

```text
# Example structure for a new KV driver (e.g., Memcached = family 28)
libs/cpp_dbc/examples/kv/memcached/28_001_example_memcached_basic.cpp
libs/cpp_dbc/examples/kv/memcached/28_021_example_memcached_connection_info.cpp
libs/cpp_dbc/examples/kv/memcached/28_031_example_memcached_connection_pool.cpp
libs/cpp_dbc/examples/kv/memcached/28_041_example_memcached_transaction.cpp
libs/cpp_dbc/examples/kv/memcached/28_061_example_memcached_blob.cpp
libs/cpp_dbc/examples/kv/memcached/28_081_example_memcached_batch.cpp
libs/cpp_dbc/examples/kv/memcached/28_091_example_memcached_error_handling.cpp
```

**Existing KV Examples:**
- Redis uses family 24 (24_xxx series)

**Key operations to demonstrate:**
- Connection (URL-based)
- String operations (SET, GET, DEL)
- Expiration (TTL)
- List operations (LPUSH, RPUSH, LRANGE)
- Hash operations (HSET, HGET, HGETALL)
- Set operations (SADD, SMEMBERS)

**Example structure:**
```cpp
#include <cpp_dbc/cpp_dbc.hpp>
#if USE_NEWKV
#include <cpp_dbc/drivers/kv/driver_newkv.hpp>
#endif

int main()
{
#if USE_NEWKV
    auto driver = std::make_shared<NewKVDriver>();
    auto conn = driver->connectKV("newkv://localhost:1234", "", "");

    // String operations
    conn->setString("key", "value");
    std::string val = conn->getString("key");

    // With expiration
    conn->setString("key_exp", "value", 60);

    // Other KV operations...
#endif
    return 0;
}
```

#### Document Database Examples

For document databases (MongoDB), create examples following the numeric naming convention:

```text
# Example structure for a new document driver (e.g., CouchDB = family 29)
libs/cpp_dbc/examples/document/couchdb/29_001_example_couchdb_basic.cpp
libs/cpp_dbc/examples/document/couchdb/29_021_example_couchdb_connection_info.cpp
libs/cpp_dbc/examples/document/couchdb/29_031_example_couchdb_connection_pool.cpp
libs/cpp_dbc/examples/document/couchdb/29_081_example_couchdb_batch.cpp
libs/cpp_dbc/examples/document/couchdb/29_091_example_couchdb_error_handling.cpp
```

**Existing Document Examples:**
- MongoDB uses family 25 (25_xxx series)

**Key operations to demonstrate:**
- Connection and database selection
- Collection operations
- Document CRUD (insert, find, update, delete)
- Query with filters
- Aggregation pipelines (for JOINs)
- BSON/JSON handling

**Example structure:**
```cpp
#include <cpp_dbc/cpp_dbc.hpp>
#if USE_NEWDOC
#include <cpp_dbc/drivers/document/driver_newdoc.hpp>
#endif

#if USE_NEWDOC
void demonstrateDocumentDB()
{
    auto driver = std::make_shared<NewDocDriver>();
    auto conn = driver->connectDocument("newdoc://localhost:27017", "database");

    // Insert document
    auto doc = conn->createDocument();
    doc->set("name", "John");
    doc->set("age", 30);
    conn->insertOne("collection", doc);

    // Query documents
    auto cursor = conn->find("collection", {});
    while (cursor->next()) {
        auto result = cursor->current();
        std::cout << result->toJson() << std::endl;
    }
}
#endif
```

#### Columnar Database Examples

For columnar/wide-column databases (ScyllaDB/Cassandra), create examples following the numeric naming convention:

```text
# Example structure for a new columnar driver (e.g., Cassandra = family 30)
libs/cpp_dbc/examples/columnar/cassandra/30_001_example_cassandra_basic.cpp
libs/cpp_dbc/examples/columnar/cassandra/30_021_example_cassandra_connection_info.cpp
libs/cpp_dbc/examples/columnar/cassandra/30_031_example_cassandra_connection_pool.cpp
libs/cpp_dbc/examples/columnar/cassandra/30_051_example_cassandra_json.cpp
libs/cpp_dbc/examples/columnar/cassandra/30_061_example_cassandra_blob.cpp
libs/cpp_dbc/examples/columnar/cassandra/30_081_example_cassandra_batch.cpp
libs/cpp_dbc/examples/columnar/cassandra/30_091_example_cassandra_error_handling.cpp
```

**Existing Columnar Examples:**
- ScyllaDB uses family 26 (26_xxx series)

**Key operations to demonstrate:**
- Connection with contact points
- Keyspace creation and selection
- Table creation with partition/clustering keys
- CQL queries (INSERT, SELECT, UPDATE, DELETE)
- Prepared statements
- BLOB handling
- JSON/map column types

**Example structure:**
```cpp
#include <cpp_dbc/cpp_dbc.hpp>
#if USE_NEWCOLUMNAR
#include <cpp_dbc/drivers/columnar/driver_newcolumnar.hpp>
#endif

#if USE_NEWCOLUMNAR
void demonstrateColumnarDB(std::shared_ptr<cpp_dbc::ColumnarDBConnection> conn)
{
    // Create keyspace
    conn->executeUpdate("CREATE KEYSPACE IF NOT EXISTS test_ks ...");

    // Create table with partition key
    conn->executeUpdate("CREATE TABLE test_ks.users (id int PRIMARY KEY, name text)");

    // Insert with prepared statement
    auto stmt = conn->prepareStatement("INSERT INTO test_ks.users (id, name) VALUES (?, ?)");
    stmt->setInt(1, 1);
    stmt->setString(2, "John");
    stmt->executeUpdate();

    // Query
    auto rs = conn->executeQuery("SELECT * FROM test_ks.users");
    while (rs->next()) {
        std::cout << rs->getInt("id") << ": " << rs->getString("name") << std::endl;
    }
}
#endif
```

#### Graph Database Examples (Future)

For graph databases, examples should demonstrate:
- Node creation
- Edge/relationship creation
- Graph traversal queries
- Path finding
- Pattern matching

#### Time-Series Database Examples (Future)

For time-series databases, examples should demonstrate:
- Time-based data insertion
- Time-range queries
- Aggregations over time windows
- Downsampling

### Update CMakeLists.txt for Examples

Add the example executables in `libs/cpp_dbc/CMakeLists.txt` following the numeric naming convention:

```cmake
# ============================================================================
# SQL Server Examples (27_xxx) - examples/relational/sqlserver/
# ============================================================================
if(USE_SQLSERVER AND CPP_DBC_BUILD_EXAMPLES)
    # Basic operations
    add_executable(27_001_example_sqlserver_basic
        examples/relational/sqlserver/27_001_example_sqlserver_basic.cpp)
    target_link_libraries(27_001_example_sqlserver_basic PRIVATE cpp_dbc)

    # Connection info
    add_executable(27_021_example_sqlserver_connection_info
        examples/relational/sqlserver/27_021_example_sqlserver_connection_info.cpp)
    target_link_libraries(27_021_example_sqlserver_connection_info PRIVATE cpp_dbc)

    # Connection pool
    add_executable(27_031_example_sqlserver_connection_pool
        examples/relational/sqlserver/27_031_example_sqlserver_connection_pool.cpp)
    target_link_libraries(27_031_example_sqlserver_connection_pool PRIVATE cpp_dbc)

    # Transaction manager
    add_executable(27_041_example_sqlserver_transaction_manager
        examples/relational/sqlserver/27_041_example_sqlserver_transaction_manager.cpp)
    target_link_libraries(27_041_example_sqlserver_transaction_manager PRIVATE cpp_dbc)

    # JSON operations
    add_executable(27_051_example_sqlserver_json
        examples/relational/sqlserver/27_051_example_sqlserver_json.cpp)
    target_link_libraries(27_051_example_sqlserver_json PRIVATE cpp_dbc)

    # BLOB operations
    add_executable(27_061_example_sqlserver_blob
        examples/relational/sqlserver/27_061_example_sqlserver_blob.cpp)
    target_link_libraries(27_061_example_sqlserver_blob PRIVATE cpp_dbc)

    # JOIN operations
    add_executable(27_071_example_sqlserver_join
        examples/relational/sqlserver/27_071_example_sqlserver_join.cpp)
    target_link_libraries(27_071_example_sqlserver_join PRIVATE cpp_dbc)

    # Batch operations
    add_executable(27_081_example_sqlserver_batch
        examples/relational/sqlserver/27_081_example_sqlserver_batch.cpp)
    target_link_libraries(27_081_example_sqlserver_batch PRIVATE cpp_dbc)

    # Error handling
    add_executable(27_091_example_sqlserver_error_handling
        examples/relational/sqlserver/27_091_example_sqlserver_error_handling.cpp)
    target_link_libraries(27_091_example_sqlserver_error_handling PRIVATE cpp_dbc)
endif()
```

**Important**:
- Wrap driver-specific examples in `if(USE_<DRIVER> AND CPP_DBC_BUILD_EXAMPLES)` blocks
- Use the numeric prefix system: `XX_YZZ_example_<db>_<feature>.cpp`
- Organize by database family folder: `examples/relational/`, `examples/kv/`, `examples/document/`, `examples/columnar/`
- For a new relational driver, use the next available family number (e.g., 27 for SQL Server)

---

## Checklist Summary

### Phase 1: Driver Files
- [ ] Created `libs/cpp_dbc/include/cpp_dbc/drivers/<family>/driver_<name>.hpp`
- [ ] Created `libs/cpp_dbc/include/cpp_dbc/drivers/<family>/<name>/` with all .hpp files
- [ ] Created `libs/cpp_dbc/src/drivers/<family>/<name>/` with all .cpp files
- [ ] All files follow C++17+ conventions and project coding standards
- [ ] RAII handles for all external resources
- [ ] Thread safety with `DB_DRIVER_LOCK_GUARD(m_mutex)` macro
- [ ] Updated `libs/cpp_dbc/include/cpp_dbc/cpp_dbc.hpp`
  - [ ] Added `#ifndef USE_<DRIVER>` macro definition
  - [ ] Added `#if USE_<DRIVER>` conditional include

### Phase 2: Build Configuration
- [ ] Updated `libs/cpp_dbc/CMakeLists.txt`
  - [ ] Added option
  - [ ] Added compile definitions
  - [ ] Added source files
  - [ ] Added find_package
  - [ ] Added to install files
- [ ] Created `libs/cpp_dbc/cmake/Find<Name>.cmake`
- [ ] Updated `libs/cpp_dbc/cmake/cpp_dbc-config.cmake`
  - [ ] Added `CPP_DBC_USE_<DRIVER>` variable
  - [ ] Added compile definitions
  - [ ] Added find_package and linking
- [ ] Updated `libs/cpp_dbc/cmake/FindCPPDBC.cmake`
  - [ ] Added driver detection and linking
- [ ] Updated `libs/cpp_dbc/benchmark/benchmark_common.hpp`
  - [ ] Added `#if USE_<DRIVER>` include
- [ ] Updated `libs/cpp_dbc/build_cpp_dbc.sh`
- [ ] Updated `libs/cpp_dbc/build_test_cpp_dbc.sh`
- [ ] Updated `libs/cpp_dbc/run_test_cpp_dbc.sh`
- [ ] Updated `libs/cpp_dbc/run_benchmarks_cpp_dbc.sh`
- [ ] Updated `libs/cpp_dbc/build_dist_pkg.sh`
- [ ] Updated root `helper.sh`
  - [ ] Added driver option to `show_usage()` function
  - [ ] Added driver option to `cmd_run_build()` function
  - [ ] Added driver option to `cmd_run_test()` function
  - [ ] Added driver option to `cmd_run_benchmarks()` function
  - [ ] Updated `--mc-combo-XX` shortcuts to include new driver
  - [ ] Updated `--bk-combo-XX` shortcuts to include new driver
- [ ] Updated root `run_test.sh`
- [ ] Updated ALL distro folders:
  - [ ] `distros/debian_12/Dockerfile`
  - [ ] `distros/debian_12/build_script.sh`
  - [ ] `distros/debian_13/Dockerfile`
  - [ ] `distros/debian_13/build_script.sh`
  - [ ] `distros/ubuntu_22_04/Dockerfile`
  - [ ] `distros/ubuntu_22_04/build_script.sh`
  - [ ] `distros/ubuntu_24_04/Dockerfile`
  - [ ] `distros/ubuntu_24_04/build_script.sh`
  - [ ] `distros/fedora_42/Dockerfile`
  - [ ] `distros/fedora_42/build_script.sh`
  - [ ] `distros/fedora_43/Dockerfile`
  - [ ] `distros/fedora_43/build_script.sh`
- [ ] (If no official package) Created `libs/cpp_dbc/download_and_setup_<driver>_driver.sh`
  - [ ] Checks for existing installation (pkg-config, headers, libraries)
  - [ ] Installs build dependencies for apt-get, dnf, yum
  - [ ] Downloads, builds, and installs to /usr/local
  - [ ] Integrated with build_cpp_dbc.sh

#### ⚠️ CHECKPOINT: Validate Before Phase 3

**DO NOT proceed to Phase 3 until you verify the library compiles successfully.**

```bash
# Option 1: Use a combo shortcut (after updating it to include the new driver)
./helper.sh --mc-combo-01

# Option 2: Use --run-build with specific options for the new driver
./helper.sh --run-build=clean,<new_driver>,test
```

- [ ] Library compiles without errors
- [ ] No warnings with `-Wall -Wextra -Wpedantic`
- [ ] Driver can be instantiated (basic smoke test)

### Phase 3: Tests
- [ ] Compiles without warnings with `-Wall -Wextra -Wpedantic`
- [ ] Created test directory structure: `test/<family>/<driver>/`
  - Relational: `test/relational/<driver>/`
  - KV: `test/kv/<driver>/`
  - Document: `test/document/<driver>/`
  - Columnar: `test/columnar/<driver>/`
- [ ] Created test helper files (`XX_001_...common.cpp/hpp`) in appropriate family directory
- [ ] Created all required test files in appropriate family directory
- [ ] Updated `libs/cpp_dbc/test/CMakeLists.txt`
  - [ ] Added option for the new driver
  - [ ] Added test files with family-prefixed paths (e.g., `relational/sqlserver/27_001_...`)
  - [ ] Added compile definitions
  - [ ] Added include directory for the new driver (e.g., `${CMAKE_CURRENT_SOURCE_DIR}/relational/sqlserver`)
- [ ] Updated `common/10_061_test_integration.cpp` (if applicable)
  - [ ] Added `#if USE_<DRIVER>` include for driver's common header
  - [ ] Added integration test case wrapped in `#if USE_<DRIVER>`
- [ ] Added entries to `libs/cpp_dbc/test/common/test_db_connections.yml` (single file in common/, add new driver entries)
- [ ] All tests pass

#### ⚠️ CHECKPOINT: Validate Before Phase 4

**DO NOT proceed to Phase 4 until all tests pass.**

```bash
# Option 1: Use a combo shortcut (after updating it to include the new driver)
./helper.sh --bk-combo-01

# Option 2: Use --run-test with specific options for the new driver
./helper.sh --run-test=rebuild,<new_driver>,auto,run=1
```

- [ ] All tests compile without errors
- [ ] All tests pass (no failures)
- [ ] No memory leaks detected (if using valgrind)

### Phase 4: Benchmarks
- [ ] Compiles without warnings
- [ ] Created all benchmark files (select, insert, update, delete)
- [ ] Updated `libs/cpp_dbc/benchmark/CMakeLists.txt`
- [ ] Added entries to `libs/cpp_dbc/benchmark/benchmark_db_connections.yml` (single file, add new driver entries)
- [ ] All benchmarks run successfully

#### ⚠️ CHECKPOINT: Validate Before Phase 5

**DO NOT proceed to Phase 5 until all benchmarks run successfully.**

```bash
# Use a combo shortcut for benchmarks
./helper.sh --dk-combo-01

# Or use --run-benchmarks with specific options
./helper.sh --run-benchmarks=rebuild,<new_driver>,memory-usage
```

- [ ] All benchmarks compile without errors
- [ ] All benchmarks execute successfully
- [ ] Performance is reasonable for the driver

### Phase 5: Examples
- [ ] Created example files following numeric naming convention `XX_YZZ_example_<db>_<feature>.cpp`
  - [ ] Basic example: `XX_001_example_<driver>_basic.cpp`
  - [ ] Connection info: `XX_021_example_<driver>_connection_info.cpp`
  - [ ] Connection pool: `XX_031_example_<driver>_connection_pool.cpp`
  - [ ] Additional feature examples (transaction, JSON, BLOB, JOIN, batch, error handling) as applicable
- [ ] Created examples in appropriate family folder:
  - Relational: `examples/relational/<driver>/`
  - KV: `examples/kv/<driver>/`
  - Document: `examples/document/<driver>/`
  - Columnar: `examples/columnar/<driver>/`
- [ ] Updated `libs/cpp_dbc/CMakeLists.txt` to add example executables
- [ ] Examples wrapped in `if(USE_<DRIVER> AND CPP_DBC_BUILD_EXAMPLES)` blocks
- [ ] Examples compile and run successfully
- [ ] Examples are well-documented with comments
- [ ] Used next available family number (e.g., 27 for SQL Server)

---

## Common Mistakes to Avoid

### 1. Forgetting Distro Files
The most common mistake is updating the main build scripts but forgetting the distro folder files:
- `libs/cpp_dbc/distros/<distro>/Dockerfile`
- `libs/cpp_dbc/distros/<distro>/build_script.sh`

### 2. Inconsistent Option Names
Ensure the option name is consistent across all files:
- CMake: `USE_SQLSERVER`
- Shell: `--sqlserver`, `--sqlserver-on`, `--sqlserver-off`
- Debug: `DEBUG_SQLSERVER`

### 3. Missing Compile Definitions
Don't forget to add compile definitions in:
- `libs/cpp_dbc/CMakeLists.txt`
- `libs/cpp_dbc/test/CMakeLists.txt`
- `libs/cpp_dbc/benchmark/CMakeLists.txt`

### 4. Missing Debug Options
If your driver needs debug output, add:
- `DEBUG_<DRIVER>` option to CMake
- `--debug-<driver>` option to shell scripts
- Handler in `helper.sh`

### 5. Not Using RAII
Always use RAII handles for external resources:
```cpp
// Good - RAII handle
using SQLServerStmtHandle = std::unique_ptr<SQLHSTMT, SQLServerStmtDeleter>;

// Bad - raw handle
SQLHSTMT stmt;  // Must remember to free manually
```

### 6. Thread Safety
Use the macro for conditional locking:
```cpp
DB_DRIVER_LOCK_GUARD(m_mutex);
```

### 7. DBException Error Codes

> **See also**: [error_handling_patterns.md](error_handling_patterns.md) for complete patterns including exception-based and nothrow API usage.

Error codes must be **12-character uppercase alphanumeric** strings with:
- At least 5 letters (A-Z)
- No more than 4 consecutive repeated characters
- Unique across the entire project

```cpp
// Correct - generated code
throw DBException("7K3F9J2B5Z8D", "Connection failed", system_utils::captureCallStack());

// Incorrect examples
throw DBException("INVALID_KEY", ...);   // Not alphanumeric only
throw DBException("001", ...);           // Too short
throw DBException("AAAAAB123456", ...);  // 5 consecutive 'A's
```

Generate unique codes with:
```bash
./generate_dbexception_code.sh /path/to/project      # Generate 1 code
./generate_dbexception_code.sh /path/to/project 10   # Generate 10 codes
```

See `generate_dbexception_code.sh` for implementation details.

### 8. Missing Source Compilation Script
For drivers without official `.deb`/`.rpm` packages, you must create a `download_and_setup_<driver>_driver.sh` script. Don't assume the developer will manually install the driver. See [Source Compilation Fallback](#source-compilation-fallback).

### 9. Forgetting Combo Commands in helper.sh
The root `helper.sh` has combo shortcuts that build/test multiple drivers at once:
- `--mc-combo-01`, `--mc-combo-02`, etc. (build combos)
- `--bk-combo-01`, `--bk-combo-02`, etc. (benchmark combos)

When adding a new driver, update these combos to include support for compiling and testing with the new database.

### 10. Missing or Incomplete Examples
Examples are essential documentation for users. Don't skip Phase 5:
- Each driver needs examples following the numeric naming convention `XX_YZZ_example_<db>_<feature>.cpp`
- At minimum, create basic and connection pool examples
- Examples must be in appropriate family folder: `examples/<family>/<driver>/`
- Examples must be wrapped in `#if USE_<DRIVER>` preprocessor guards
- Examples must be added to `CMakeLists.txt` inside `if(USE_<DRIVER> AND CPP_DBC_BUILD_EXAMPLES)` blocks
- The API demonstrated should match the driver's family (don't show SQL for a KV store)
- Use the next available family number for the new driver

### 11. Forgetting CMake Config Files
The installed package configuration files also need updates:
- `libs/cpp_dbc/cmake/cpp_dbc-config.cmake` - For package consumers
- `libs/cpp_dbc/cmake/FindCPPDBC.cmake` - For fallback discovery
- `libs/cpp_dbc/benchmark/benchmark_common.hpp` - For benchmark includes

Without these updates, users who install cpp_dbc as a package won't be able to use the new driver.

---

## See Also

For detailed information on specific topics, see these companion documents:

| Document | When to Use |
|----------|-------------|
| [shell_script_dependencies.md](shell_script_dependencies.md) | Understanding script relationships during Phase 2 |
| [error_handling_patterns.md](error_handling_patterns.md) | Implementing error handling during Phase 1 |

---

## Contact and Support

For questions about adding new drivers, consult existing implementations:
- Relational: `libs/cpp_dbc/include/cpp_dbc/drivers/relational/mysql/`
- Document: `libs/cpp_dbc/include/cpp_dbc/drivers/document/mongodb/`
- Key-Value: `libs/cpp_dbc/include/cpp_dbc/drivers/kv/redis/`
- Columnar: `libs/cpp_dbc/include/cpp_dbc/drivers/columnar/scylladb/`
