#!/bin/bash

# build.sh
# Script to automate the build process for the C++ demo application with configurable database drivers

set -e

echo "Building C++ demo application..."

# Build the cpp_dbc library using its own build script
echo "Building cpp_dbc library..."

# Parse command line arguments to set USE_MYSQL, USE_POSTGRESQL, USE_SQLITE, BUILD_TYPE, and BUILD_TESTS
USE_MYSQL=ON
USE_POSTGRESQL=OFF
USE_SQLITE=OFF
USE_CPP_YAML=OFF
BUILD_TYPE=Debug
BUILD_TESTS=OFF
BUILD_EXAMPLES=OFF

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
        --yaml|--yaml-on)
        USE_CPP_YAML=ON
        ;;
        --release)
        BUILD_TYPE=Release
        ;;
        --test)
        BUILD_TESTS=ON
        ;;
        --examples)
        BUILD_EXAMPLES=ON
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
        echo "  --release              Build in Release mode (default: Debug)"
        echo "  --test                 Build cpp_dbc tests"
        echo "  --examples             Build cpp_dbc examples"
        echo "  --help                 Show this help message"
        exit 0
        ;;
    esac
done

# Export the variables so they're available to the build script
export USE_MYSQL
export USE_POSTGRESQL
export USE_SQLITE
export USE_CPP_YAML
export BUILD_TYPE
export BUILD_TESTS
export BUILD_EXAMPLES

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

# Pass the build type to the cpp_dbc build script
if [ "$BUILD_TYPE" = "Release" ]; then
    BUILD_TYPE_PARAM="--release"
else
    BUILD_TYPE_PARAM=""
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

# Pass the YAML support option to the cpp_dbc build script
if [ "$USE_CPP_YAML" = "ON" ]; then
    YAML_PARAM="--yaml"
else
    YAML_PARAM=""
fi

# Build the cpp_dbc library
./libs/cpp_dbc/build_cpp_dbc.sh $MYSQL_PARAM $POSTGRES_PARAM $SQLITE_PARAM $YAML_PARAM $BUILD_TYPE_PARAM $BUILD_TESTS_PARAM $BUILD_EXAMPLES_PARAM

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
         -DCMAKE_PREFIX_PATH=../build/libs/cpp_dbc \
         -Wno-dev

# Build the main project
echo "Building the main project..."
cmake --build .

echo "Build completed successfully!"

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
echo "  YAML support: $USE_CPP_YAML"
echo "  Build type: $BUILD_TYPE"
echo "  Build tests: $BUILD_TESTS"
echo "  Build examples: $BUILD_EXAMPLES"
