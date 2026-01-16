#!/bin/bash
set -e

# Default values
DISTROS="ubuntu:24.04"
BUILD_OPTIONS=""
VERSION=""

# Function to display usage information
function show_usage {
    echo "Usage: $0 [OPTIONS]"
    echo "Build .deb/.rpm packages for cpp_dbc library in Docker containers."
    echo ""
    echo "Options:"
    echo "  --distro=DISTROS    Specify the distributions to build for, separated by '+' symbol."
    echo "                      Supported values: debian:12, debian:13, ubuntu:22.04, ubuntu:24.04,"
    echo "                                        fedora:42, fedora:43"
    echo "                      Default: ubuntu:24.04"
    echo "                      Example: --distro=ubuntu:24.04+fedora:42+debian:12"
    echo "  --build=OPTIONS     Comma-separated build options."
    echo "                      Supported values: yaml, mysql, postgres, sqlite, firebird, mongodb,"
    echo "                                        debug, release, dw, examples, thread-safe-off,"
    echo "                                        debug-pool, debug-txmgr, debug-sqlite, debug-firebird,"
    echo "                                        debug-mongodb, debug-all"
    echo "                      Default: yaml,mysql,postgres,sqlite,debug,dw"
    echo "  --version=VERSION   Specify a version string to use instead of timestamp."
    echo "                      Example: --version=1.0.1 or --version=2-0-1"
    echo "  --help              Display this help message and exit"
    echo ""
    echo "Examples:"
    echo "  $0 --distro=ubuntu:24.04+debian:12 --build=yaml,mysql,postgres,sqlite,firebird,debug,dw,examples"
    echo "  $0 --distro=ubuntu:24.04+ubuntu:22.04 --build=mysql,postgres,sqlite,debug,dw,yaml --version=1.0.1"
    exit 1
}

# Parse command line arguments
for arg in "$@"; do
    case $arg in
        --distro=*)
            DISTROS="${arg#*=}"
            ;;
        --build=*)
            BUILD_OPTIONS="${arg#*=}"
            ;;
        --version=*)
            VERSION="${arg#*=}"
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
if [ -z "$DISTROS" ]; then
    DISTROS="ubuntu:24.04"
fi

# Generate a single timestamp for all builds or use the provided version
if [ -z "$VERSION" ]; then
    TIMESTAMP=$(date +"%Y-%m-%d-%H-%M")
else
    TIMESTAMP="$VERSION"
fi

# Function to get package dependencies for a distro
function get_distro_packages() {
    local distro=$1
    
    case $distro in
        debian:12|debian:13|ubuntu:22.04|ubuntu:24.04)
            MYSQL_DEV_PKG="default-libmysqlclient-dev"
            POSTGRESQL_DEV_PKG="libpq-dev"
            SQLITE_DEV_PKG="libsqlite3-dev"
            FIREBIRD_DEV_PKG="firebird-dev"
            MONGODB_DEV_PKG="libmongoc-dev"
            SCYLLA_DEV_PKG="libuv1-dev, libssl-dev, zlib1g-dev"
            LIBDW_DEV_PKG="libdw-dev"
            ;;
        fedora:42|fedora:43)
            MYSQL_DEV_PKG="mysql-devel"
            POSTGRESQL_DEV_PKG="libpq-devel"
            SQLITE_DEV_PKG="sqlite-devel"
            FIREBIRD_DEV_PKG="firebird-devel"
            MONGODB_DEV_PKG="mongo-c-driver-devel"
            SCYLLA_DEV_PKG="libuv-devel, openssl-devel, zlib-devel"
            LIBDW_DEV_PKG="elfutils-devel"
            ;;
        *)
            echo "Unsupported distribution: $distro"
            show_usage
            ;;
    esac
}

# Function to convert distro format to directory name
function get_distro_dir() {
    local distro=$1
    
    case $distro in
        debian:12)
            echo "debian_12"
            ;;
        debian:13)
            echo "debian_13"
            ;;
        ubuntu:22.04)
            echo "ubuntu_22_04"
            ;;
        ubuntu:24.04)
            echo "ubuntu_24_04"
            ;;
        fedora:42)
            echo "fedora_42"
            ;;
        fedora:43)
            echo "fedora_43"
            ;;
        *)
            echo "Unsupported distribution: $distro"
            show_usage
            ;;
    esac
}

# Set up build options
CMAKE_YAML_OPTION="-DCPP_DBC_WITH_YAML=OFF"
CMAKE_MYSQL_OPTION="-DCPP_DBC_WITH_MYSQL=OFF"
CMAKE_POSTGRESQL_OPTION="-DCPP_DBC_WITH_POSTGRESQL=OFF"
CMAKE_SQLITE_OPTION="-DCPP_DBC_WITH_SQLITE=OFF"
CMAKE_FIREBIRD_OPTION="-DCPP_DBC_WITH_FIREBIRD=OFF"
CMAKE_MONGODB_OPTION="-DCPP_DBC_WITH_MONGODB=OFF"
CMAKE_SCYLLA_OPTION="-DCPP_DBC_WITH_SCYLLA=OFF"
CMAKE_DW_OPTION="-DBACKWARD_HAS_DW=OFF"

DEB_DEPENDENCIES="libc6"

# Process build options
MYSQL_CONTROL_DEP=""
POSTGRESQL_CONTROL_DEP=""
SQLITE_CONTROL_DEP=""
FIREBIRD_CONTROL_DEP=""
MONGODB_CONTROL_DEP=""
SCYLLA_CONTROL_DEP=""
LIBDW_CONTROL_DEP=""
USE_MYSQL="OFF"
USE_POSTGRESQL="OFF"
USE_SQLITE="OFF"
USE_FIREBIRD="OFF"
USE_MONGODB="OFF"
USE_SCYLLA="OFF"
USE_CPP_YAML="OFF"
USE_DW="OFF"
BUILD_TYPE="Debug"
BUILD_EXAMPLES="OFF"
DB_DRIVER_THREAD_SAFE="ON"
DEBUG_CONNECTION_POOL="OFF"
DEBUG_TRANSACTION_MANAGER="OFF"
DEBUG_SQLITE="OFF"
DEBUG_FIREBIRD="OFF"
DEBUG_MONGODB="OFF"
DEBUG_ALL="OFF"
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
        firebird)
            CMAKE_FIREBIRD_OPTION="-DCPP_DBC_WITH_FIREBIRD=ON"
            DEB_DEPENDENCIES="$DEB_DEPENDENCIES, $FIREBIRD_DEV_PKG"
            FIREBIRD_CONTROL_DEP=", $FIREBIRD_DEV_PKG"
            USE_FIREBIRD="ON"
            BUILD_FLAGS="$BUILD_FLAGS --firebird"
            ;;
        mongodb)
            CMAKE_MONGODB_OPTION="-DCPP_DBC_WITH_MONGODB=ON"
            DEB_DEPENDENCIES="$DEB_DEPENDENCIES, $MONGODB_DEV_PKG"
            MONGODB_CONTROL_DEP=", $MONGODB_DEV_PKG"
            USE_MONGODB="ON"
            BUILD_FLAGS="$BUILD_FLAGS --mongodb"
            ;;
        scylla)
            CMAKE_SCYLLA_OPTION="-DCPP_DBC_WITH_SCYLLA=ON"
            DEB_DEPENDENCIES="$DEB_DEPENDENCIES, $SCYLLA_DEV_PKG"
            SCYLLA_CONTROL_DEP=", $SCYLLA_DEV_PKG"
            USE_SCYLLA="ON"
            BUILD_FLAGS="$BUILD_FLAGS --scylla"
            ;;
        debug)
            BUILD_TYPE="Debug"
            BUILD_FLAGS="$BUILD_FLAGS --debug"
            ;;
        release)
            BUILD_TYPE="Release"
            BUILD_FLAGS="$BUILD_FLAGS --release"
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
        thread-safe-off)
            DB_DRIVER_THREAD_SAFE="OFF"
            BUILD_FLAGS="$BUILD_FLAGS --db-driver-thread-safe-off"
            ;;
        debug-pool)
            DEBUG_CONNECTION_POOL="ON"
            BUILD_FLAGS="$BUILD_FLAGS --debug-pool"
            ;;
        debug-txmgr)
            DEBUG_TRANSACTION_MANAGER="ON"
            BUILD_FLAGS="$BUILD_FLAGS --debug-txmgr"
            ;;
        debug-sqlite)
            DEBUG_SQLITE="ON"
            BUILD_FLAGS="$BUILD_FLAGS --debug-sqlite"
            ;;
        debug-firebird)
            DEBUG_FIREBIRD="ON"
            BUILD_FLAGS="$BUILD_FLAGS --debug-firebird"
            ;;
        debug-mongodb)
            DEBUG_MONGODB="ON"
            BUILD_FLAGS="$BUILD_FLAGS --debug-mongodb"
            ;;
        debug-all)
            DEBUG_CONNECTION_POOL="ON"
            DEBUG_TRANSACTION_MANAGER="ON"
            DEBUG_SQLITE="ON"
            DEBUG_FIREBIRD="ON"
            DEBUG_MONGODB="ON"
            DEBUG_ALL="ON"
            BUILD_FLAGS="$BUILD_FLAGS --debug-all"
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

# Process each distribution
IFS='+' read -ra DISTRO_LIST <<< "$DISTROS"
for DISTRO in "${DISTRO_LIST[@]}"; do
    echo "Processing distribution: $DISTRO"
    
    # Get package dependencies for this distro
    get_distro_packages "$DISTRO"
    
    # Get directory name for this distro
    DISTRO_DIR=$(get_distro_dir "$DISTRO")
    
    # Create temporary build directory
    TEMP_BUILD_DIR=$(mktemp -d)
    
    # Copy Dockerfile and build script to temporary directory
    cp -r "$(dirname "$0")/distros/$DISTRO_DIR/"* "$TEMP_BUILD_DIR/"
    
    # Replace placeholders in Dockerfile
    sed -i "s/__MYSQL_DEV_PKG__/$MYSQL_DEV_PKG/g" "$TEMP_BUILD_DIR/Dockerfile"
    sed -i "s/__POSTGRESQL_DEV_PKG__/$POSTGRESQL_DEV_PKG/g" "$TEMP_BUILD_DIR/Dockerfile"
    sed -i "s/__SQLITE_DEV_PKG__/$SQLITE_DEV_PKG/g" "$TEMP_BUILD_DIR/Dockerfile"
    sed -i "s/__FIREBIRD_DEV_PKG__/$FIREBIRD_DEV_PKG/g" "$TEMP_BUILD_DIR/Dockerfile"
    sed -i "s/__MONGODB_DEV_PKG__/$MONGODB_DEV_PKG/g" "$TEMP_BUILD_DIR/Dockerfile"
    sed -i "s/__LIBDW_DEV_PKG__/$LIBDW_DEV_PKG/g" "$TEMP_BUILD_DIR/Dockerfile"
    
    # Replace placeholders in build script
    sed -i "s/__CMAKE_YAML_OPTION__/$CMAKE_YAML_OPTION/g" "$TEMP_BUILD_DIR/build_script.sh"
    sed -i "s/__CMAKE_MYSQL_OPTION__/$CMAKE_MYSQL_OPTION/g" "$TEMP_BUILD_DIR/build_script.sh"
    sed -i "s/__CMAKE_POSTGRESQL_OPTION__/$CMAKE_POSTGRESQL_OPTION/g" "$TEMP_BUILD_DIR/build_script.sh"
    sed -i "s/__CMAKE_SQLITE_OPTION__/$CMAKE_SQLITE_OPTION/g" "$TEMP_BUILD_DIR/build_script.sh"
    sed -i "s/__CMAKE_FIREBIRD_OPTION__/$CMAKE_FIREBIRD_OPTION/g" "$TEMP_BUILD_DIR/build_script.sh"
    sed -i "s/__CMAKE_MONGODB_OPTION__/$CMAKE_MONGODB_OPTION/g" "$TEMP_BUILD_DIR/build_script.sh"
    sed -i "s/__CMAKE_DW_OPTION__/$CMAKE_DW_OPTION/g" "$TEMP_BUILD_DIR/build_script.sh"
    sed -i "s/__DEB_DEPENDENCIES__/$DEB_DEPENDENCIES/g" "$TEMP_BUILD_DIR/build_script.sh"
    
    # Replace placeholders for debian control file
    sed -i "s/__MYSQL_CONTROL_DEP__/$MYSQL_CONTROL_DEP/g" "$TEMP_BUILD_DIR/build_script.sh"
    sed -i "s/__POSTGRESQL_CONTROL_DEP__/$POSTGRESQL_CONTROL_DEP/g" "$TEMP_BUILD_DIR/build_script.sh"
    sed -i "s/__SQLITE_CONTROL_DEP__/$SQLITE_CONTROL_DEP/g" "$TEMP_BUILD_DIR/build_script.sh"
    sed -i "s/__FIREBIRD_CONTROL_DEP__/$FIREBIRD_CONTROL_DEP/g" "$TEMP_BUILD_DIR/build_script.sh"
    sed -i "s/__MONGODB_CONTROL_DEP__/$MONGODB_CONTROL_DEP/g" "$TEMP_BUILD_DIR/build_script.sh"
    sed -i "s/__SCYLLA_CONTROL_DEP__/$SCYLLA_CONTROL_DEP/g" "$TEMP_BUILD_DIR/build_script.sh"
    sed -i "s/__LIBDW_CONTROL_DEP__/$LIBDW_CONTROL_DEP/g" "$TEMP_BUILD_DIR/build_script.sh"
    sed -i "s/__USE_MYSQL__/$USE_MYSQL/g" "$TEMP_BUILD_DIR/build_script.sh"
    sed -i "s/__USE_POSTGRESQL__/$USE_POSTGRESQL/g" "$TEMP_BUILD_DIR/build_script.sh"
    sed -i "s/__USE_SQLITE__/$USE_SQLITE/g" "$TEMP_BUILD_DIR/build_script.sh"
    sed -i "s/__USE_FIREBIRD__/$USE_FIREBIRD/g" "$TEMP_BUILD_DIR/build_script.sh"
    sed -i "s/__USE_MONGODB__/$USE_MONGODB/g" "$TEMP_BUILD_DIR/build_script.sh"
    sed -i "s/__USE_SCYLLA__/$USE_SCYLLA/g" "$TEMP_BUILD_DIR/build_script.sh"
    sed -i "s/__USE_CPP_YAML__/$USE_CPP_YAML/g" "$TEMP_BUILD_DIR/build_script.sh"
    sed -i "s/__USE_DW__/$USE_DW/g" "$TEMP_BUILD_DIR/build_script.sh"
    sed -i "s/__BUILD_TYPE__/$BUILD_TYPE/g" "$TEMP_BUILD_DIR/build_script.sh"
    sed -i "s/__BUILD_EXAMPLES__/$BUILD_EXAMPLES/g" "$TEMP_BUILD_DIR/build_script.sh"
    sed -i "s/__DB_DRIVER_THREAD_SAFE__/$DB_DRIVER_THREAD_SAFE/g" "$TEMP_BUILD_DIR/build_script.sh"
    sed -i "s/__DEBUG_CONNECTION_POOL__/$DEBUG_CONNECTION_POOL/g" "$TEMP_BUILD_DIR/build_script.sh"
    sed -i "s/__DEBUG_TRANSACTION_MANAGER__/$DEBUG_TRANSACTION_MANAGER/g" "$TEMP_BUILD_DIR/build_script.sh"
    sed -i "s/__DEBUG_SQLITE__/$DEBUG_SQLITE/g" "$TEMP_BUILD_DIR/build_script.sh"
    sed -i "s/__DEBUG_FIREBIRD__/$DEBUG_FIREBIRD/g" "$TEMP_BUILD_DIR/build_script.sh"
    sed -i "s/__DEBUG_MONGODB__/$DEBUG_MONGODB/g" "$TEMP_BUILD_DIR/build_script.sh"
    sed -i "s/__DEBUG_ALL__/$DEBUG_ALL/g" "$TEMP_BUILD_DIR/build_script.sh"
    # Pass the build flags to the build script
    sed -i "s/__BUILD_FLAGS__/$BUILD_FLAGS/g" "$TEMP_BUILD_DIR/build_script.sh"
    
    # Extract distro name and version for the filename
    DISTRO_NAME=$(echo "$DISTRO" | cut -d':' -f1)
    DISTRO_VERSION=$(echo "$DISTRO" | cut -d':' -f2 | tr '.' '-')
    
    # Replace placeholders in build script - use the same timestamp for all builds
    sed -i "s/__TIMESTAMP__/$TIMESTAMP/g" "$TEMP_BUILD_DIR/build_script.sh"
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
    
    # Run the Docker container to build the package
    if [[ "$DISTRO" == fedora:* ]]; then
        echo "Building .rpm package for cpp_dbc on $DISTRO..."
    else
        echo "Building .deb package for cpp_dbc on $DISTRO..."
    fi
    docker run --rm -v "$(pwd)/build:/output:Z" cpp_dbc_build:$DISTRO_DIR
    
    # Clean up temporary directory
    rm -rf "$TEMP_BUILD_DIR"
    
    echo "Build for $DISTRO completed successfully!"
done

echo "All builds completed successfully!"
echo "The packages can be found in the build directory."