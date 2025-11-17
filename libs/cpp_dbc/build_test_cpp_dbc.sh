#!/bin/bash
set -e  # Exit on error

# Script: build_test_cpp_dbc.sh
# Description: Builds the cpp_dbc library and its tests
# Usage: ./build_test_cpp_dbc.sh [options]
# Options:
#   --mysql, --mysql-on    Enable MySQL support (default)
#   --mysql-off            Disable MySQL support
#   --postgres, --postgres-on  Enable PostgreSQL support
#   --postgres-off         Disable PostgreSQL support
#   --sqlite, --sqlite-on  Enable SQLite support
#   --sqlite-off           Disable SQLite support
#   --release              Build in Release mode (default: Debug)
#   --asan                 Enable Address Sanitizer
#   --debug-pool           Enable debug output for ConnectionPool
#   --debug-txmgr          Enable debug output for TransactionManager
#   --debug-sqlite         Enable debug output for SQLite driver
#   --debug-all            Enable all debug output
#   --help                 Show this help message

# Default values
USE_MYSQL=ON
USE_POSTGRESQL=OFF
USE_SQLITE=OFF
USE_CPP_YAML=OFF
BUILD_TYPE=Debug
ENABLE_ASAN=OFF
NO_REBUILD_DEPS=false
DEBUG_CONNECTION_POOL=OFF
DEBUG_TRANSACTION_MANAGER=OFF
DEBUG_SQLITE=OFF
BACKWARD_HAS_DW=ON

# Parse command line arguments
while [[ $# -gt 0 ]]; do
    case $1 in
        --yaml|--yaml-on)
            USE_CPP_YAML=ON
            shift
            ;;
        --yaml-off)
            USE_CPP_YAML=OFF
            shift
            ;;
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
        --release)
            BUILD_TYPE=Release
            shift
            ;;
        --debug)
            BUILD_TYPE=Debug
            shift
            ;;
        --asan)
            ENABLE_ASAN=ON
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
            BACKWARD_HAS_DW=OFF
            shift
            ;;
        --test)
            # Option accepted for compatibility, but not needed since this script always builds tests
            shift
            ;;
        --help)
            echo "Usage: $0 [options]"
            echo "Options:"
            echo "  --yaml, --yaml-on      Enable yaml support"
            echo "  --yaml-off             Disable yaml support"
            echo "  --mysql, --mysql-on    Enable MySQL support (default)"
            echo "  --mysql-off            Disable MySQL support"
            echo "  --postgres, --postgres-on  Enable PostgreSQL support"
            echo "  --postgres-off         Disable PostgreSQL support"
            echo "  --sqlite, --sqlite-on  Enable SQLite support"
            echo "  --sqlite-off           Disable SQLite support"
            echo "  --release              Build in Release mode (default: Debug)"
            echo "  --debug                Build in Debug mode (default)"
            echo "  --asan                 Enable Address Sanitizer"
            echo "  --debug-pool           Enable debug output for ConnectionPool"
            echo "  --debug-txmgr          Enable debug output for TransactionManager"
            echo "  --debug-sqlite         Enable debug output for SQLite driver"
            echo "  --debug-all            Enable all debug output"
            echo "  --dw-off               Disable libdw support for stack traces"
            echo "  --help                 Show this help message"
            exit 0
            ;;
        *)
            # Unknown option
            echo "Unknown option: $1"
            echo "Usage: $0 [--mysql|--mysql-on|--mysql-off] [--yaml|--yaml-on|--yaml-off] [--postgres|--postgres-on|--postgres-off] [--sqlite|--sqlite-on|--sqlite-off] [--release] [--asan] [--dw-off] [--help]"
            exit 1
            ;;
    esac
done

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
ensure_package_installed "git"

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

echo "Building cpp_dbc tests..."
echo "Configuration:"
echo "  CPP-YAML support: $USE_CPP_YAML"
echo "  MySQL support: $USE_MYSQL"
echo "  PostgreSQL support: $USE_POSTGRESQL"
echo "  SQLite support: $USE_SQLITE"
echo "  Build type: $BUILD_TYPE"
echo "  Address Sanitizer: $ENABLE_ASAN"
echo "  Debug ConnectionPool: $DEBUG_CONNECTION_POOL"
echo "  Debug TransactionManager: $DEBUG_TRANSACTION_MANAGER"
echo "  Debug SQLite: $DEBUG_SQLITE"
echo "  libdw support: $BACKWARD_HAS_DW"

# Create build directory for tests
SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )"
CPP_DBC_DIR="${SCRIPT_DIR}"
#TEST_BUILD_DIR="${CPP_DBC_DIR}/build/test_build"
TEST_BUILD_DIR="${CPP_DBC_DIR}/../../build/libs/cpp_dbc/test"

mkdir -p "${TEST_BUILD_DIR}"
cd "${TEST_BUILD_DIR}"

# Print commands before executing them
set -x

# Define Conan directory in the project root's build directory
CONAN_DIR="${CPP_DBC_DIR}/../../build/libs/cpp_dbc/conan"
mkdir -p "${CONAN_DIR}"

# Install dependencies with Conan
echo "Installing dependencies with Conan..."
cd "${CPP_DBC_DIR}"
conan install . --output-folder="${CONAN_DIR}" --build=missing -s build_type=$BUILD_TYPE
cd "${TEST_BUILD_DIR}"

# Find the conan_toolchain.cmake file
if [ -d "${CONAN_DIR}/build/${BUILD_TYPE}" ]; then
    TOOLCHAIN_FILE="${CONAN_DIR}/build/${BUILD_TYPE}/generators/conan_toolchain.cmake"
else
    TOOLCHAIN_FILE="${CONAN_DIR}/generators/conan_toolchain.cmake"
fi

if [ ! -f "$TOOLCHAIN_FILE" ]; then
    echo "Error: Could not find conan_toolchain.cmake file. Conan installation may have failed."
    exit 1
fi

echo "Using toolchain file: $TOOLCHAIN_FILE"

# Configure with CMake
echo "Configuring with CMake..."
cmake "${CPP_DBC_DIR}" \
      -DCMAKE_TOOLCHAIN_FILE="$TOOLCHAIN_FILE" \
      -DCMAKE_BUILD_TYPE=$BUILD_TYPE \
      -DUSE_CPP_YAML=$USE_CPP_YAML \
      -DUSE_MYSQL=$USE_MYSQL \
      -DUSE_POSTGRESQL=$USE_POSTGRESQL \
      -DUSE_SQLITE=$USE_SQLITE \
      -DCPP_DBC_BUILD_TESTS=ON \
      -DENABLE_ASAN=$ENABLE_ASAN \
      -DDEBUG_CONNECTION_POOL=$DEBUG_CONNECTION_POOL \
      -DDEBUG_TRANSACTION_MANAGER=$DEBUG_TRANSACTION_MANAGER \
      -DDEBUG_SQLITE=$DEBUG_SQLITE \
      -DBACKWARD_HAS_DW=$BACKWARD_HAS_DW \
      -DCMAKE_CXX_FLAGS="-Wall -Wextra -Wpedantic -Wconversion -Wshadow -Wcast-qual -Wformat=2 -Wunused -Werror=return-type -Werror=switch -Wdouble-promotion -Wfloat-equal -Wundef -Wpointer-arith -Wcast-align" \
      -Wno-dev

# Print status message about ASAN
if [ "${ENABLE_ASAN}" = "OFF" ]; then
    echo "Address Sanitizer is disabled"
else
    echo "Address Sanitizer is enabled"
fi

# Build the tests
echo "Building the tests..."
cmake --build . --target cpp_dbc_tests

# Turn off command echoing
{ set +x; } 2>/dev/null

echo -e "\nTests built successfully!"