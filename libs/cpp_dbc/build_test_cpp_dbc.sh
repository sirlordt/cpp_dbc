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
#   --firebird, --firebird-on  Enable Firebird support
#   --firebird-off         Disable Firebird support
#   --release              Build in Release mode (default: Debug)
#   --asan                 Enable Address Sanitizer
#   --debug-pool           Enable debug output for ConnectionPool
#   --debug-txmgr          Enable debug output for TransactionManager
#   --debug-sqlite         Enable debug output for SQLite driver
#   --debug-firebird       Enable debug output for Firebird driver
#   --debug-all            Enable all debug output
#   --help                 Show this help message

# Default values
USE_MYSQL=ON
USE_POSTGRESQL=OFF
USE_SQLITE=OFF
USE_FIREBIRD=OFF
USE_MONGODB=OFF
USE_SCYLLA=OFF
USE_REDIS=OFF
USE_CPP_YAML=OFF
BUILD_TYPE=Debug
ENABLE_ASAN=OFF
ENABLE_GCC_ANALYZER=OFF
NO_REBUILD_DEPS=false
DEBUG_CONNECTION_POOL=OFF
DEBUG_TRANSACTION_MANAGER=OFF
DEBUG_SQLITE=OFF
DEBUG_FIREBIRD=OFF
DEBUG_MONGODB=OFF
DEBUG_SCYLLA=OFF
DEBUG_REDIS=OFF
BACKWARD_HAS_DW=ON
DB_DRIVER_THREAD_SAFE=ON

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
        --scylla|--scylla-on)
            USE_SCYLLA=ON
            shift
            ;;
        --scylla-off)
            USE_SCYLLA=OFF
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
        --gcc-analyzer)
            ENABLE_GCC_ANALYZER=ON
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
        --debug-scylla)
            DEBUG_SCYLLA=ON
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
            DEBUG_SCYLLA=ON
            DEBUG_REDIS=ON
            shift
            ;;
        --dw-off)
            BACKWARD_HAS_DW=OFF
            shift
            ;;
        --db-driver-thread-safe-off)
            DB_DRIVER_THREAD_SAFE=OFF
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
            echo "  --firebird, --firebird-on  Enable Firebird support"
            echo "  --firebird-off         Disable Firebird support"
            echo "  --mongodb, --mongodb-on  Enable MongoDB support"
            echo "  --mongodb-off          Disable MongoDB support"
            echo "  --scylla, --scylla-on    Enable ScyllaDB support"
            echo "  --scylla-off           Disable ScyllaDB support"
            echo "  --redis, --redis-on    Enable Redis support"
            echo "  --redis-off            Disable Redis support"
            echo "  --release              Build in Release mode (default: Debug)"
            echo "  --debug                Build in Debug mode (default)"
            echo "  --asan                 Enable Address Sanitizer"
            echo "  --gcc-analyzer         Enable GCC Static Analyzer (GCC 10+)"
            echo "  --debug-pool           Enable debug output for ConnectionPool"
            echo "  --debug-txmgr          Enable debug output for TransactionManager"
            echo "  --debug-sqlite         Enable debug output for SQLite driver"
            echo "  --debug-firebird       Enable debug output for Firebird driver"
            echo "  --debug-mongodb        Enable debug output for MongoDB driver"
            echo "  --debug-scylla         Enable debug output for ScyllaDB driver"
            echo "  --debug-redis          Enable debug output for Redis driver"
            echo "  --debug-all            Enable all debug output"
            echo "  --dw-off               Disable libdw support for stack traces"
            echo "  --db-driver-thread-safe-off  Disable thread-safe database driver operations"
            echo "  --help                 Show this help message"
            exit 0
            ;;
        *)
            # Unknown option
            echo "Unknown option: $1"
            echo "Usage: $0 [--mysql|--mysql-on|--mysql-off] [--yaml|--yaml-on|--yaml-off] [--postgres|--postgres-on|--postgres-off] [--sqlite|--sqlite-on|--sqlite-off] [--firebird|--firebird-on|--firebird-off] [--mongodb|--mongodb-on|--mongodb-off] [--scylla|--scylla-on|--scylla-off] [--release] [--asan] [--dw-off] [--help]"
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
echo "  Firebird support: $USE_FIREBIRD"
echo "  MongoDB support: $USE_MONGODB"
echo "  ScyllaDB support: $USE_SCYLLA"
echo "  Redis support: $USE_REDIS"
echo "  Build type: $BUILD_TYPE"
echo "  Address Sanitizer: $ENABLE_ASAN"
echo "  Debug ConnectionPool: $DEBUG_CONNECTION_POOL"
echo "  Debug TransactionManager: $DEBUG_TRANSACTION_MANAGER"
echo "  Debug SQLite: $DEBUG_SQLITE"
echo "  Debug Firebird: $DEBUG_FIREBIRD"
echo "  Debug MongoDB: $DEBUG_MONGODB"
echo "  Debug ScyllaDB: $DEBUG_SCYLLA"
echo "  Debug Redis: $DEBUG_REDIS"
echo "  libdw support: $BACKWARD_HAS_DW"
echo "  DB driver thread-safe: $DB_DRIVER_THREAD_SAFE"

# Create build directory for tests
SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )"
CPP_DBC_DIR="${SCRIPT_DIR}"

# Check for ScyllaDB dependencies
if [ "$USE_SCYLLA" = "ON" ]; then
    echo "Checking for ScyllaDB (Cassandra) driver..."
    # Call the setup script
    if [ -f "${SCRIPT_DIR}/download_and_setup_cassandra_driver.sh" ]; then
        bash "${SCRIPT_DIR}/download_and_setup_cassandra_driver.sh"
        if [ $? -ne 0 ]; then
            echo "Error: Failed to setup Cassandra driver."
            exit 1
        fi
    else
        echo "Error: download_and_setup_cassandra_driver.sh not found in ${SCRIPT_DIR}"
        exit 1
    fi
fi
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

# Check if GCC supports -fanalyzer (GCC 10+)
GCC_VERSION=$(gcc -dumpversion | cut -d. -f1)
ANALYZER_FLAG=""

if [ "$ENABLE_GCC_ANALYZER" = "ON" ]; then
    if [ "$GCC_VERSION" -ge 10 ]; then
        echo "GCC version $GCC_VERSION supports -fanalyzer. Enabling it."
        ANALYZER_FLAG="-fanalyzer"
    else
        echo "GCC version $GCC_VERSION does not support -fanalyzer. Ignoring --gcc-analyzer."
    fi
else
    echo "GCC Static Analyzer is disabled (use --gcc-analyzer to enable)"
fi

# Configure with CMake
echo "Configuring with CMake..."
cmake "${CPP_DBC_DIR}" \
      -DCMAKE_TOOLCHAIN_FILE="$TOOLCHAIN_FILE" \
      -DCMAKE_BUILD_TYPE=$BUILD_TYPE \
      -DUSE_CPP_YAML=$USE_CPP_YAML \
      -DUSE_MYSQL=$USE_MYSQL \
      -DUSE_POSTGRESQL=$USE_POSTGRESQL \
      -DUSE_SQLITE=$USE_SQLITE \
      -DUSE_FIREBIRD=$USE_FIREBIRD \
      -DUSE_MONGODB=$USE_MONGODB \
      -DUSE_SCYLLA=$USE_SCYLLA \
      -DUSE_REDIS=$USE_REDIS \
      -DCPP_DBC_BUILD_TESTS=ON \
      -DENABLE_ASAN=$ENABLE_ASAN \
      -DDEBUG_CONNECTION_POOL=$DEBUG_CONNECTION_POOL \
      -DDEBUG_TRANSACTION_MANAGER=$DEBUG_TRANSACTION_MANAGER \
      -DDEBUG_SQLITE=$DEBUG_SQLITE \
      -DDEBUG_FIREBIRD=$DEBUG_FIREBIRD \
      -DDEBUG_MONGODB=$DEBUG_MONGODB \
      -DDEBUG_SCYLLA=$DEBUG_SCYLLA \
      -DDEBUG_REDIS=$DEBUG_REDIS \
      -DBACKWARD_HAS_DW=$BACKWARD_HAS_DW \
      -DDB_DRIVER_THREAD_SAFE=$DB_DRIVER_THREAD_SAFE \
      -DCMAKE_CXX_FLAGS="-Wall -Wextra -Wpedantic -Wconversion -Wshadow -Wcast-qual -Wformat=2 -Wunused -Werror=return-type -Werror=switch -Wdouble-promotion -Wfloat-equal -Wundef -Wpointer-arith -Wcast-align ${ANALYZER_FLAG}" \
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