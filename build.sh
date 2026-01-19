#!/bin/bash

# build.sh
# Script to automate the build process for the C++ demo application with configurable database drivers

set -e

echo "Building C++ demo application..."

# Save the original command line arguments for IntelliSense synchronization
BUILD_ARGS_FILE="build/.build_args"
mkdir -p build
echo "$@" > "$BUILD_ARGS_FILE"

# Build the cpp_dbc library using its own build script
echo "Building cpp_dbc library..."

# Parse command line arguments to set USE_MYSQL, USE_POSTGRESQL, USE_SQLITE, BUILD_TYPE, and BUILD_TESTS
USE_MYSQL=ON
USE_POSTGRESQL=OFF
USE_SQLITE=OFF
USE_FIREBIRD=OFF
USE_MONGODB=OFF
USE_SCYLLADB=OFF
USE_REDIS=OFF
USE_CPP_YAML=OFF
BUILD_TYPE=Debug
BUILD_TESTS=OFF
BUILD_EXAMPLES=OFF
BUILD_BENCHMARKS=OFF
DEBUG_CONNECTION_POOL=OFF
DEBUG_TRANSACTION_MANAGER=OFF
DEBUG_SQLITE=OFF
DEBUG_FIREBIRD=OFF
DEBUG_MONGODB=OFF
DEBUG_SCYLLADB=OFF
DEBUG_REDIS=OFF
DEBUG_ALL=OFF
BACKWARD_HAS_DW=ON
DB_DRIVER_THREAD_SAFE=ON

for arg in "$@"
do
    case $arg in
        --mysql|--mysql-on)
        USE_MYSQL=ON
        ;;
        --mysql-off)
        USE_MYSQL=OFF
        ;;
        --postgres|--postgres-on)
        USE_POSTGRESQL=ON
        ;;
        --postgres-off)
        USE_POSTGRESQL=OFF
        ;;
        --sqlite|--sqlite-on)
        USE_SQLITE=ON
        ;;
        --sqlite-off)
        USE_SQLITE=OFF
        ;;
        --firebird|--firebird-on)
        USE_FIREBIRD=ON
        ;;
        --firebird-off)
        USE_FIREBIRD=OFF
        ;;
        --mongodb|--mongodb-on)
        USE_MONGODB=ON
        ;;
        --mongodb-off)
        USE_MONGODB=OFF
        ;;
        --scylladb|--scylladb-on)
        USE_SCYLLADB=ON
        ;;
        --scylladb-off)
        USE_SCYLLADB=OFF
        ;;
        --redis|--redis-on)
        USE_REDIS=ON
        ;;
        --redis-off)
        USE_REDIS=OFF
        ;;
        --yaml|--yaml-on)
        USE_CPP_YAML=ON
        ;;
        --release)
        BUILD_TYPE=Release
        ;;
        --gcc-analyzer)
        ENABLE_GCC_ANALYZER=ON
        ;;
        --test)
        BUILD_TESTS=ON
        ;;
        --examples)
        BUILD_EXAMPLES=ON
        ;;
        --benchmarks)
        BUILD_BENCHMARKS=ON
        ;;
        --clean)
        # Clean build directories before building
        echo "Cleaning build directories..."
        rm -rf build
        rm -rf libs/cpp_dbc/build
        ;;
        --debug-pool)
        DEBUG_CONNECTION_POOL=ON
        ;;
        --debug-txmgr)
        DEBUG_TRANSACTION_MANAGER=ON
        ;;
        --debug-sqlite)
        DEBUG_SQLITE=ON
        ;;
        --debug-firebird)
        DEBUG_FIREBIRD=ON
        ;;
        --debug-mongodb)
        DEBUG_MONGODB=ON
        ;;
        --debug-scylladbdb)
        DEBUG_SCYLLADB=ON
        ;;
        --debug-redis)
        DEBUG_REDIS=ON
        ;;
        --debug-all)
        DEBUG_CONNECTION_POOL=ON
        DEBUG_TRANSACTION_MANAGER=ON
        DEBUG_SQLITE=ON
        DEBUG_FIREBIRD=ON
        DEBUG_MONGODB=ON
        DEBUG_REDIS=ON
        DEBUG_ALL=ON
        ;;
        --dw-off)
        BACKWARD_HAS_DW=OFF
        ;;
        --db-driver-thread-safe-off)
        DB_DRIVER_THREAD_SAFE=OFF
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
        echo "  --yaml, --yaml-on      Enable YAML configuration support"
        echo "  --clean                Clean build directories before building"
        echo "  --release              Build in Release mode (default: Debug)"
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
        exit 0
        ;;
    esac
done

# Export the variables so they're available to the build script
export USE_MYSQL
export USE_POSTGRESQL
export USE_SQLITE
export USE_FIREBIRD
export USE_MONGODB
export USE_SCYLLADB
export USE_REDIS
export USE_CPP_YAML
export ENABLE_GCC_ANALYZER
export BUILD_TYPE
export BUILD_TESTS
export BUILD_EXAMPLES
export BUILD_BENCHMARKS
export DEBUG_CONNECTION_POOL
export DEBUG_TRANSACTION_MANAGER
export DEBUG_SQLITE
export DEBUG_FIREBIRD
export DEBUG_MONGODB
export DEBUG_SCYLLADB
export DEBUG_ALL
export BACKWARD_HAS_DW
export DB_DRIVER_THREAD_SAFE

# Call the cpp_dbc build script with explicit parameters
if [ "$USE_MYSQL" = "ON" ]; then
    MYSQL_PARAM="--mysql"
else
    MYSQL_PARAM="--mysql-off"
fi

if [ "$USE_POSTGRESQL" = "ON" ]; then
    POSTGRES_PARAM="--postgres"
else
    POSTGRES_PARAM="--postgres-off"
fi

if [ "$USE_SQLITE" = "ON" ]; then
    SQLITE_PARAM="--sqlite"
else
    SQLITE_PARAM="--sqlite-off"
fi

if [ "$USE_FIREBIRD" = "ON" ]; then
    FIREBIRD_PARAM="--firebird"
else
    FIREBIRD_PARAM="--firebird-off"
fi

if [ "$USE_MONGODB" = "ON" ]; then
    MONGODB_PARAM="--mongodb"
else
    MONGODB_PARAM="--mongodb-off"
fi

if [ "$USE_SCYLLADB" = "ON" ]; then
    SCYLLA_PARAM="--scylladb"
else
    SCYLLA_PARAM="--scylladb-off"
fi

if [ "$USE_REDIS" = "ON" ]; then
    REDIS_PARAM="--redis"
else
    REDIS_PARAM="--redis-off"
fi

# Pass the build type to the cpp_dbc build script
if [ "$BUILD_TYPE" = "Release" ]; then
    BUILD_TYPE_PARAM="--release"
else
    BUILD_TYPE_PARAM=""
fi

if [ "$ENABLE_GCC_ANALYZER" = "ON" ]; then
    BUILD_TYPE_PARAM="$BUILD_TYPE_PARAM --gcc-analyzer"
fi

# Pass the build tests option to the cpp_dbc build script
if [ "$BUILD_TESTS" = "ON" ]; then
    BUILD_TESTS_PARAM="--test"
else
    BUILD_TESTS_PARAM=""
fi

# Pass the build examples option to the cpp_dbc build script
if [ "$BUILD_EXAMPLES" = "ON" ]; then
    BUILD_EXAMPLES_PARAM="--examples"
else
    BUILD_EXAMPLES_PARAM=""
fi

# Pass the build benchmarks option to the cpp_dbc build script
if [ "$BUILD_BENCHMARKS" = "ON" ]; then
    BUILD_BENCHMARKS_PARAM="--benchmarks"
else
    BUILD_BENCHMARKS_PARAM=""
fi

# Pass the YAML support option to the cpp_dbc build script
if [ "$USE_CPP_YAML" = "ON" ]; then
    YAML_PARAM="--yaml"
else
    YAML_PARAM=""
fi

# Pass the debug options to the cpp_dbc build script
DEBUG_PARAMS=""
if [ "$DEBUG_CONNECTION_POOL" = "ON" ]; then
    DEBUG_PARAMS="$DEBUG_PARAMS --debug-pool"
fi

if [ "$DEBUG_TRANSACTION_MANAGER" = "ON" ]; then
    DEBUG_PARAMS="$DEBUG_PARAMS --debug-txmgr"
fi

if [ "$DEBUG_SQLITE" = "ON" ]; then
    DEBUG_PARAMS="$DEBUG_PARAMS --debug-sqlite"
fi

if [ "$DEBUG_FIREBIRD" = "ON" ]; then
    DEBUG_PARAMS="$DEBUG_PARAMS --debug-firebird"
fi

if [ "$DEBUG_MONGODB" = "ON" ]; then
    DEBUG_PARAMS="$DEBUG_PARAMS --debug-mongodb"
fi

if [ "$DEBUG_SCYLLADB" = "ON" ]; then
    DEBUG_PARAMS="$DEBUG_PARAMS --debug-scylladb"
fi

if [ "$DEBUG_ALL" = "ON" ]; then
    DEBUG_PARAMS="$DEBUG_PARAMS --debug-all"
fi

# Pass the libdw option to the cpp_dbc build script
if [ "$BACKWARD_HAS_DW" = "OFF" ]; then
    DEBUG_PARAMS="$DEBUG_PARAMS --dw-off"
fi

# Pass the db-driver-thread-safe-off option to the cpp_dbc build script
if [ "$DB_DRIVER_THREAD_SAFE" = "OFF" ]; then
    DEBUG_PARAMS="$DEBUG_PARAMS --db-driver-thread-safe-off"
fi

echo "$0 >= Running ./libs/cpp_dbc/build_cpp_dbc.sh "
# Build the cpp_dbc library
./libs/cpp_dbc/build_cpp_dbc.sh $MYSQL_PARAM $POSTGRES_PARAM $SQLITE_PARAM $FIREBIRD_PARAM $MONGODB_PARAM $SCYLLA_PARAM $REDIS_PARAM $YAML_PARAM $BUILD_TYPE_PARAM $BUILD_TESTS_PARAM $BUILD_EXAMPLES_PARAM $BUILD_BENCHMARKS_PARAM $DEBUG_PARAMS

# If the cpp_dbc build script fails, stop the build process
if [ $? -ne 0 ]; then
    echo "Error: cpp_dbc library build failed. Stopping."
    exit 1
fi

# Create build directory for the main project
mkdir -p build
cd build

# Generate Conan files for the main project
echo "Generating Conan files for the main project..."
echo "Build type: $BUILD_TYPE"

# Use the specified build type for Conan
conan install .. --build=missing -s build_type=$BUILD_TYPE

# Configure the main project to use the installed library
echo "Configuring the main project..."
# Find the conan_toolchain.cmake file in the specific build type directory
TOOLCHAIN_FILE="./${BUILD_TYPE}/generators/conan_toolchain.cmake"
if [ ! -f "$TOOLCHAIN_FILE" ]; then
    echo "Error: Could not find conan_toolchain.cmake file in ${BUILD_TYPE}/generators. Conan installation may have failed."
    exit 1
fi
echo "Using toolchain file: $TOOLCHAIN_FILE"

cmake .. -DCMAKE_TOOLCHAIN_FILE=$TOOLCHAIN_FILE \
         -DCMAKE_BUILD_TYPE=$BUILD_TYPE \
         -DUSE_MYSQL=$USE_MYSQL \
         -DUSE_POSTGRESQL=$USE_POSTGRESQL \
         -DUSE_SQLITE=$USE_SQLITE \
         -DUSE_FIREBIRD=$USE_FIREBIRD \
         -DUSE_MONGODB=$USE_MONGODB \
         -DUSE_SCYLLADB=$USE_SCYLLADB \
         -DUSE_CPP_YAML=$USE_CPP_YAML \
         -DCMAKE_PREFIX_PATH=../build/libs/cpp_dbc \
         -Wno-dev

# Build the main project
echo "Building the main project..."
cmake --build .

echo "Build completed successfully!"

# Save build configuration for IntelliSense synchronization
BUILD_CONFIG_FILE="../build/.build_config"
echo "# Build configuration - Auto-generated by build.sh" > "$BUILD_CONFIG_FILE"
echo "# This file is used to keep VSCode IntelliSense in sync with build settings" >> "$BUILD_CONFIG_FILE"
echo "USE_MYSQL=$USE_MYSQL" >> "$BUILD_CONFIG_FILE"
echo "USE_POSTGRESQL=$USE_POSTGRESQL" >> "$BUILD_CONFIG_FILE"
echo "USE_SQLITE=$USE_SQLITE" >> "$BUILD_CONFIG_FILE"
echo "USE_FIREBIRD=$USE_FIREBIRD" >> "$BUILD_CONFIG_FILE"
echo "USE_MONGODB=$USE_MONGODB" >> "$BUILD_CONFIG_FILE"
echo "USE_SCYLLADB=$USE_SCYLLADB" >> "$BUILD_CONFIG_FILE"
echo "USE_REDIS=$USE_REDIS" >> "$BUILD_CONFIG_FILE"
echo "USE_CPP_YAML=$USE_CPP_YAML" >> "$BUILD_CONFIG_FILE"
echo "BUILD_TYPE=$BUILD_TYPE" >> "$BUILD_CONFIG_FILE"
echo "DB_DRIVER_THREAD_SAFE=$DB_DRIVER_THREAD_SAFE" >> "$BUILD_CONFIG_FILE"
echo "BACKWARD_HAS_DW=$BACKWARD_HAS_DW" >> "$BUILD_CONFIG_FILE"
echo "DEBUG_CONNECTION_POOL=$DEBUG_CONNECTION_POOL" >> "$BUILD_CONFIG_FILE"
echo "DEBUG_TRANSACTION_MANAGER=$DEBUG_TRANSACTION_MANAGER" >> "$BUILD_CONFIG_FILE"
echo "DEBUG_SQLITE=$DEBUG_SQLITE" >> "$BUILD_CONFIG_FILE"
echo "DEBUG_FIREBIRD=$DEBUG_FIREBIRD" >> "$BUILD_CONFIG_FILE"
echo "DEBUG_MONGODB=$DEBUG_MONGODB" >> "$BUILD_CONFIG_FILE"
echo "DEBUG_SCYLLADB=$DEBUG_SCYLLADB" >> "$BUILD_CONFIG_FILE"
echo "DEBUG_REDIS=$DEBUG_REDIS" >> "$BUILD_CONFIG_FILE"
echo "DEBUG_ALL=$DEBUG_ALL" >> "$BUILD_CONFIG_FILE"
echo ""
echo "✓ Build configuration saved to build/.build_config"

# Get the binary name from .dist_build using a more precise method
if [ -f "../.dist_build" ]; then
    BIN_NAME=$(awk -F'"' '/^Container_Bin_Name=/{print $2}' ../.dist_build)
else
    BIN_NAME="cpp_dbc_demo"
fi
echo "You can run the application with: ./build/${BIN_NAME}"
echo ""
echo "Options status:"
echo "  MySQL: $USE_MYSQL"
echo "  PostgreSQL: $USE_POSTGRESQL"
echo "  SQLite: $USE_SQLITE"
echo "  Firebird: $USE_FIREBIRD"
echo "  MongoDB: $USE_MONGODB"
echo "  ScyllaDB: $USE_SCYLLADB"
echo "  Redis: $USE_REDIS"
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
echo "  Debug Redis: $DEBUG_REDIS"
echo "  Debug All: $DEBUG_ALL"
echo "  libdw support: $BACKWARD_HAS_DW"
echo "  DB driver thread-safe: $DB_DRIVER_THREAD_SAFE"
