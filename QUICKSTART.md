# CPP_DBC Quick Start Guide

## Common Helper Script Commands

This guide provides quick examples for common operations using the helper.sh script.

### Predefined Test Combinations

```bash
# Run tests with rebuild, SQLite, PostgreSQL, MySQL, YAML, Valgrind, auto mode, run once
./helper.sh --bk-combo-01

# Run tests with rebuild, SQLite, PostgreSQL, MySQL, YAML, auto mode, run once (no Valgrind)
./helper.sh --bk-combo-02

# Run tests with SQLite, PostgreSQL, MySQL, YAML, Valgrind, auto mode, run once (no rebuild)
./helper.sh --bk-combo-03

# Run tests with SQLite, PostgreSQL, MySQL, YAML, auto mode, run once (no rebuild, no Valgrind)
./helper.sh --bk-combo-04
```

### Predefined Build Combinations

```bash
# Clean build with PostgreSQL, MySQL, SQLite, YAML, tests, and examples
./helper.sh --mc-combo-01

# Build with PostgreSQL, SQLite, MySQL, YAML, tests, and examples (no clean)
./helper.sh --mc-combo-02

# Clean build of distribution with PostgreSQL, MySQL, SQLite, YAML, tests, and examples
./helper.sh --kfc-combo-01

# Build distribution with PostgreSQL, SQLite, MySQL, YAML, tests, and examples (no clean)
./helper.sh --kfc-combo-02
```

For more detailed information, run:

```bash
./helper.sh
```

This will display the full usage information with all available options.

### Build Operations

```bash
# Run tests with rebuild, valgrind, SQLite, PostgreSQL, MySQL, YAML, auto mode, run once
./helper.sh --run-test=rebuild,valgrind,sqlite,postgres,mysql,yaml,auto,run=1

# Build with MySQL support only (default in Debug mode)
./helper.sh --run-build

# Enable PostgreSQL support
./helper.sh --run-build=postgres

# Enable both MySQL and PostgreSQL
./helper.sh --run-build=mysql,postgres

# Enable SQLite support
./helper.sh --run-build=sqlite

# Enable all database drivers
./helper.sh --run-build=mysql,postgres,sqlite

# Build in Release mode with all drivers and YAML support
./helper.sh --run-build=release,mysql,postgres,sqlite,yaml

# Clean build and rebuild with all features
./helper.sh --run-build=clean,mysql,postgres,sqlite,yaml,test,examples
```

All build output is now automatically logged to files in the `logs/build/` directory with timestamps in the filenames. The system automatically rotates logs, keeping the 4 most recent files.

### Test Operations

```bash
# Run tests with default settings
./helper.sh --run-test

# Run tests with SQLite support
./helper.sh --run-test=sqlite

# Run tests with PostgreSQL support
./helper.sh --run-test=postgres

# Run tests with all database drivers
./helper.sh --run-test=sqlite,postgres,mysql

# Run tests with Valgrind memory checking
./helper.sh --run-test=valgrind

# Run tests with Address Sanitizer
./helper.sh --run-test=asan

# Run tests with CTest
./helper.sh --run-test=ctest

# Force rebuild of tests before running
./helper.sh --run-test=rebuild

# Run tests multiple times
./helper.sh --run-test=run=3

# Run specific tests by tag
./helper.sh --run-test=test=sqlite_real_inner_join

# Run multiple specific tests by tag
./helper.sh --run-test=test=sqlite_real_inner_join+sqlite_real_left_join

# Run JSON-specific tests
./helper.sh --run-test=test=mysql_real_json+postgresql_real_json

# Run tests with debug output for ConnectionPool
./helper.sh --run-test=debug-pool

# Run tests with debug output for TransactionManager
./helper.sh --run-test=debug-txmgr

# Run tests with debug output for SQLite driver
./helper.sh --run-test=debug-sqlite

# Run tests with all debug output enabled
./helper.sh --run-test=debug-all

# Combine options: Run specific tests with debug output
./helper.sh --run-test=test=sqlite_real_inner_join,debug-sqlite,debug-pool
```

All test output is now automatically logged to files in the `logs/test/` directory with timestamps in the filenames. The system automatically rotates logs, keeping the 4 most recent files.

### Test Log Analysis

```bash
# Check the most recent test log file for failures and memory issues
./helper.sh --check-test-log

# Check a specific test log file
./helper.sh --check-test-log=logs/test/output-20251109-152030.log
```

The test log analysis feature checks for:
- Test failures from Catch2 output
- Memory leaks from Valgrind output
- Valgrind errors from ERROR SUMMARY

### Container Operations

```bash
# Run the container
./helper.sh --run-ctr [container_name]

# Show container labels
./helper.sh --show-labels-ctr [container_name]

# Show container tags
./helper.sh --show-tags-ctr [container_name]

# Show container environment variables
./helper.sh --show-env-ctr [container_name]

# Run ldd on the executable inside the container
./helper.sh --ldd-bin-ctr [container_name]
```

### Local Binary Operations

```bash
# Run ldd on the local build executable
./helper.sh --ldd-build-bin

# Run the local build executable
./helper.sh --run-build-bin
```

### Utility Operations

```bash
# Clear Conan cache
./helper.sh --clean-conan-cache
```

### Combining Multiple Commands

```bash
# Clean Conan cache, then build with all features
./helper.sh --clean-conan-cache --run-build=clean,mysql,postgres,sqlite,yaml,test,examples

# Build and then run tests
./helper.sh --run-build=mysql,postgres,sqlite --run-test=mysql,postgres,sqlite

# Build distribution and check container labels
./helper.sh --kfc-combo-01 --show-labels-ctr
```

## Examples for Specific Tasks

### Complete Development Setup

```bash
# Clean everything and set up a full development environment
./helper.sh --clean-conan-cache --run-build=clean,mysql,postgres,sqlite,yaml,test,examples
```

### Quick Test Run

```bash
# Run a quick test with SQLite only
./helper.sh --run-test=sqlite,auto
```

### Debug Specific Component

```bash
# Debug SQLite JOIN operations with debug output
./helper.sh --run-test=rebuild,mysql,sqlite,postgres,test=sqlite_real_inner_join+sqlite_real_left_join,debug-sqlite

# Test JSON functionality in MySQL and PostgreSQL
./helper.sh --run-test=rebuild,mysql,postgres,test=mysql_real_json+postgresql_real_json
```

### Production Build

```bash
# Create a production-ready build
./helper.sh --run-build=clean,release,mysql,postgres,sqlite,yaml
```

### Container Deployment

```bash
# Build a container for deployment
./helper.sh --kfc-combo-01
```

### VSCode Integration

The project now includes VSCode configuration files in the `.vscode` directory:

```bash
# Build using VSCode tasks (Ctrl+Shift+B)
# Options:
# - "CMake: build" - Default build task
# - "Build with C++ Properties" - Build using preprocessor definitions from c_cpp_properties.json
```

The "Auto install extensions" task runs automatically when opening the folder and installs recommended extensions.

