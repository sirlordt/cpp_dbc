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
#   --release              Run in Release mode (default: Debug)
#   --asan                 Enable Address Sanitizer
#   --valgrind             Run tests with Valgrind
#   --ctest                Run tests using CTest
#   --check                Check shared library dependencies of test executable
#   --rebuild              Rebuild the test targets before running
#   --help                 Show this help message

# Default values for options
USE_MYSQL=ON
USE_POSTGRESQL=OFF
BUILD_TYPE=Debug
ASAN_OPTIONS=""
ENABLE_ASAN=false
RUN_CTEST=false
USE_VALGRIND=false
CHECK_DEPENDENCIES=false
REBUILD=false

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
        --rebuild)
            REBUILD=true
            shift
            ;;
        --help)
            echo "Usage: $0 [options]"
            echo "Options:"
            echo "  --mysql, --mysql-on    Enable MySQL support (default)"
            echo "  --mysql-off            Disable MySQL support"
            echo "  --postgres, --postgres-on  Enable PostgreSQL support"
            echo "  --postgres-off         Disable PostgreSQL support"
            echo "  --release              Run in Release mode (default: Debug)"
            echo "  --asan                 Enable Address Sanitizer"
            echo "  --valgrind             Run tests with Valgrind"
            echo "  --ctest                Run tests using CTest"
            echo "  --check                Check shared library dependencies of test executable"
            echo "  --rebuild              Rebuild the test targets before running"
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
    fi
    
    if [ "$USE_POSTGRESQL" = "ON" ]; then
        BUILD_CMD="$BUILD_CMD --postgres-on"
    fi
    
    if [ "$BUILD_TYPE" = "Release" ]; then
        BUILD_CMD="$BUILD_CMD --release"
    fi
    
    # Always include --test to ensure tests are built
    BUILD_CMD="$BUILD_CMD --test"
    
    echo "Running build command: $BUILD_CMD"
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
    
    # Ask for confirmation
    read -p "Do you want to continue anyway? (y/n) " -n 1 -r
    echo
    if [[ ! $REPLY =~ ^[Yy]$ ]]; then
        echo "Exiting as requested."
        exit 1
    fi
    echo -e "Continuing as requested. Expect potential issues.\n"
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

# Execute the command
echo "Running tests with command: $CMD"
$CMD

echo -e "\nAll tests completed!"