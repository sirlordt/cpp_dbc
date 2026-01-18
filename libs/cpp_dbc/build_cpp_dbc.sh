#!/bin/bash

# build_cpp_dbc.sh
# Script to install dependencies and build the cpp_dbc library

set -e

# Determine script directory
SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )"

# Default values
USE_MYSQL=ON
USE_POSTGRESQL=OFF
USE_SQLITE=OFF
USE_FIREBIRD=OFF
USE_MONGODB=OFF
USE_SCYLLADB=OFF
USE_REDIS=OFF
USE_CPP_YAML=OFF
ENABLE_GCC_ANALYZER=OFF
ENABLE_ASAN=OFF
BUILD_TYPE=Debug
BUILD_TESTS=OFF
BUILD_EXAMPLES=OFF
BUILD_BENCHMARKS=OFF
DEBUG_CONNECTION_POOL=OFF
DEBUG_TRANSACTION_MANAGER=OFF
DEBUG_SQLITE=OFF
DEBUG_FIREBIRD=OFF
DEBUG_MONGODB=OFF
DEBUG_REDIS=OFF
DEBUG_ALL=OFF
BACKWARD_HAS_DW=ON
DB_DRIVER_THREAD_SAFE=ON

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
        USE_CPP_YAML=ON
        shift
        ;;
        --yaml-off)
        USE_CPP_YAML=OFF
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
        --gcc-analyzer)
        ENABLE_GCC_ANALYZER=ON
        shift
        ;;
        --asan)
        ENABLE_ASAN=ON
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
        --benchmarks)
        BUILD_BENCHMARKS=ON
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
        --debug-scylladbdb)
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
        DEBUG_SCYLLADB=ON
        DEBUG_REDIS=ON
        DEBUG_ALL=ON
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
        --help)
        echo "Usage: $0 [options]"
        echo "Options:"
        echo "  --mysql, --mysql-on    Enable MySQL support (default)"
        echo "  --mysql-off            Disable MySQL support"
        echo "  --postgres, --postgres-on  Enable PostgreSQL support"
        echo "  --postgres-off         Disable PostgreSQL support"
        echo "  --sqlite, --sqlite-on  Enable SQLite support"
        echo "  --sqlite-off           Disable SQLite support"
        echo "  --firebird, --firebird-on  Enable Firebird SQL support"
        echo "  --firebird-off         Disable Firebird SQL support"
        echo "  --mongodb, --mongodb-on  Enable MongoDB support"
        echo "  --mongodb-off          Disable MongoDB support"
        echo "  --scylladb, --scylladb-on    Enable ScyllaDB support"
        echo "  --scylladb-off           Disable ScyllaDB support"
        echo "  --redis, --redis-on    Enable Redis support"
        echo "  --redis-off            Disable Redis support"
        echo "  --yaml, --yaml-on      Enable YAML configuration support"
        echo "  --debug                Build in Debug mode (default)"
        echo "  --release              Build in Release mode"
        echo "  --gcc-analyzer         Enable GCC Static Analyzer (GCC 10+)"
        echo "  --test                 Build cpp_dbc tests"
        echo "  --examples             Build cpp_dbc examples"
        echo "  --benchmarks           Build cpp_dbc benchmarks"
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
        echo "  --help                 Show this help message"
        exit 1
        ;;
    esac
done

echo "Building cpp_dbc library..."
echo "Database driver configuration:"
echo "  MySQL support: $USE_MYSQL"
echo "  PostgreSQL support: $USE_POSTGRESQL"
echo "  SQLite support: $USE_SQLITE"
echo "  Firebird support: $USE_FIREBIRD"
echo "  MongoDB support: $USE_MONGODB"
echo "  ScyllaDB support: $USE_SCYLLADB"
echo "  YAML support: $USE_CPP_YAML"
echo "  Build type: $BUILD_TYPE"
echo "  Build tests: $BUILD_TESTS"
echo "  Build examples: $BUILD_EXAMPLES"
echo "  Build benchmarks: $BUILD_BENCHMARKS"
echo "  Debug ConnectionPool: $DEBUG_CONNECTION_POOL"
echo "  Debug TransactionManager: $DEBUG_TRANSACTION_MANAGER"
echo "  Debug SQLite: $DEBUG_SQLITE"
echo "  Debug Firebird: $DEBUG_FIREBIRD"
echo "  Debug MongoDB: $DEBUG_MONGODB"
echo "  Debug ScyllaDB: $DEBUG_SCYLLADB"
echo "  Debug All: $DEBUG_ALL"
echo "  libdw support: $BACKWARD_HAS_DW"
echo "  DB driver thread-safe: $DB_DRIVER_THREAD_SAFE"

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

# Check for SQLite dependencies
if [ "$USE_SQLITE" = "ON" ]; then
    echo "Checking for SQLite development libraries..."
    
    # Detect package manager
    if command -v apt-get &> /dev/null; then
        # Debian/Ubuntu
        if ! dpkg -l | grep -q libsqlite3-dev; then
            echo "SQLite development libraries not found. Installing..."
            sudo apt-get update
            sudo apt-get install -y libsqlite3-dev
        else
            echo "SQLite development libraries already installed."
        fi
    elif command -v dnf &> /dev/null; then
        # Fedora/RHEL/CentOS
        if ! rpm -q sqlite-devel &> /dev/null; then
            echo "SQLite development libraries not found. Installing..."
            sudo dnf install -y sqlite-devel
        else
            echo "SQLite development libraries already installed."
        fi
    elif command -v yum &> /dev/null; then
        # Older RHEL/CentOS
        if ! rpm -q sqlite-devel &> /dev/null; then
            echo "SQLite development libraries not found. Installing..."
            sudo yum install -y sqlite-devel
        else
            echo "SQLite development libraries already installed."
        fi
    else
        echo "Warning: Could not detect package manager. Please install SQLite development libraries manually."
    fi
fi

# Check for Firebird dependencies
if [ "$USE_FIREBIRD" = "ON" ]; then
    echo "Checking for Firebird development libraries..."
    
    # Detect package manager
    if command -v apt-get &> /dev/null; then
        # Debian/Ubuntu
        if ! dpkg -l | grep -q firebird-dev; then
            echo "Firebird development libraries not found. Installing..."
            sudo apt-get update
            sudo apt-get install -y firebird-dev libfbclient2
        else
            echo "Firebird development libraries already installed."
        fi
    elif command -v dnf &> /dev/null; then
        # Fedora/RHEL/CentOS
        if ! rpm -q firebird-devel &> /dev/null; then
            echo "Firebird development libraries not found. Installing..."
            sudo dnf install -y firebird-devel libfbclient2
        else
            echo "Firebird development libraries already installed."
        fi
    elif command -v yum &> /dev/null; then
        # Older RHEL/CentOS
        if ! rpm -q firebird-devel &> /dev/null; then
            echo "Firebird development libraries not found. Installing..."
            sudo yum install -y firebird-devel libfbclient2
        else
            echo "Firebird development libraries already installed."
        fi
    else
        echo "Warning: Could not detect package manager. Please install Firebird development libraries manually."
        echo "  Debian/Ubuntu: sudo apt-get install firebird-dev libfbclient2"
        echo "  Fedora/RHEL: sudo dnf install firebird-devel libfbclient2"
    fi
fi

# Check for MongoDB dependencies
if [ "$USE_MONGODB" = "ON" ]; then
    echo "Checking for MongoDB C driver development libraries..."
    
    # Detect package manager
    if command -v apt-get &> /dev/null; then
        # Debian/Ubuntu
        if ! dpkg -l | grep -q libmongoc-dev; then
            echo "MongoDB C driver development libraries not found. Installing..."
            sudo apt-get update
            sudo apt-get install -y libmongoc-dev libbson-dev
        else
            echo "MongoDB C driver development libraries already installed."
        fi
    elif command -v dnf &> /dev/null; then
        # Fedora/RHEL/CentOS
        if ! rpm -q mongo-c-driver-devel &> /dev/null; then
            echo "MongoDB C driver development libraries not found. Installing..."
            sudo dnf install -y mongo-c-driver-devel libbson-devel
        else
            echo "MongoDB C driver development libraries already installed."
        fi
    elif command -v yum &> /dev/null; then
        # Older RHEL/CentOS
        if ! rpm -q mongo-c-driver-devel &> /dev/null; then
            echo "MongoDB C driver development libraries not found. Installing..."
            sudo yum install -y mongo-c-driver-devel libbson-devel
        else
            echo "MongoDB C driver development libraries already installed."
        fi
    else
        echo "Warning: Could not detect package manager. Please install MongoDB C driver development libraries manually."
        echo "  Debian/Ubuntu: sudo apt-get install libmongoc-dev libbson-dev"
        echo "  Fedora/RHEL: sudo dnf install mongo-c-driver-devel libbson-devel"
    fi
fi

# Check for ScyllaDB dependencies
if [ "$USE_SCYLLADB" = "ON" ]; then
    echo "Checking for ScyllaDB (Cassandra) driver development libraries..."
    
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

# Check for Redis dependencies
if [ "$USE_REDIS" = "ON" ]; then
    echo "Checking for Redis development libraries..."
    
    # Detect package manager
    if command -v apt-get &> /dev/null; then
        # Debian/Ubuntu
        if ! dpkg -l | grep -q libhiredis-dev; then
            echo "Redis development libraries not found. Installing..."
            sudo apt-get update
            sudo apt-get install -y libhiredis-dev
        else
            echo "Redis development libraries already installed."
        fi
    elif command -v dnf &> /dev/null; then
        # Fedora/RHEL/CentOS
        if ! rpm -q hiredis-devel &> /dev/null; then
            echo "Redis development libraries not found. Installing..."
            sudo dnf install -y hiredis-devel
        else
            echo "Redis development libraries already installed."
        fi
    elif command -v yum &> /dev/null; then
        # Older RHEL/CentOS
        if ! rpm -q hiredis-devel &> /dev/null; then
            echo "Redis development libraries not found. Installing..."
            sudo yum install -y hiredis-devel
        else
            echo "Redis development libraries already installed."
        fi
    else
        echo "Warning: Could not detect package manager. Please install Redis development libraries manually."
        echo "  Debian/Ubuntu: sudo apt-get install libhiredis-dev"
        echo "  Fedora/RHEL: sudo dnf install hiredis-devel"
    fi
fi

# Check for libdw dependencies if enabled
if [ "$BACKWARD_HAS_DW" = "ON" ]; then
    echo "Checking for libdw development libraries..."
    
    # Detect package manager
    if command -v apt-get &> /dev/null; then
        # Debian/Ubuntu
        if ! dpkg -l | grep -q libdw-dev; then
            echo "libdw development libraries not found. Installing..."
            sudo apt-get update
            sudo apt-get install -y libdw-dev
        else
            echo "libdw development libraries already installed."
        fi
    elif command -v dnf &> /dev/null; then
        # Fedora/RHEL/CentOS
        if ! rpm -q elfutils-devel &> /dev/null; then
            echo "libdw development libraries not found. Installing..."
            sudo dnf install -y elfutils-devel
        else
            echo "libdw development libraries already installed."
        fi
    elif command -v yum &> /dev/null; then
        # Older RHEL/CentOS
        if ! rpm -q elfutils-devel &> /dev/null; then
            echo "libdw development libraries not found. Installing..."
            sudo yum install -y elfutils-devel
        else
            echo "libdw development libraries already installed."
        fi
    else
        echo "Warning: Could not detect package manager. Please install libdw development libraries manually."
    fi
fi

# Create build directory
# SCRIPT_DIR is defined at the top
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
      -DUSE_SQLITE=$USE_SQLITE \
      -DUSE_FIREBIRD=$USE_FIREBIRD \
      -DUSE_MONGODB=$USE_MONGODB \
      -DUSE_SCYLLADB=$USE_SCYLLADB \
      -DUSE_REDIS=$USE_REDIS \
      -DUSE_CPP_YAML=$USE_CPP_YAML \
      -DCMAKE_INSTALL_PREFIX="${INSTALL_DIR}" \
      -DCPP_DBC_BUILD_TESTS=$BUILD_TESTS \
      -DCPP_DBC_BUILD_EXAMPLES=$BUILD_EXAMPLES \
      -DCPP_DBC_BUILD_BENCHMARKS=$BUILD_BENCHMARKS \
      -DDEBUG_CONNECTION_POOL=$DEBUG_CONNECTION_POOL \
      -DDEBUG_TRANSACTION_MANAGER=$DEBUG_TRANSACTION_MANAGER \
      -DDEBUG_SQLITE=$DEBUG_SQLITE \
      -DDEBUG_FIREBIRD=$DEBUG_FIREBIRD \
      -DDEBUG_MONGODB=$DEBUG_MONGODB \
      -DDEBUG_SCYLLADB=$DEBUG_SCYLLADB \
      -DDEBUG_REDIS=$DEBUG_REDIS \
      -DDEBUG_ALL=$DEBUG_ALL \
      -DBACKWARD_HAS_DW=$BACKWARD_HAS_DW \
      -DDB_DRIVER_THREAD_SAFE=$DB_DRIVER_THREAD_SAFE \
      -DENABLE_ASAN=$ENABLE_ASAN \
      -DCMAKE_TOOLCHAIN_FILE=$TOOLCHAIN_FILE \
      -DCMAKE_CXX_FLAGS="-Wall -Wextra -Wpedantic -Wconversion -Wshadow -Wcast-qual -Wformat=2 -Wunused -Werror=return-type -Werror=switch -Wdouble-promotion -Wfloat-equal -Wundef -Wpointer-arith -Wcast-align ${ANALYZER_FLAG}" \
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
    
    # Pass SQLite configuration
    if [ "$USE_SQLITE" = "ON" ]; then
        TEST_PARAMS="$TEST_PARAMS --sqlite"
    fi
    
    # Pass Firebird configuration
    if [ "$USE_FIREBIRD" = "ON" ]; then
        TEST_PARAMS="$TEST_PARAMS --firebird"
    fi
    
    # Pass MongoDB configuration
    if [ "$USE_MONGODB" = "ON" ]; then
        TEST_PARAMS="$TEST_PARAMS --mongodb"
    fi

    # Pass ScyllaDB configuration
    if [ "$USE_SCYLLADB" = "ON" ]; then
        TEST_PARAMS="$TEST_PARAMS --scylladb"
    fi
    
    # Pass Redis configuration
    if [ "$USE_REDIS" = "ON" ]; then
        TEST_PARAMS="$TEST_PARAMS --redis"
    fi
    
    # Pass build type
    if [ "$BUILD_TYPE" = "Release" ]; then
        TEST_PARAMS="$TEST_PARAMS --release"
    fi

    if [ "$ENABLE_GCC_ANALYZER" = "ON" ]; then
        TEST_PARAMS="$TEST_PARAMS --gcc-analyzer"
    fi

    if [ "$ENABLE_ASAN" = "ON" ]; then
        TEST_PARAMS="$TEST_PARAMS --asan"
    fi
    
    # Pass debug options
    if [ "$DEBUG_CONNECTION_POOL" = "ON" ]; then
        TEST_PARAMS="$TEST_PARAMS --debug-pool"
    fi
    
    if [ "$DEBUG_TRANSACTION_MANAGER" = "ON" ]; then
        TEST_PARAMS="$TEST_PARAMS --debug-txmgr"
    fi
    
    if [ "$DEBUG_SQLITE" = "ON" ]; then
        TEST_PARAMS="$TEST_PARAMS --debug-sqlite"
    fi
    
    if [ "$DEBUG_FIREBIRD" = "ON" ]; then
        TEST_PARAMS="$TEST_PARAMS --debug-firebird"
    fi
    
    if [ "$DEBUG_MONGODB" = "ON" ]; then
        TEST_PARAMS="$TEST_PARAMS --debug-mongodb"
    fi

    if [ "$DEBUG_SCYLLADB" = "ON" ]; then
        TEST_PARAMS="$TEST_PARAMS --debug-scylladb"
    fi
    
    if [ "$DEBUG_REDIS" = "ON" ]; then
        TEST_PARAMS="$TEST_PARAMS --debug-redis"
    fi
    
    if [ "$DEBUG_ALL" = "ON" ]; then
        TEST_PARAMS="$TEST_PARAMS --debug-all"
    fi
    
    # Call build_test_cpp_dbc.sh with the parameters
    echo "Running: ${SCRIPT_DIR}/build_test_cpp_dbc.sh $TEST_PARAMS"
    "${SCRIPT_DIR}/build_test_cpp_dbc.sh" $TEST_PARAMS
fi

echo "Database driver status:"
echo "  MySQL: $USE_MYSQL"
echo "  PostgreSQL: $USE_POSTGRESQL"
echo "  SQLite: $USE_SQLITE"
echo "  Firebird: $USE_FIREBIRD"
echo "  MongoDB: $USE_MONGODB"
echo "  ScyllaDB: $USE_SCYLLADB"
echo "  YAML support: $USE_CPP_YAML"
echo "  Build type: $BUILD_TYPE"
echo "  Build tests: $BUILD_TESTS"
echo "  Build examples: $BUILD_EXAMPLES"
echo "  Build benchmarks: $BUILD_BENCHMARKS"
echo "  Debug ConnectionPool: $DEBUG_CONNECTION_POOL"
echo "  Debug TransactionManager: $DEBUG_TRANSACTION_MANAGER"
echo "  Debug SQLite: $DEBUG_SQLITE"
echo "  Debug Firebird: $DEBUG_FIREBIRD"
echo "  Debug MongoDB: $DEBUG_MONGODB"
echo "  Debug Redis: $DEBUG_REDIS"
echo "  Debug All: $DEBUG_ALL"
echo "  libdw support: $BACKWARD_HAS_DW"
echo "  DB driver thread-safe: $DB_DRIVER_THREAD_SAFE"