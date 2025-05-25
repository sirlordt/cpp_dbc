#!/bin/bash

# build.sh
# Script to automate the build process for the C++ demo application with configurable database drivers

set -e

echo "Building C++ demo application..."

# Build the cpp_dbc library using its own build script
echo "Building cpp_dbc library..."

# Parse command line arguments to set USE_MYSQL, USE_POSTGRESQL, and BUILD_TYPE
USE_MYSQL=ON
USE_POSTGRESQL=OFF
BUILD_TYPE=Debug

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
        --release)
        BUILD_TYPE=Release
        ;;
        --help)
        echo "Usage: $0 [options]"
        echo "Options:"
        echo "  --mysql, --mysql-on    Enable MySQL support (default)"
        echo "  --mysql-off            Disable MySQL support"
        echo "  --postgres, --postgres-on  Enable PostgreSQL support"
        echo "  --postgres-off         Disable PostgreSQL support"
        echo "  --release              Build in Release mode (default: Debug)"
        echo "  --help                 Show this help message"
        exit 0
        ;;
    esac
done

# Export the variables so they're available to the build script
export USE_MYSQL
export USE_POSTGRESQL
export BUILD_TYPE

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

# Pass the build type to the cpp_dbc build script
if [ "$BUILD_TYPE" = "Release" ]; then
    BUILD_TYPE_PARAM="--release"
else
    BUILD_TYPE_PARAM=""
fi

# First, install dependencies for cpp_dbc library including Catch2
echo "Installing dependencies for cpp_dbc library..."
cd libs/cpp_dbc
conan install . --build=missing -s build_type=$BUILD_TYPE
cd ../..

# Now build the cpp_dbc library
./libs/cpp_dbc/build_cpp_dbc.sh $MYSQL_PARAM $POSTGRES_PARAM $BUILD_TYPE_PARAM

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
echo "Database driver status:"
echo "  MySQL: $USE_MYSQL"
echo "  PostgreSQL: $USE_POSTGRESQL"
echo "  Build type: $BUILD_TYPE"
