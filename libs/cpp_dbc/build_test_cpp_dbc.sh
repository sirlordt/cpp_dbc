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
#   --release              Build in Release mode (default: Debug)
#   --asan                 Enable Address Sanitizer
#   --help                 Show this help message

# Default values
USE_MYSQL=ON
USE_POSTGRESQL=OFF
BUILD_TYPE=Debug
ENABLE_ASAN=OFF
NO_REBUILD_DEPS=false

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
            ENABLE_ASAN=ON
            shift
            ;;
        --test)
            # Option accepted for compatibility, but not needed since this script always builds tests
            shift
            ;;
        --help)
            echo "Usage: $0 [options]"
            echo "Options:"
            echo "  --mysql, --mysql-on    Enable MySQL support (default)"
            echo "  --mysql-off            Disable MySQL support"
            echo "  --postgres, --postgres-on  Enable PostgreSQL support"
            echo "  --postgres-off         Disable PostgreSQL support"
            echo "  --release              Build in Release mode (default: Debug)"
            echo "  --asan                 Enable Address Sanitizer"
            echo "  --help                 Show this help message"
            exit 0
            ;;
        *)
            # Unknown option
            echo "Unknown option: $1"
            echo "Usage: $0 [--mysql-on|--mysql-off] [--postgres-on|--postgres-off] [--release] [--asan] [--help]"
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
echo "  MySQL support: $USE_MYSQL"
echo "  PostgreSQL support: $USE_POSTGRESQL"
echo "  Build type: $BUILD_TYPE"
echo "  Address Sanitizer: $ENABLE_ASAN"

# Create build directory for tests
SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )"
CPP_DBC_DIR="${SCRIPT_DIR}"
TEST_BUILD_DIR="${CPP_DBC_DIR}/build/test_build"

mkdir -p "${TEST_BUILD_DIR}"
cd "${TEST_BUILD_DIR}"

# Print commands before executing them
set -x

# Install dependencies with Conan
echo "Installing dependencies with Conan..."
cd "${CPP_DBC_DIR}"
conan install . --build=missing -s build_type=$BUILD_TYPE
cd "${TEST_BUILD_DIR}"

# Find the conan_toolchain.cmake file
CONAN_DIR="${CPP_DBC_DIR}/build"
if [ -d "${CONAN_DIR}/${BUILD_TYPE}" ]; then
    TOOLCHAIN_FILE="${CONAN_DIR}/${BUILD_TYPE}/generators/conan_toolchain.cmake"
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
      -DUSE_MYSQL=$USE_MYSQL \
      -DUSE_POSTGRESQL=$USE_POSTGRESQL \
      -DCPP_DBC_BUILD_TESTS=ON \
      -DENABLE_ASAN=$ENABLE_ASAN \
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