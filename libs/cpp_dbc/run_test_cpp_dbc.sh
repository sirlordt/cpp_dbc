#!/bin/bash
set -e  # Exit on error

# Script: run_test_cpp_dbc.sh
# Description: Runs the cpp_dbc library tests
# Usage: ./run_test_cpp_dbc.sh [options]
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

# Check if valgrind is installed if --valgrind is used
if [ "$USE_VALGRIND" = true ]; then
    if ! command -v valgrind &> /dev/null; then
        echo "Error: Valgrind is not installed but --valgrind option was specified."
        echo "Please install Valgrind or remove the --valgrind option."
        exit 1
    fi
fi

# Function to check if a command exists
command_exists() {
    command -v "$1" >/dev/null 2>&1
}

# Function to install packages if they don't exist
ensure_package_installed() {
    local package_name="$1"
    local command_name="${2:-$1}"  # Default to package name if command name not provided
    
    if ! command_exists "$command_name"; then
        echo "$command_name is not installed. Attempting to install $package_name..."
        
        # Check if we have sudo access
        if command_exists sudo; then
            sudo apt-get update
            sudo apt-get install -y "$package_name"
        else
            echo "Error: sudo is not available. Please install $package_name manually."
            exit 1
        fi
        
        # Verify installation
        if ! command_exists "$command_name"; then
            echo "Error: Failed to install $package_name. Please install it manually."
            exit 1
        fi
        
        echo "$package_name installed successfully."
    else
        echo "$command_name is already installed."
    fi
}

# Check and install required packages
ensure_package_installed "cmake"
ensure_package_installed "python3-pip" "pip3"

# Check for Conan
if ! command_exists conan; then
    echo "Conan is not installed. Attempting to install via pip..."
    pip3 install conan
    
    # Verify installation
    if ! command_exists conan; then
        echo "Error: Failed to install Conan. Please install it manually."
        exit 1
    fi
    
    echo "Conan installed successfully."
fi

# Check if Conan profile exists and create one if it doesn't
if ! conan profile list | grep -q default; then
    echo "No default Conan profile found. Creating one..."
    conan profile detect
fi

# Set paths
SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )"
CPP_DBC_DIR="${SCRIPT_DIR}"
TEST_BUILD_DIR="${CPP_DBC_DIR}/build/test_build"
TEST_EXECUTABLE="${TEST_BUILD_DIR}/test/cpp_dbc_tests"

# Check if the tests are built
if [ ! -f "$TEST_EXECUTABLE" ] || [ "$REBUILD" = true ]; then
    # Build with appropriate options
    BUILD_CMD="./build_test_cpp_dbc.sh"
    if [ "$ENABLE_ASAN" = true ]; then
        BUILD_CMD="$BUILD_CMD --asan"
    fi
    if [ "$USE_MYSQL" = "OFF" ]; then
        BUILD_CMD="$BUILD_CMD --mysql-off"
    fi
    if [ "$USE_POSTGRESQL" = "ON" ]; then
        BUILD_CMD="$BUILD_CMD --postgres-on"
    fi
    if [ "$BUILD_TYPE" = "Release" ]; then
        BUILD_CMD="$BUILD_CMD --release"
    fi

    # Execute the build command
    echo "Building tests with command: $BUILD_CMD"
    $BUILD_CMD
else
    echo "Using existing test executable. Use --rebuild to force a rebuild."
fi

# Turn off command echoing if it was enabled by build_test_cpp_dbc.sh
{ set +x; } 2>/dev/null

echo -e "\nRunning tests..."

# Check shared library dependencies if requested
if [ "$CHECK_DEPENDENCIES" = true ]; then
    echo -e "\n========== Checking Shared Library Dependencies ==========\n"
    echo -e "Running ldd on cpp_dbc_tests executable...\n"
    ldd "$TEST_EXECUTABLE"
    echo -e "\n========== End of Dependencies Check ==========\n"
    
    # If checking dependencies, exit here without running tests
    exit 0
fi

echo -e "\n========== Running Tests ==========\n"

# Function to run the test executable with appropriate options
run_test() {
    cd "$TEST_BUILD_DIR"
    
    if [ "$USE_VALGRIND" = true ]; then
        # Using Valgrind
        if [ -n "$ASAN_OPTIONS" ]; then
            env ASAN_OPTIONS="$ASAN_OPTIONS" valgrind --leak-check=full --show-leak-kinds=all --track-origins=yes --verbose "$TEST_EXECUTABLE" $@
        else
            valgrind --leak-check=full --show-leak-kinds=all --track-origins=yes --verbose "$TEST_EXECUTABLE" $@
        fi
    else
        # Not using Valgrind
        if [ -n "$ASAN_OPTIONS" ]; then
            env ASAN_OPTIONS="$ASAN_OPTIONS" "$TEST_EXECUTABLE" $@
        else
            "$TEST_EXECUTABLE" $@
        fi
    fi
}

# Run tests with CTest if requested
if [ "$RUN_CTEST" = true ]; then
    echo -e "Running tests with CTest...\n"
    cd "$TEST_BUILD_DIR"
    
    if [ -n "$ASAN_OPTIONS" ]; then
        env ASAN_OPTIONS="$ASAN_OPTIONS" ctest -C "$BUILD_TYPE" --output-on-failure
    else
        ctest -C "$BUILD_TYPE" --output-on-failure
    fi
else
    # Run tests directly
    echo -e "Running tests directly...\n"
    run_test
fi

echo -e "\n========== All Tests Completed ==========\n"
echo -e "Test execution completed. Check the output above for any errors."