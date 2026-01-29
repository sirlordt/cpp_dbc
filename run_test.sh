#!/bin/bash
set -e  # Exit on error

# Script: run_test.sh
# Description: Runs the cpp_dbc library tests
# Usage: ./run_test.sh [options]
# Options:
#   --mysql, --mysql-on    Enable MySQL support (default)
#   --mysql-off            Disable MySQL support
#   --postgres, --postgres-on  Enable PostgreSQL support
#   --postgres-off         Disable PostgreSQL support
#   --sqlite, --sqlite-on  Enable SQLite support
#   --sqlite-off           Disable SQLite support
#   --firebird, --firebird-on  Enable Firebird support
#   --firebird-off         Disable Firebird support
#   --mongodb, --mongodb-on  Enable MongoDB support
#   --mongodb-off          Disable MongoDB support
#   --redis, --redis-on    Enable Redis support
#   --redis-off            Disable Redis support
#   --yaml, --yaml-on      Enable YAML configuration support
#   --yaml-off             Disable YAML configuration support
#   --auto                 Automatically run tests without user interaction
#   --release              Run in Release mode (default: Debug)
#   --asan                 Enable Address Sanitizer
#   --valgrind             Run tests with Valgrind
#   --ctest                Run tests using CTest
#   --check                Check shared library dependencies of test executable
#   --clean                Clean build directories before building
#   --rebuild              Rebuild the test targets before running
#   --skip-build           Skip the build step
#   --list                 List tests only (do not run)
#   --run-test="tag"       Run only tests with the specified tag (use + to separate multiple tags, e.g. "tag1+tag2+tag3")
#   --debug-pool           Enable debug output for ConnectionPool
#   --debug-txmgr          Enable debug output for TransactionManager
#   --debug-sqlite         Enable debug output for SQLite driver
#   --debug-mongodb        Enable debug output for MongoDB driver
#   --debug-redis          Enable debug output for Redis driver
#   --debug-all            Enable all debug output
#   --dw-off               Disable libdw support for stack traces
#   --db-driver-thread-safe-off  Disable thread-safe database driver operations
#   --help                 Show this help message

# Default values for options
USE_MYSQL=ON
USE_POSTGRESQL=OFF
USE_SQLITE=OFF
USE_FIREBIRD=OFF
USE_MONGODB=OFF
USE_SCYLLADB=OFF
USE_REDIS=OFF
USE_YAML=ON
BUILD_TYPE=Debug
ENABLE_GCC_ANALYZER=OFF
ASAN_OPTIONS=""
ENABLE_ASAN=false
RUN_CTEST=false
USE_VALGRIND=false
CHECK_DEPENDENCIES=false
REBUILD=false
AUTO_MODE=false
RUN_COUNT=1
RUN_SPECIFIC_TEST=""
DEBUG_CONNECTION_POOL=OFF
DEBUG_TRANSACTION_MANAGER=OFF
DEBUG_SQLITE=OFF
DEBUG_FIREBIRD=OFF
DEBUG_MONGODB=OFF
DEBUG_SCYLLADB=OFF
DEBUG_REDIS=OFF
DEBUG_ALL=OFF
DW_OFF=false
DB_DRIVER_THREAD_SAFE_OFF=false
SHOW_PROGRESS=false
SKIP_BUILD=false
LIST_ONLY=false

# Parse command line arguments
while [[ $# -gt 0 ]]; do
    case $1 in
        --mysql|--mysql-on)
            USE_MYSQL=ON
            shift
            ;;
        --mysql-off)
            USE_MYSQL=OFF
            shift
            ;;
        --postgres|--postgres-on)
            USE_POSTGRESQL=ON
            shift
            ;;
        --postgres-off)
            USE_POSTGRESQL=OFF
            shift
            ;;
        --sqlite|--sqlite-on)
            USE_SQLITE=ON
            shift
            ;;
        --sqlite-off)
            USE_SQLITE=OFF
            shift
            ;;
        --firebird|--firebird-on)
            USE_FIREBIRD=ON
            shift
            ;;
        --firebird-off)
            USE_FIREBIRD=OFF
            shift
            ;;
        --mongodb|--mongodb-on)
            USE_MONGODB=ON
            shift
            ;;
        --mongodb-off)
            USE_MONGODB=OFF
            shift
            ;;
        --scylladb|--scylladb-on)
            USE_SCYLLADB=ON
            shift
            ;;
        --scylladb-off)
            USE_SCYLLADB=OFF
            shift
            ;;
        --redis|--redis-on)
            USE_REDIS=ON
            shift
            ;;
        --redis-off)
            USE_REDIS=OFF
            shift
            ;;
        --yaml|--yaml-on)
            USE_YAML=ON
            shift
            ;;
        --yaml-off)
            USE_YAML=OFF
            shift
            ;;
        --auto)
            AUTO_MODE=true
            shift
            ;;
        --release)
            BUILD_TYPE=Release
            shift
            ;;
        --gcc-analyzer)
            ENABLE_GCC_ANALYZER=ON
            shift
            ;;
        --asan)
            ASAN_OPTIONS="handle_segv=0:allow_user_segv_handler=1:detect_leaks=1:print_stacktrace=1:halt_on_error=0"
            ENABLE_ASAN=true
            shift
            ;;
        --valgrind)
            USE_VALGRIND=true
            shift
            ;;
        --ctest)
            RUN_CTEST=true
            shift
            ;;
        --check)
            CHECK_DEPENDENCIES=true
            shift
            ;;
        --clean)
            # Clean build directories before building
            echo "Cleaning build directories..."
            rm -rf build
            rm -rf libs/cpp_dbc/build
            REBUILD=true
            shift
            ;;
        --rebuild)
            REBUILD=true
            shift
            ;;
        --skip-build)
            SKIP_BUILD=true
            shift
            ;;
        --list)
            LIST_ONLY=true
            shift
            ;;
        --run=*)
            RUN_COUNT="${1#*=}"
            # Validate that RUN_COUNT is a positive integer
            if ! [[ "$RUN_COUNT" =~ ^[1-9][0-9]*$ ]]; then
                echo "Error: --run parameter must be a positive integer."
                exit 1
            fi
            shift
            ;;
        --run-test=*)
            RUN_SPECIFIC_TEST="${1#*=}"
            shift
            ;;
        --debug-pool)
            DEBUG_CONNECTION_POOL=ON
            shift
            ;;
        --debug-txmgr)
            DEBUG_TRANSACTION_MANAGER=ON
            shift
            ;;
        --debug-sqlite)
            DEBUG_SQLITE=ON
            shift
            ;;
        --debug-firebird)
            DEBUG_FIREBIRD=ON
            shift
            ;;
        --debug-mongodb)
            DEBUG_MONGODB=ON
            shift
            ;;
        --debug-scylladb)
            DEBUG_SCYLLADB=ON
            shift
            ;;
        --debug-redis)
            DEBUG_REDIS=ON
            shift
            ;;
        --debug-all)
            DEBUG_CONNECTION_POOL=ON
            DEBUG_TRANSACTION_MANAGER=ON
            DEBUG_SQLITE=ON
            DEBUG_FIREBIRD=ON
            DEBUG_MONGODB=ON
            DEBUG_REDIS=ON
            DEBUG_ALL=ON
            shift
            ;;
        --dw-off)
            DW_OFF=true
            shift
            ;;
        --db-driver-thread-safe-off)
            DB_DRIVER_THREAD_SAFE_OFF=true
            shift
            ;;
        --progress)
            SHOW_PROGRESS=true
            shift
            ;;
        --help)
            echo "Usage: $0 [options]"
            echo "Options:"
            echo "  --mysql, --mysql-on    Enable MySQL support (default)"
            echo "  --mysql-off            Disable MySQL support"
            echo "  --postgres, --postgres-on  Enable PostgreSQL support"
            echo "  --postgres-off         Disable PostgreSQL support"
            echo "  --sqlite, --sqlite-on  Enable SQLite support"
            echo "  --sqlite-off           Disable SQLite support"
            echo "  --firebird, --firebird-on  Enable Firebird support"
            echo "  --firebird-off         Disable Firebird support"
            echo "  --mongodb, --mongodb-on  Enable MongoDB support"
            echo "  --mongodb-off          Disable MongoDB support"
            echo "  --scylladb, --scylladb-on    Enable ScyllaDB support"
            echo "  --scylladb-off           Disable ScyllaDB support"
            echo "  --redis, --redis-on    Enable Redis support"
            echo "  --redis-off            Disable Redis support"
            echo "  --yaml, --yaml-on      Enable YAML configuration support"
            echo "  --yaml-off             Disable YAML configuration support"
            echo "  --auto                 Automatically run tests without user interaction"
            echo "  --release              Run in Release mode (default: Debug)"
            echo "  --gcc-analyzer         Enable GCC Static Analyzer (GCC 10+)"
            echo "  --asan                 Enable Address Sanitizer"
            echo "  --valgrind             Run tests with Valgrind"
            echo "  --ctest                Run tests using CTest"
            echo "  --check                Check shared library dependencies of test executable"
            echo "  --clean                Clean build directories before building (Always activate the --rebuild flag)"
            echo "  --rebuild              Rebuild the test targets before running"
            echo "  --skip-build           Skip the build step"
            echo "  --list                 List tests only (do not run)"
            echo "  --run=N                Run all test sets N times (default: 1)"
            echo "  --run-test=\"tag\"       Run only tests with the specified tag (use + to separate multiple tags, e.g. \"tag1+tag2+tag3\")"
            echo "  --debug-pool           Enable debug output for ConnectionPool"
            echo "  --debug-txmgr          Enable debug output for TransactionManager"
            echo "  --debug-sqlite         Enable debug output for SQLite driver"
            echo "  --debug-firebird       Enable debug output for Firebird driver"
            echo "  --debug-mongodb        Enable debug output for MongoDB driver"
            echo "  --debug-scylladb         Enable debug output for ScyllaDB driver"
            echo "  --debug-redis          Enable debug output for Redis driver"
            echo "  --debug-all            Enable all debug output"
            echo "  --dw-off               Disable libdw support for stack traces"
            echo "  --db-driver-thread-safe-off  Disable thread-safe database driver operations"
            echo "  --progress             Show visual progress bar during test execution"
            echo "  --help                 Show this help message"
            exit 0
            ;;
        *)
            # Unknown option
            echo "Unknown option: $1"
            echo "Usage: $0 [options]"
            exit 1
            ;;
    esac
done

# Note: We don't build the main project here anymore.
# run_test_cpp_dbc.sh will handle building both the library and tests in one place,
# avoiding double compilation of the cpp_dbc library.

# Check for incompatible options
if [ "$USE_VALGRIND" = true ] && [ "$ENABLE_ASAN" = true ]; then
    echo -e "\nWARNING: Valgrind and AddressSanitizer are not fully compatible."
    echo -e "You should use either Valgrind or AddressSanitizer, but not both."
    echo -e "Consider not using --asan with --valgrind.\n"
    
    # Ask for confirmation if not in auto mode
    if [ "$AUTO_MODE" = false ]; then
        read -p "Do you want to continue anyway? (y/n) " -n 1 -r
        echo
        if [[ ! $REPLY =~ ^[Yy]$ ]]; then
            echo "Exiting as requested."
            exit 1
        fi
        echo -e "Continuing as requested. Expect potential issues.\n"
    else
        echo -e "Auto mode enabled. Continuing with both options. Expect potential issues.\n"
    fi
fi

# Build the command to pass to run_test_cpp_dbc.sh
CMD="./libs/cpp_dbc/run_test_cpp_dbc.sh"

if [ "$ENABLE_GCC_ANALYZER" = "ON" ]; then
    CMD="$CMD --gcc-analyzer"
fi

# Add options based on user input
if [ "$USE_MYSQL" = "OFF" ]; then
    CMD="$CMD --mysql-off"
fi

if [ "$USE_POSTGRESQL" = "ON" ]; then
    CMD="$CMD --postgres-on"
fi

if [ "$USE_SQLITE" = "ON" ]; then
    CMD="$CMD --sqlite-on"
fi

if [ "$USE_FIREBIRD" = "ON" ]; then
    CMD="$CMD --firebird-on"
fi

if [ "$USE_MONGODB" = "ON" ]; then
    CMD="$CMD --mongodb-on"
fi

if [ "$USE_SCYLLADB" = "ON" ]; then
    CMD="$CMD --scylladb-on"
fi

if [ "$USE_REDIS" = "ON" ]; then
    CMD="$CMD --redis-on"
fi

if [ "$USE_YAML" = "ON" ]; then
    CMD="$CMD --yaml-on"
else
    CMD="$CMD --yaml-off"
fi

if [ "$AUTO_MODE" = true ]; then
    CMD="$CMD --auto"
fi

if [ "$BUILD_TYPE" = "Release" ]; then
    CMD="$CMD --release"
fi

if [ "$ENABLE_ASAN" = true ]; then
    CMD="$CMD --asan"
fi

if [ "$USE_VALGRIND" = true ]; then
    CMD="$CMD --valgrind"
fi

if [ "$RUN_CTEST" = true ]; then
    CMD="$CMD --ctest"
fi

if [ "$CHECK_DEPENDENCIES" = true ]; then
    CMD="$CMD --check"
fi

if [ "$REBUILD" = true ]; then
    CMD="$CMD --rebuild"
fi

if [ "$SKIP_BUILD" = true ]; then
    CMD="$CMD --skip-build"
fi

if [ "$LIST_ONLY" = true ]; then
    CMD="$CMD --list"
fi

# Add run count parameter
if [ "$RUN_COUNT" -gt 1 ]; then
    CMD="$CMD --run=$RUN_COUNT"
fi

# Add specific test parameter if provided
if [ -n "$RUN_SPECIFIC_TEST" ]; then
    echo "Debug: run_test.sh - RUN_SPECIFIC_TEST value: '$RUN_SPECIFIC_TEST'"
    CMD="$CMD --run-test=$RUN_SPECIFIC_TEST"
    echo "Debug: run_test.sh - Final command: $CMD"
fi

# Add debug options
if [ "$DEBUG_CONNECTION_POOL" = "ON" ]; then
    CMD="$CMD --debug-pool"
fi

if [ "$DEBUG_TRANSACTION_MANAGER" = "ON" ]; then
    CMD="$CMD --debug-txmgr"
fi

if [ "$DEBUG_SQLITE" = "ON" ]; then
    CMD="$CMD --debug-sqlite"
fi

if [ "$DEBUG_FIREBIRD" = "ON" ]; then
    CMD="$CMD --debug-firebird"
fi

if [ "$DEBUG_MONGODB" = "ON" ]; then
    CMD="$CMD --debug-mongodb"
fi

if [ "$DEBUG_SCYLLADB" = "ON" ]; then
    CMD="$CMD --debug-scylladb"
fi

if [ "$DEBUG_REDIS" = "ON" ]; then
    CMD="$CMD --debug-redis"
fi

if [ "$DEBUG_ALL" = "ON" ]; then
    CMD="$CMD --debug-all"
fi

# Add dw-off option if specified
if [ "$DW_OFF" = true ]; then
    CMD="$CMD --dw-off"
fi

# Add db-driver-thread-safe-off option if specified
if [ "$DB_DRIVER_THREAD_SAFE_OFF" = true ]; then
    CMD="$CMD --db-driver-thread-safe-off"
fi

# Add progress option if specified
if [ "$SHOW_PROGRESS" = true ]; then
    CMD="$CMD --progress"
fi

# Execute the command
echo "Running tests with command: $CMD"
$CMD

echo -e "\nAll tests completed!"