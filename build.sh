#!/bin/bash

# build.sh
# Script to automate the build process for the C++ demo application with configurable database drivers

set -e

echo "Building C++ demo application..."

# Build the cpp_dbc library using its own build script
echo "Building cpp_dbc library..."

# Parse command line arguments to set USE_MYSQL and USE_POSTGRESQL
USE_MYSQL=ON
USE_POSTGRESQL=OFF

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
        --help)
        echo "Usage: $0 [options]"
        echo "Options:"
        echo "  --mysql, --mysql-on    Enable MySQL support (default)"
        echo "  --mysql-off            Disable MySQL support"
        echo "  --postgres, --postgres-on  Enable PostgreSQL support"
        echo "  --postgres-off         Disable PostgreSQL support"
        echo "  --help                 Show this help message"
        exit 0
        ;;
    esac
done

# Export the variables so they're available to the build script
export USE_MYSQL
export USE_POSTGRESQL

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

./src/libs/cpp_dbc/build_cpp_dbc.sh $MYSQL_PARAM $POSTGRES_PARAM

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
conan install .. --output-folder=. --build=missing

# Configure the main project to use the installed library
echo "Configuring the main project..."
cmake .. -DCMAKE_TOOLCHAIN_FILE=conan_toolchain.cmake \
         -DCMAKE_BUILD_TYPE=Debug \
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
