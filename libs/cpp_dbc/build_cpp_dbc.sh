#!/bin/bash

# build_cpp_dbc.sh
# Script to install dependencies and build the cpp_dbc library

set -e

# Default values
USE_MYSQL=ON
USE_POSTGRESQL=OFF
BUILD_TYPE=Debug
BUILD_TESTS=OFF

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
        --help)
        echo "Usage: $0 [options]"
        echo "Options:"
        echo "  --mysql, --mysql-on    Enable MySQL support (default)"
        echo "  --mysql-off            Disable MySQL support"
        echo "  --postgres, --postgres-on  Enable PostgreSQL support"
        echo "  --postgres-off         Disable PostgreSQL support"
        echo "  --debug                Build in Debug mode (default)"
        echo "  --release              Build in Release mode"
        echo "  --test                 Build cpp_dbc tests"
        echo "  --help                 Show this help message"
        exit 1
        ;;
    esac
done

echo "Building cpp_dbc library..."
echo "Database driver configuration:"
echo "  MySQL support: $USE_MYSQL"
echo "  PostgreSQL support: $USE_POSTGRESQL"
echo "  Build type: $BUILD_TYPE"
echo "  Build tests: $BUILD_TESTS"

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

# Configure with CMake
echo "Configuring with CMake..."
cmake "${SCRIPT_DIR}" \
      -DCMAKE_BUILD_TYPE=$BUILD_TYPE \
      -DUSE_MYSQL=$USE_MYSQL \
      -DUSE_POSTGRESQL=$USE_POSTGRESQL \
      -DCMAKE_INSTALL_PREFIX="${INSTALL_DIR}" \
      -DCPP_DBC_BUILD_TESTS=$BUILD_TESTS \
      -Wno-dev

# Build and install the library
echo "Building and installing the library..."
cmake --build . --target install

echo "cpp_dbc library build completed successfully."
echo "The library has been installed to: ${INSTALL_DIR}"
echo ""
echo "Database driver status:"
echo "  MySQL: $USE_MYSQL"
echo "  PostgreSQL: $USE_POSTGRESQL"
echo "  Build type: $BUILD_TYPE"
echo "  Build tests: $BUILD_TESTS"