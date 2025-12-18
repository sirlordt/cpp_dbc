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
#   --run-test="tag"       Run only tests with the specified tag (use + to separate multiple tags, e.g. "tag1+tag2+tag3")
#   --debug-pool           Enable debug output for ConnectionPool
#   --debug-txmgr          Enable debug output for TransactionManager
#   --debug-sqlite         Enable debug output for SQLite driver
#   --debug-all            Enable all debug output
#   --dw-off               Disable libdw support for stack traces
#   --db-driver-thread-safe-off  Disable thread-safe database driver operations
#   --help                 Show this help message

# Default values for options
USE_MYSQL=ON
USE_POSTGRESQL=OFF
USE_SQLITE=OFF
USE_YAML=ON
BUILD_TYPE=Debug
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
DW_OFF=false
DB_DRIVER_THREAD_SAFE_OFF=false

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
        --debug-all)
            DEBUG_CONNECTION_POOL=ON
            DEBUG_TRANSACTION_MANAGER=ON
            DEBUG_SQLITE=ON
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
        --help)
            echo "Usage: $0 [options]"
            echo "Options:"
            echo "  --mysql, --mysql-on    Enable MySQL support (default)"
            echo "  --mysql-off            Disable MySQL support"
            echo "  --postgres, --postgres-on  Enable PostgreSQL support"
            echo "  --postgres-off         Disable PostgreSQL support"
            echo "  --sqlite, --sqlite-on  Enable SQLite support"
            echo "  --sqlite-off           Disable SQLite support"
            echo "  --yaml, --yaml-on      Enable YAML configuration support"
            echo "  --yaml-off             Disable YAML configuration support"
            echo "  --auto                 Automatically run tests without user interaction"
            echo "  --release              Run in Release mode (default: Debug)"
            echo "  --asan                 Enable Address Sanitizer"
            echo "  --valgrind             Run tests with Valgrind"
            echo "  --ctest                Run tests using CTest"
            echo "  --check                Check shared library dependencies of test executable"
            echo "  --clean                Clean build directories before building (Always activate the --rebuild flag)"
            echo "  --rebuild              Rebuild the test targets before running"
            echo "  --run=N                Run all test sets N times (default: 1)"
            echo "  --run-test=\"tag\"       Run only tests with the specified tag (use + to separate multiple tags, e.g. \"tag1+tag2+tag3\")"
            echo "  --debug-pool           Enable debug output for ConnectionPool"
            echo "  --debug-txmgr          Enable debug output for TransactionManager"
            echo "  --debug-sqlite         Enable debug output for SQLite driver"
            echo "  --debug-all            Enable all debug output"
            echo "  --dw-off               Disable libdw support for stack traces"
            echo "  --db-driver-thread-safe-off  Disable thread-safe database driver operations"
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

# Check if the project has been built
# Get the binary name from .dist_build
if [ -f ".dist_build" ]; then
    BIN_NAME=$(awk -F'"' '/^Container_Bin_Name=/{print $2}' .dist_build)
else
    BIN_NAME="cpp_dbc_demo"  # Default name if .dist_build is not found
fi

MAIN_EXECUTABLE="build/${BIN_NAME}"
if [ ! -f "$MAIN_EXECUTABLE" ]; then
    echo "Project has not been built yet. Building the project first..."
    
    # Build the project with appropriate options
    BUILD_CMD="./build.sh"
    
    # Pass the same database options to build.sh
    if [ "$USE_MYSQL" = "OFF" ]; then
        BUILD_CMD="$BUILD_CMD --mysql-off"
    else
        BUILD_CMD="$BUILD_CMD --mysql"
    fi
    
    if [ "$USE_POSTGRESQL" = "ON" ]; then
        BUILD_CMD="$BUILD_CMD --postgres-on"
    fi
    
    if [ "$USE_SQLITE" = "ON" ]; then
        BUILD_CMD="$BUILD_CMD --sqlite"
    fi
    
    if [ "$USE_YAML" = "ON" ]; then
        BUILD_CMD="$BUILD_CMD --yaml"
    else 
        BUILD_CMD="$BUILD_CMD --yaml-off"
    fi
    
    if [ "$BUILD_TYPE" = "Release" ]; then
        BUILD_CMD="$BUILD_CMD --release"
    fi
    
    # Always include --test to ensure tests are built
    BUILD_CMD="$BUILD_CMD --test"
    
    echo "$0 => Running build command: $BUILD_CMD"
    $BUILD_CMD
    
    # Check if build was successful
    if [ $? -ne 0 ]; then
        echo "Error: Project build failed. Cannot run tests."
        exit 1
    fi
    
    echo "Project built successfully. Continuing with tests..."
    echo ""
fi

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

# Add dw-off option if specified
if [ "$DW_OFF" = true ]; then
    CMD="$CMD --dw-off"
fi

# Add db-driver-thread-safe-off option if specified
if [ "$DB_DRIVER_THREAD_SAFE_OFF" = true ]; then
    CMD="$CMD --db-driver-thread-safe-off"
fi

# Execute the command
echo "Running tests with command: $CMD"
$CMD

echo -e "\nAll tests completed!"