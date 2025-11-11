# CPP_DBC Technical Context

## Technologies Used

### Programming Language
- **C++23**: The library uses modern C++ features including:
  - Smart pointers (`std::shared_ptr`)
  - Lambda expressions
  - Thread support library
  - Chrono library for time management
  - Atomic operations
  - Move semantics
  - Other C++23 features as needed
  - `<cstdint>` for fixed-width integer types (used in BLOB handling)

### Database Client Libraries
- **MySQL Client Library**: For MySQL database connectivity
  - Uses the C API (`mysql.h`)
  - Requires libmysqlclient development package
  
- **PostgreSQL Client Library**: For PostgreSQL database connectivity
  - Uses the C API (`libpq-fe.h`)
  - Requires libpq development package

- **YAML-CPP Library**: For YAML configuration support (optional)
  - Used for parsing YAML configuration files
  - Included via Conan dependency management
  - Only compiled when USE_CPP_YAML is enabled

### Threading
- **Standard C++ Thread Library**: For multi-threading support
  - `std::thread` for background maintenance tasks
  - `std::mutex` and `std::lock_guard` for thread synchronization
  - `std::condition_variable` for thread signaling
  - `std::atomic` for thread-safe counters

## Development Setup

### Required Packages
- C++ compiler with C++23 support (GCC, Clang, MSVC)
- MySQL development libraries (for MySQL support)
- PostgreSQL development libraries (for PostgreSQL support)
- CMake (for build system)
- Conan (for dependency management)

### Build System
The project uses:
- CMake for cross-platform build configuration
- Conan for dependency management
- Custom build scripts (`build.sh`, `libs/cpp_dbc/build_cpp_dbc.sh`, and `build.dist.sh`) for simplified building
- Debug mode by default, with option for Release mode
- Conditional compilation options:
  - `--yaml`: Enable YAML configuration support
  - `--examples`: Build example applications
  - `--test`: Build unit tests

### Development Environment
The code is structured to be developed in any C++ IDE or text editor, with common options being:
- Visual Studio Code with C++ extensions and CMakeTools
- CLion
- Visual Studio
- Eclipse with CDT

#### VSCode Configuration
The project includes VSCode configuration files for seamless development:
- `.vscode/settings.json`: Configures CMake to use Debug mode by default (without using presets)
- `.vscode/c_cpp_properties.json`: Sets up include paths for IntelliSense
- `.vscode/launch.json`: Provides debugging configurations (standard and CMake-based)
- `.vscode/tasks.json`: Defines build tasks including "CMake: build"

The project is configured to work with the CMakeTools extension, but does not rely on CMake presets to avoid configuration issues. Instead, it uses direct configuration settings in the VSCode files.

#### Known VSCode Issues
- **IntelliSense Preprocessor Definition Caching**: IntelliSense may show `USE_POSTGRESQL` as 0 even after compilation has activated it. This can cause confusion when working with conditional code. To fix this issue:
  1. After compilation, press `CTRL+SHIFT+P`
  2. Type and select "Developer: Reload Window"
  3. This forces IntelliSense to reload with the updated preprocessor definitions

## Technical Constraints

### Database Support
- Currently supports MySQL, PostgreSQL, and SQLite
- Adding support for other databases requires implementing new driver classes

### Thread Safety
- Connection objects are not thread-safe and should not be shared between threads
- ConnectionPool and TransactionManager are thread-safe
- Applications must handle their own thread synchronization when sharing data between threads

### Memory Management
- Uses smart pointers for automatic resource management
- Relies on RAII for proper cleanup
- No explicit memory management required from client code

### Error Handling
- Uses exceptions for error propagation
- Client code should handle SQLException appropriately

## Dependencies

### External Libraries
- **MySQL Client Library**:
  - Required for MySQL support
  - Typically installed via package manager (e.g., `libmysqlclient-dev` on Debian/Ubuntu)
  - Header: `mysql/mysql.h`

- **PostgreSQL Client Library**:
  - Required for PostgreSQL support
  - Typically installed via package manager (e.g., `libpq-dev` on Debian/Ubuntu)
  - Header: `libpq-fe.h`

- **YAML-CPP Library**:
  - Optional dependency for YAML configuration support
  - Managed via Conan package manager
  - Only required when building with `--yaml` option
  - Header: `yaml-cpp/yaml.h`

### Standard Library Dependencies
- `<string>`: For string handling
- `<vector>`: For dynamic arrays
- `<memory>`: For smart pointers
- `<map>`: For associative containers
- `<stdexcept>`: For standard exceptions
- `<mutex>`: For thread synchronization
- `<condition_variable>`: For thread signaling
- `<thread>`: For multi-threading
- `<atomic>`: For atomic operations
- `<chrono>`: For time management
- `<functional>`: For function objects
- `<random>`: For random number generation (used in UUID generation)
- `<cstdint>`: For fixed-width integer types (used in BLOB handling)
- `<fstream>`: For file I/O (used in file-based BLOB implementations)

## Tool Usage Patterns

### Connection Management
- Connections should be obtained from a ConnectionPool rather than created directly
- Connections should be closed when no longer needed to return them to the pool
- Smart pointers ensure connections are properly returned to the pool

### Transaction Handling
- Transactions should be managed through the TransactionManager for thread safety
- Transaction IDs should be passed between components that need to work within the same transaction
- Explicit commit or rollback is required to complete a transaction
- Transaction isolation levels can be set on connections to control concurrency behavior
- Default isolation levels are database-specific (REPEATABLE READ for MySQL, READ COMMITTED for PostgreSQL)

### Query Execution
- Prepared statements should be used for parameterized queries
- Direct query execution should be limited to simple, non-parameterized queries
- Result sets should be processed in a timely manner to avoid resource leaks
- BLOB data should be handled using the provided BLOB interfaces for consistency
- JSON data should be handled using the database-specific JSON functions and operators

### BLOB Handling
- Use the Blob interface for working with binary data
- Use InputStream and OutputStream for streaming binary data
- Use getBlob(), getBinaryStream(), and getBytes() methods for retrieving BLOB data
- Use setBlob(), setBinaryStream(), and setBytes() methods for storing BLOB data
- Consider memory usage when working with large BLOBs

### JSON Handling
- Use the appropriate JSON functions and operators for each database:
  - MySQL: JSON_EXTRACT, JSON_SEARCH, JSON_CONTAINS, JSON_ARRAYAGG
  - PostgreSQL: JSON operators (@>, <@, ?, ?|, ?&), jsonb_set, jsonb_insert
- Use prepared statements with JSON parameters for safe JSON data manipulation
- Consider indexing JSON fields for performance in query-intensive applications
- Be aware of database-specific JSON syntax differences
- Use JSON validation functions to ensure data integrity

### Error Handling
- SQLException should be caught and handled appropriately
- Connection errors should be handled with retry logic when appropriate
- Transaction errors should trigger rollback

### Configuration Management
- YAML configuration files should be used for managing database settings
- Configuration should be loaded at application startup
- Database configurations can be retrieved by name
- Connection pool configurations can be customized in the YAML file
- Test queries can be defined in the YAML file for different database types
