#!/bin/bash

# build_cpp_dbc.sh
# Script to install dependencies and build the cpp_dbc library

set -e

# Default values
USE_MYSQL=ON
USE_POSTGRESQL=OFF
USE_CPP_YAML=OFF
BUILD_TYPE=Debug
BUILD_TESTS=OFF
BUILD_EXAMPLES=OFF

# Parse command line arguments
for arg in "$@"
do
    case $arg in
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
        --yaml|--yaml-on)
        USE_CPP_YAML=ON
        shift
        ;;
        --debug)
        BUILD_TYPE=Debug
        shift
        ;;
        --release)
        BUILD_TYPE=Release
        shift
        ;;
        --test)
        BUILD_TESTS=ON
        shift
        ;;
        --examples)
        BUILD_EXAMPLES=ON
        shift
        ;;
        --examples)
        BUILD_EXAMPLES=ON
        shift
        ;;
        --help)
        echo "Usage: $0 [options]"
        echo "Options:"
        echo "  --mysql, --mysql-on    Enable MySQL support (default)"
        echo "  --mysql-off            Disable MySQL support"
        echo "  --postgres, --postgres-on  Enable PostgreSQL support"
        echo "  --postgres-off         Disable PostgreSQL support"
        echo "  --yaml, --yaml-on      Enable YAML configuration support"
        echo "  --debug                Build in Debug mode (default)"
        echo "  --release              Build in Release mode"
        echo "  --test                 Build cpp_dbc tests"
        echo "  --examples             Build cpp_dbc examples"
        echo "  --help                 Show this help message"
        exit 1
        ;;
    esac
done

echo "Building cpp_dbc library..."
echo "Database driver configuration:"
echo "  MySQL support: $USE_MYSQL"
echo "  PostgreSQL support: $USE_POSTGRESQL"
echo "  YAML support: $USE_CPP_YAML"
echo "  Build type: $BUILD_TYPE"
echo "  Build tests: $BUILD_TESTS"
echo "  Build examples: $BUILD_EXAMPLES"

# Check for MySQL dependencies
if [ "$USE_MYSQL" = "ON" ]; then
    echo "Checking for MySQL development libraries..."
    
    # Detect package manager
    if command -v apt-get &> /dev/null; then
        # Debian/Ubuntu
        if ! dpkg -l | grep -q libmysqlclient-dev; then
            echo "MySQL development libraries not found. Installing..."
            sudo apt-get update
            sudo apt-get install -y libmysqlclient-dev
        else
            echo "MySQL development libraries already installed."
        fi
    elif command -v dnf &> /dev/null; then
        # Fedora/RHEL/CentOS
        if ! rpm -q mysql-devel &> /dev/null; then
            echo "MySQL development libraries not found. Installing..."
            sudo dnf install -y mysql-devel
        else
            echo "MySQL development libraries already installed."
        fi
    elif command -v yum &> /dev/null; then
        # Older RHEL/CentOS
        if ! rpm -q mysql-devel &> /dev/null; then
            echo "MySQL development libraries not found. Installing..."
            sudo yum install -y mysql-devel
        else
            echo "MySQL development libraries already installed."
        fi
    else
        echo "Warning: Could not detect package manager. Please install MySQL development libraries manually."
    fi
fi

# Check for PostgreSQL dependencies
if [ "$USE_POSTGRESQL" = "ON" ]; then
    echo "Checking for PostgreSQL development libraries..."
    
    # Detect package manager
    if command -v apt-get &> /dev/null; then
        # Debian/Ubuntu
        if ! dpkg -l | grep -q libpq-dev; then
            echo "PostgreSQL development libraries not found. Installing..."
            sudo apt-get update
            sudo apt-get install -y libpq-dev
        else
            echo "PostgreSQL development libraries already installed."
        fi
    elif command -v dnf &> /dev/null; then
        # Fedora/RHEL/CentOS
        if ! rpm -q postgresql-devel &> /dev/null; then
            echo "PostgreSQL development libraries not found. Installing..."
            sudo dnf install -y postgresql-devel
        else
            echo "PostgreSQL development libraries already installed."
        fi
    elif command -v yum &> /dev/null; then
        # Older RHEL/CentOS
        if ! rpm -q postgresql-devel &> /dev/null; then
            echo "PostgreSQL development libraries not found. Installing..."
            sudo yum install -y postgresql-devel
        else
            echo "PostgreSQL development libraries already installed."
        fi
    else
        echo "Warning: Could not detect package manager. Please install PostgreSQL development libraries manually."
    fi
fi

# Create build directory
SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )"
PROJECT_ROOT="$( cd "${SCRIPT_DIR}/../.." &> /dev/null && pwd )"
BUILD_DIR="${PROJECT_ROOT}/build/libs/cpp_dbc/build"
INSTALL_DIR="${PROJECT_ROOT}/build/libs/cpp_dbc"

mkdir -p "${BUILD_DIR}"
cd "${BUILD_DIR}"

# Define Conan directory - use the build directory in the project root
CONAN_DIR="${PROJECT_ROOT}/build/libs/cpp_dbc/conan"
mkdir -p "${CONAN_DIR}"

# Run Conan in the project root's build directory, not in libs/cpp_dbc/build
echo "Installing dependencies with Conan..."
cd "${SCRIPT_DIR}"
conan install . --output-folder="${CONAN_DIR}" --build=missing -s build_type=$BUILD_TYPE
cd "${BUILD_DIR}"

# Configure with CMake
echo "Configuring with CMake..."

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

cmake "${SCRIPT_DIR}" \
      -DCMAKE_BUILD_TYPE=$BUILD_TYPE \
      -DUSE_MYSQL=$USE_MYSQL \
      -DUSE_POSTGRESQL=$USE_POSTGRESQL \
      -DUSE_CPP_YAML=$USE_CPP_YAML \
      -DCMAKE_INSTALL_PREFIX="${INSTALL_DIR}" \
      -DCPP_DBC_BUILD_TESTS=$BUILD_TESTS \
      -DCPP_DBC_BUILD_EXAMPLES=$BUILD_EXAMPLES \
      -DCMAKE_TOOLCHAIN_FILE=$TOOLCHAIN_FILE \
      -Wno-dev

# Build and install the library
echo "Building and installing the library..."
cmake --build . --target install

echo "cpp_dbc library build completed successfully."
echo "The library has been installed to: ${INSTALL_DIR}"
echo ""

# If tests are enabled, call build_test_cpp_dbc.sh with the appropriate parameters
if [ "$BUILD_TESTS" = "ON" ]; then
    echo "Building tests using build_test_cpp_dbc.sh..."
    
    # Prepare parameters to pass to build_test_cpp_dbc.sh
    TEST_PARAMS=""

    # Pass Yaml configuration
    if [ "$USE_CPP_YAML" = "ON" ]; then
        TEST_PARAMS="$TEST_PARAMS --yaml"
    else
        TEST_PARAMS="$TEST_PARAMS --yaml-off"
    fi
    
    # Pass MySQL configuration
    if [ "$USE_MYSQL" = "ON" ]; then
        TEST_PARAMS="$TEST_PARAMS --mysql"
    else
        TEST_PARAMS="$TEST_PARAMS --mysql-off"
    fi
    
    # Pass PostgreSQL configuration
    if [ "$USE_POSTGRESQL" = "ON" ]; then
        TEST_PARAMS="$TEST_PARAMS --postgres"
    fi
    
    # Pass build type
    if [ "$BUILD_TYPE" = "Release" ]; then
        TEST_PARAMS="$TEST_PARAMS --release"
    fi
    
    # Call build_test_cpp_dbc.sh with the parameters
    echo "Running: ${SCRIPT_DIR}/build_test_cpp_dbc.sh $TEST_PARAMS"
    "${SCRIPT_DIR}/build_test_cpp_dbc.sh" $TEST_PARAMS
fi

echo "Database driver status:"
echo "  MySQL: $USE_MYSQL"
echo "  PostgreSQL: $USE_POSTGRESQL"
echo "  YAML support: $USE_CPP_YAML"
echo "  Build type: $BUILD_TYPE"
echo "  Build tests: $BUILD_TESTS"
echo "  Build examples: $BUILD_EXAMPLES"