#!/bin/bash
set -e

# Default values
DISTRO="ubuntu:24.04"
BUILD_OPTIONS=""

# Function to display usage information
function show_usage {
    echo "Usage: $0 [OPTIONS]"
    echo "Build a .deb package for cpp_dbc library in a Docker container."
    echo ""
    echo "Options:"
    echo "  --distro=DISTRO     Specify the distribution to build for."
    echo "                      Supported values: debian:12, debian:13, ubuntu:22.04, ubuntu:24.04"
    echo "                      Default: debian:12"
    echo "  --build=OPTIONS     Comma-separated build options."
    echo "                      Supported values: yaml, mysql, postgres, sqlite, debug, dw, examples"
    echo "                      Default: yaml,mysql,postgres,sqlite,debug,dw"
    echo "  --help              Display this help message and exit"
    echo ""
    echo "Example:"
    echo "  $0 --distro=ubuntu:24.04 --build=yaml,mysql,postgres,sqlite,debug,dw,examples"
    exit 1
}

# Parse command line arguments
for arg in "$@"; do
    case $arg in
        --distro=*)
            DISTRO="${arg#*=}"
            ;;
        --build=*)
            BUILD_OPTIONS="${arg#*=}"
            ;;
        --help)
            show_usage
            ;;
        *)
            echo "Unknown option: $arg"
            show_usage
            ;;
    esac
done

# Set default build options if not provided
if [ -z "$BUILD_OPTIONS" ]; then
    BUILD_OPTIONS="yaml,mysql,postgres,sqlite,dw,debug"
fi

# Set default build options if not provided
if [ -z "$DISTRO" ]; then
    DiSTRO="ubuntu_24_04"
fi

# Convert distro format to directory name
case $DISTRO in
    debian:12)
        DISTRO_DIR="debian_12"
        MYSQL_DEV_PKG="default-libmysqlclient-dev"
        POSTGRESQL_DEV_PKG="libpq-dev"
        SQLITE_DEV_PKG="libsqlite3-dev"
        LIBDW_DEV_PKG="libdw-dev"
        ;;
    debian:13)
        DISTRO_DIR="debian_13"
        MYSQL_DEV_PKG="default-libmysqlclient-dev"
        POSTGRESQL_DEV_PKG="libpq-dev"
        SQLITE_DEV_PKG="libsqlite3-dev"
        LIBDW_DEV_PKG="libdw-dev"
        ;;
    ubuntu:22.04)
        DISTRO_DIR="ubuntu_22_04"
        MYSQL_DEV_PKG="default-libmysqlclient-dev"
        POSTGRESQL_DEV_PKG="libpq-dev"
        SQLITE_DEV_PKG="libsqlite3-dev"
        LIBDW_DEV_PKG="libdw-dev"
        ;;
    ubuntu:24.04)
        DISTRO_DIR="ubuntu_24_04"
        MYSQL_DEV_PKG="default-libmysqlclient-dev"
        POSTGRESQL_DEV_PKG="libpq-dev"
        SQLITE_DEV_PKG="libsqlite3-dev"
        LIBDW_DEV_PKG="libdw-dev"
        ;;
    *)
        echo "Unsupported distribution: $DISTRO"
        show_usage
        ;;
esac

# Set up build options
CMAKE_YAML_OPTION="-DCPP_DBC_WITH_YAML=OFF"
CMAKE_MYSQL_OPTION="-DCPP_DBC_WITH_MYSQL=OFF"
CMAKE_POSTGRESQL_OPTION="-DCPP_DBC_WITH_POSTGRESQL=OFF"
CMAKE_SQLITE_OPTION="-DCPP_DBC_WITH_SQLITE=OFF"
CMAKE_DW_OPTION="-DBACKWARD_HAS_DW=OFF"

DEB_DEPENDENCIES="libc6"

# Process build options
MYSQL_CONTROL_DEP=""
POSTGRESQL_CONTROL_DEP=""
SQLITE_CONTROL_DEP=""
LIBDW_CONTROL_DEP=""
USE_MYSQL="OFF"
USE_POSTGRESQL="OFF"
USE_SQLITE="OFF"
USE_CPP_YAML="OFF"
USE_DW="OFF"
BUILD_TYPE="Debug"
BUILD_EXAMPLES="OFF"
# Create a variable to store the build flags for the Debian package
BUILD_FLAGS=""

IFS=',' read -ra OPTIONS <<< "$BUILD_OPTIONS"
for option in "${OPTIONS[@]}"; do
    case $option in
        yaml)
            CMAKE_YAML_OPTION="-DCPP_DBC_WITH_YAML=ON"
            DEB_DEPENDENCIES="$DEB_DEPENDENCIES, libyaml-cpp-dev"
            USE_CPP_YAML="ON"
            BUILD_FLAGS="$BUILD_FLAGS --yaml"
            ;;
        mysql)
            CMAKE_MYSQL_OPTION="-DCPP_DBC_WITH_MYSQL=ON"
            DEB_DEPENDENCIES="$DEB_DEPENDENCIES, $MYSQL_DEV_PKG"
            MYSQL_CONTROL_DEP=", $MYSQL_DEV_PKG"
            USE_MYSQL="ON"
            BUILD_FLAGS="$BUILD_FLAGS --mysql"
            ;;
        postgres)
            CMAKE_POSTGRESQL_OPTION="-DCPP_DBC_WITH_POSTGRESQL=ON"
            DEB_DEPENDENCIES="$DEB_DEPENDENCIES, $POSTGRESQL_DEV_PKG"
            POSTGRESQL_CONTROL_DEP=", $POSTGRESQL_DEV_PKG"
            USE_POSTGRESQL="ON"
            BUILD_FLAGS="$BUILD_FLAGS --postgres"
            ;;
        sqlite)
            CMAKE_SQLITE_OPTION="-DCPP_DBC_WITH_SQLITE=ON"
            DEB_DEPENDENCIES="$DEB_DEPENDENCIES, $SQLITE_DEV_PKG"
            SQLITE_CONTROL_DEP=", $SQLITE_DEV_PKG"
            USE_SQLITE="ON"
            BUILD_FLAGS="$BUILD_FLAGS --sqlite"
            ;;
        debug)
            BUILD_TYPE="Debug"
            BUILD_FLAGS="$BUILD_FLAGS --debug"
            ;;
        dw)
            USE_DW="ON"
            CMAKE_DW_OPTION="-DBACKWARD_HAS_DW=ON"
            BUILD_FLAGS="$BUILD_FLAGS --dw"
            ;;
        examples)
            BUILD_EXAMPLES="ON"
            BUILD_FLAGS="$BUILD_FLAGS --examples"
            ;;
        *)
            echo "Unknown build option: $option"
            show_usage
            ;;
    esac
done

# Add libdw to dependencies if enabled
if [ "$USE_DW" = "ON" ]; then
    DEB_DEPENDENCIES="$DEB_DEPENDENCIES, $LIBDW_DEV_PKG"
    LIBDW_CONTROL_DEP=", $LIBDW_DEV_PKG"
fi

# Create build directory if it doesn't exist
mkdir -p build

# Create temporary build directory
TEMP_BUILD_DIR=$(mktemp -d)

# Copy Dockerfile and build script to temporary directory
cp -r "$(dirname "$0")/distros/$DISTRO_DIR/"* "$TEMP_BUILD_DIR/"

# Replace placeholders in Dockerfile
sed -i "s/__MYSQL_DEV_PKG__/$MYSQL_DEV_PKG/g" "$TEMP_BUILD_DIR/Dockerfile"
sed -i "s/__POSTGRESQL_DEV_PKG__/$POSTGRESQL_DEV_PKG/g" "$TEMP_BUILD_DIR/Dockerfile"
sed -i "s/__SQLITE_DEV_PKG__/$SQLITE_DEV_PKG/g" "$TEMP_BUILD_DIR/Dockerfile"
sed -i "s/__LIBDW_DEV_PKG__/$LIBDW_DEV_PKG/g" "$TEMP_BUILD_DIR/Dockerfile"

# Replace placeholders in build script
sed -i "s/__CMAKE_YAML_OPTION__/$CMAKE_YAML_OPTION/g" "$TEMP_BUILD_DIR/build_script.sh"
sed -i "s/__CMAKE_MYSQL_OPTION__/$CMAKE_MYSQL_OPTION/g" "$TEMP_BUILD_DIR/build_script.sh"
sed -i "s/__CMAKE_POSTGRESQL_OPTION__/$CMAKE_POSTGRESQL_OPTION/g" "$TEMP_BUILD_DIR/build_script.sh"
sed -i "s/__CMAKE_SQLITE_OPTION__/$CMAKE_SQLITE_OPTION/g" "$TEMP_BUILD_DIR/build_script.sh"
sed -i "s/__CMAKE_DW_OPTION__/$CMAKE_DW_OPTION/g" "$TEMP_BUILD_DIR/build_script.sh"
sed -i "s/__DEB_DEPENDENCIES__/$DEB_DEPENDENCIES/g" "$TEMP_BUILD_DIR/build_script.sh"

# Replace placeholders for debian control file
sed -i "s/__MYSQL_CONTROL_DEP__/$MYSQL_CONTROL_DEP/g" "$TEMP_BUILD_DIR/build_script.sh"
sed -i "s/__POSTGRESQL_CONTROL_DEP__/$POSTGRESQL_CONTROL_DEP/g" "$TEMP_BUILD_DIR/build_script.sh"
sed -i "s/__SQLITE_CONTROL_DEP__/$SQLITE_CONTROL_DEP/g" "$TEMP_BUILD_DIR/build_script.sh"
sed -i "s/__LIBDW_CONTROL_DEP__/$LIBDW_CONTROL_DEP/g" "$TEMP_BUILD_DIR/build_script.sh"
sed -i "s/__USE_MYSQL__/$USE_MYSQL/g" "$TEMP_BUILD_DIR/build_script.sh"
sed -i "s/__USE_POSTGRESQL__/$USE_POSTGRESQL/g" "$TEMP_BUILD_DIR/build_script.sh"
sed -i "s/__USE_SQLITE__/$USE_SQLITE/g" "$TEMP_BUILD_DIR/build_script.sh"
sed -i "s/__USE_CPP_YAML__/$USE_CPP_YAML/g" "$TEMP_BUILD_DIR/build_script.sh"
sed -i "s/__USE_DW__/$USE_DW/g" "$TEMP_BUILD_DIR/build_script.sh"
sed -i "s/__BUILD_TYPE__/$BUILD_TYPE/g" "$TEMP_BUILD_DIR/build_script.sh"
sed -i "s/__BUILD_EXAMPLES__/$BUILD_EXAMPLES/g" "$TEMP_BUILD_DIR/build_script.sh"
# Pass the build flags to the build script
sed -i "s/__BUILD_FLAGS__/$BUILD_FLAGS/g" "$TEMP_BUILD_DIR/build_script.sh"
# Extract distro name and version for the filename
DISTRO_NAME=$(echo "$DISTRO" | cut -d':' -f1)
DISTRO_VERSION=$(echo "$DISTRO" | cut -d':' -f2 | tr '.' '-')

# Replace placeholders in build script
sed -i "s/__TIMESTAMP__/$(date +"%Y-%m-%d-%H-%M")/g" "$TEMP_BUILD_DIR/build_script.sh"
sed -i "s/__DISTRO_NAME__/$DISTRO_NAME/g" "$TEMP_BUILD_DIR/build_script.sh"
sed -i "s/__DISTRO_VERSION__/$DISTRO_VERSION/g" "$TEMP_BUILD_DIR/build_script.sh"

# Create a directory for the build script in the Docker container
mkdir -p "$TEMP_BUILD_DIR/build"
cp "$TEMP_BUILD_DIR/build_script.sh" "$TEMP_BUILD_DIR/build/"

# Copy the cpp_dbc directory to the temporary build directory
mkdir -p "$TEMP_BUILD_DIR/libs"
cp -r "$(dirname "$0")" "$TEMP_BUILD_DIR/libs/"

# Copy the CHANGELOG.md from the root directory
cp -v "$(pwd)/CHANGELOG.md" "$TEMP_BUILD_DIR/"

# Create a directory for output
mkdir -p "$(pwd)/build"

# Build the Docker image
echo "Building Docker image for $DISTRO..."
DOCKER_BUILDKIT=1 docker build -t cpp_dbc_build:$DISTRO_DIR "$TEMP_BUILD_DIR"

# Run the Docker container to build the .deb package
echo "Building .deb package for cpp_dbc..."
docker run --rm -v "$(pwd)/build:/output:Z" cpp_dbc_build:$DISTRO_DIR

# Clean up temporary directory
rm -rf "$TEMP_BUILD_DIR"

echo "Build completed successfully!"
echo "The .deb package can be found in the build directory."