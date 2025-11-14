#!/bin/bash
# build.dist.sh — Script for building a Docker container from the C++ demo application

set -e

echo "Generating Dockerfile for C++ demo application..."

# 1. Load variables from .dist_build
if [ -f .dist_build ]; then
    source .dist_build
else
    echo "Error: .dist_build file not found."
    exit 1
fi

# 2. Default values
Container_Group="${Container_Group:-cpp_dbc}"
Container_User="${Container_User:-cpp_dbc}"
Container_Group_Id="${Container_Group_Id:-1000}"
Container_User_Id="${Container_User_Id:-1000}"

# 3. Generate date/time components
YYYY=$(date "+%Y")
YY=$(date "+%y")
MM=$(date "+%m")
DD=$(date "+%d")
HH=$(date "+%H")
MIN=$(date "+%M")
SS=$(date "+%S")
Z=$(date "+%z")
TIMESTAMP="${YYYY}-${MM}-${DD}-${HH}-${MIN}-${SS}_${Z}"

export YYYY YY MM DD HH MIN SS Z TIMESTAMP

echo "Processing date/time placeholders..."
echo "YYYY: $YYYY, YY: $YY, MM: $MM, DD: $DD"
echo "HH: $HH, MIN: $MIN, SS: $SS, Z: $Z"
echo "TIMESTAMP: $TIMESTAMP"

# 4. Create temporary files with expanded values
echo "Creating temporary files with expanded values..."

# Process .dist_build to .build_env.temp
# Handle the ambiguity between ${MM} for month and minutes
# First, replace the timestamp pattern with a temporary placeholder
cat .dist_build | sed \
    -e "s/\${YYYY}-\${MM}-\${DD}-\${HH}-\${MM}-\${SS}_\${Z}/$TIMESTAMP/g" \
    > .build_env.temp.1

# Then replace individual placeholders
cat .build_env.temp.1 | sed \
    -e "s/\${YYYY}/$YYYY/g" \
    -e "s/\${YY}/$YY/g" \
    -e "s/\${MM}/$MM/g" \
    -e "s/\${DD}/$DD/g" \
    -e "s/\${HH}/$HH/g" \
    -e "s/\${SS}/$SS/g" \
    -e "s/\${Z}/$Z/g" \
    > .build_env.temp.2

# Export variables for envsubst
export Container_Bin_Name="cpp_dbc"
# Ensure Container_Bin_Path doesn't have a trailing slash to avoid double slashes
export Container_Bin_Path="/usr/local/bin/cpp_dbc"

# Expand remaining variables
cat .build_env.temp.2 | envsubst > .build_env.temp

# Clean up temporary files
rm .build_env.temp.1 .build_env.temp.2

# Process .env_dist to .env_dist.temp
# Handle the ambiguity between ${MM} for month and minutes
# First, replace the timestamp pattern with a temporary placeholder
cat .env_dist | sed \
    -e "s/\${YYYY}-\${MM}-\${DD}-\${HH}-\${MM}-\${SS}_\${Z}/$TIMESTAMP/g" \
    > .env_dist.temp.1

# Then replace individual placeholders
cat .env_dist.temp.1 | sed \
    -e "s/\${YYYY}/$YYYY/g" \
    -e "s/\${YY}/$YY/g" \
    -e "s/\${MM}/$MM/g" \
    -e "s/\${DD}/$DD/g" \
    -e "s/\${HH}/$HH/g" \
    -e "s/\${SS}/$SS/g" \
    -e "s/\${Z}/$Z/g" \
    > .env_dist.temp.2

# Expand remaining variables
cat .env_dist.temp.2 | envsubst > .env_dist.temp

# Clean up temporary files
rm .env_dist.temp.1 .env_dist.temp.2

# Source the temporary files to get expanded values
source .build_env.temp
export Container_Tags Container_App_Folders

# 5. Export variables for envsubst
export Container_Bin_Name Container_Bin_Path

# 6. Process paths and tags
PROCESSED_CONTAINER_BIN_PATH=$(echo "$Container_Bin_Path" | envsubst)
PROCESSED_CONTAINER_TAGS=$(echo "$Container_Tags" | envsubst)

# Ensure Container_Name is not empty
if [ -z "$Container_Name" ]; then
    Container_Name="cpp_dbc"
    echo "Warning: Container_Name not set in .dist_build, using default: $Container_Name"
fi

# Ensure we have at least one tag
if [ -z "$PROCESSED_CONTAINER_TAGS" ]; then
    PROCESSED_CONTAINER_TAGS="latest"
    echo "Warning: No tags defined, using default tag: latest"
fi

# Process app folders
PROCESSED_CONTAINER_APP_FOLDERS_RAW=$(echo "$Container_App_Folders" | envsubst)
IFS=';' read -ra PROCESSED_APP_FOLDERS <<< "$PROCESSED_CONTAINER_APP_FOLDERS_RAW"

echo "Container Name: $Container_Name"
echo "Binary Path: $PROCESSED_CONTAINER_BIN_PATH"
echo "Tags: $PROCESSED_CONTAINER_TAGS"
echo "User: $Container_User ($Container_User_Id), Group: $Container_Group ($Container_Group_Id)"
echo "Additional App Folders:"
printf '  - %s\n' "${PROCESSED_APP_FOLDERS[@]}"

# 7. Process .env_dist.temp
echo "Processing .env_dist.temp file..."
ENV_VARS=""
if [ -f .env_dist.temp ]; then
    while IFS= read -r line || [[ -n "$line" ]]; do
        [[ -z "$line" || "$line" =~ ^# ]] && continue
        key=${line%%=*}
        raw_value=${line#*=}
        expanded_value=$(echo "$raw_value" | envsubst)
        ENV_VARS+="ENV $key=$expanded_value"$'\n'
    done < .env_dist.temp
else
    echo "Warning: .env_dist.temp file not found."
fi

# 8. Parse command line arguments to set build options
USE_MYSQL=ON
USE_POSTGRESQL=OFF
USE_SQLITE=OFF
USE_CPP_YAML=OFF
BUILD_TYPE=Debug
BUILD_TESTS=OFF
BUILD_EXAMPLES=OFF
DEBUG_CONNECTION_POOL=OFF
DEBUG_TRANSACTION_MANAGER=OFF
DEBUG_SQLITE=OFF
BACKWARD_HAS_DW=ON

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
        --clean)
        # Clean build directories before building
        echo "Cleaning build directories..."
        rm -rf build
        rm -rf libs/cpp_dbc/build
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
        --debug-pool)
        DEBUG_CONNECTION_POOL=ON
        ;;
        --debug-txmgr)
        DEBUG_TRANSACTION_MANAGER=ON
        ;;
        --debug-sqlite)
        DEBUG_SQLITE=ON
        ;;
        --debug-all)
        DEBUG_CONNECTION_POOL=ON
        DEBUG_TRANSACTION_MANAGER=ON
        DEBUG_SQLITE=ON
        ;;
        --dw-off)
        BACKWARD_HAS_DW=OFF
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
        echo "  --clean                Clean build directories before building"
        echo "  --release              Build in Release mode (default: Debug)"
        echo "  --test                 Build cpp_dbc tests"
        echo "  --examples             Build cpp_dbc examples"
        echo "  --debug-pool           Enable debug output for ConnectionPool"
        echo "  --debug-txmgr          Enable debug output for TransactionManager"
        echo "  --debug-sqlite         Enable debug output for SQLite driver"
        echo "  --debug-all            Enable all debug output"
        echo "  --dw-off               Disable libdw support for stack traces"
        echo "  --help                 Show this help message"
        exit 0
        ;;
    esac
done

# Build command with options
BUILD_CMD="./build.sh"

if [ "$USE_MYSQL" = "ON" ]; then
    BUILD_CMD="$BUILD_CMD --mysql"
else
    BUILD_CMD="$BUILD_CMD --mysql-off"
fi

if [ "$USE_POSTGRESQL" = "ON" ]; then
    BUILD_CMD="$BUILD_CMD --postgres"
fi

if [ "$USE_SQLITE" = "ON" ]; then
    BUILD_CMD="$BUILD_CMD --sqlite"
fi

if [ "$USE_CPP_YAML" = "ON" ]; then
    BUILD_CMD="$BUILD_CMD --yaml"
fi

if [ "$BUILD_TYPE" = "Release" ]; then
    BUILD_CMD="$BUILD_CMD --release"
fi

if [ "$BUILD_TESTS" = "ON" ]; then
    BUILD_CMD="$BUILD_CMD --test"
fi

if [ "$BUILD_EXAMPLES" = "ON" ]; then
    BUILD_CMD="$BUILD_CMD --examples"
fi

# Pass debug options
if [ "$DEBUG_CONNECTION_POOL" = "ON" ]; then
    BUILD_CMD="$BUILD_CMD --debug-pool"
fi

if [ "$DEBUG_TRANSACTION_MANAGER" = "ON" ]; then
    BUILD_CMD="$BUILD_CMD --debug-txmgr"
fi

if [ "$DEBUG_SQLITE" = "ON" ]; then
    BUILD_CMD="$BUILD_CMD --debug-sqlite"
fi

if [ "$BACKWARD_HAS_DW" = "OFF" ]; then
    BUILD_CMD="$BUILD_CMD --dw-off"
fi

# 9. Build locally with Conan + CMake
echo "Building project with Conan and CMake..."
echo "Running: $BUILD_CMD"
$BUILD_CMD

# 10. Detect dependencies
echo "Detecting shared library dependencies..."
# Start with basic packages that are always needed
PACKAGES="ca-certificates"
CONTAINER_DEPS=""

# Map of known libraries to their packages
declare -A LIB_TO_PKG_MAP
LIB_TO_PKG_MAP["/lib/x86_64-linux-gnu/libasan.so.6"]="libasan6"
LIB_TO_PKG_MAP["/lib/x86_64-linux-gnu/libstdc++.so.6"]="libstdc++6"
LIB_TO_PKG_MAP["/lib/x86_64-linux-gnu/libubsan.so.1"]="libubsan1"
LIB_TO_PKG_MAP["/lib/x86_64-linux-gnu/libm.so.6"]="libc6"  # libm is part of libc6
LIB_TO_PKG_MAP["/lib/x86_64-linux-gnu/libresolv.so.2"]="libc6"  # libresolv is part of libc6
LIB_TO_PKG_MAP["/lib/x86_64-linux-gnu/libcrypto.so.3"]="libssl3"
LIB_TO_PKG_MAP["/lib/x86_64-linux-gnu/libssl.so.3"]="libssl3"
LIB_TO_PKG_MAP["/lib/x86_64-linux-gnu/libmysqlclient.so.21"]="libmysqlclient21"
LIB_TO_PKG_MAP["/lib/x86_64-linux-gnu/libpq.so.5"]="libpq5"
LIB_TO_PKG_MAP["/lib/x86_64-linux-gnu/libsasl2.so.2"]="libsasl2-2"
LIB_TO_PKG_MAP["/lib/x86_64-linux-gnu/libgnutls.so.30"]="libgnutls30"
LIB_TO_PKG_MAP["/lib/x86_64-linux-gnu/libgmp.so.10"]="libgmp10"
LIB_TO_PKG_MAP["/lib/x86_64-linux-gnu/libffi.so.8"]="libffi8"
LIB_TO_PKG_MAP["/lib/x86_64-linux-gnu/libnettle.so.8"]="libnettle8"
LIB_TO_PKG_MAP["/lib/x86_64-linux-gnu/libhogweed.so.6"]="libhogweed6"
LIB_TO_PKG_MAP["/lib/x86_64-linux-gnu/libidn2.so.0"]="libidn2-0"
LIB_TO_PKG_MAP["/lib/x86_64-linux-gnu/libp11-kit.so.0"]="libp11-kit0"
LIB_TO_PKG_MAP["/lib/x86_64-linux-gnu/libtasn1.so.6"]="libtasn1-6"
LIB_TO_PKG_MAP["/lib/x86_64-linux-gnu/libunistring.so.2"]="libunistring2"
LIB_TO_PKG_MAP["/lib/x86_64-linux-gnu/libzstd.so.1"]="libzstd1"
LIB_TO_PKG_MAP["/lib/x86_64-linux-gnu/libkrb5.so.3"]="libkrb5-3"
LIB_TO_PKG_MAP["/lib/x86_64-linux-gnu/libk5crypto.so.3"]="libkrb5-3"
LIB_TO_PKG_MAP["/lib/x86_64-linux-gnu/libkrb5support.so.0"]="libkrb5support0"
LIB_TO_PKG_MAP["/lib/x86_64-linux-gnu/libgssapi_krb5.so.2"]="libgssapi-krb5-2"
LIB_TO_PKG_MAP["/lib/x86_64-linux-gnu/liblber-2.5.so.0"]="libldap-2.5-0"
LIB_TO_PKG_MAP["/lib/x86_64-linux-gnu/libldap-2.5.so.0"]="libldap-2.5-0"
LIB_TO_PKG_MAP["/lib/x86_64-linux-gnu/libsqlite3.so.0"]="libsqlite3-0"
LIB_TO_PKG_MAP["/lib/x86_64-linux-gnu/libdw.so.1"]="libdw1"
LIB_TO_PKG_MAP["/lib/x86_64-linux-gnu/libelf.so.1"]="libelf1"
LIB_TO_PKG_MAP["/lib/x86_64-linux-gnu/libz.so.1"]="zlib1g"
LIB_TO_PKG_MAP["/lib/x86_64-linux-gnu/liblzma.so.5"]="liblzma5"
LIB_TO_PKG_MAP["/lib/x86_64-linux-gnu/libbz2.so.1.0"]="libbz2-1.0"

# Function to add a package to our list
add_package() {
    local pkg="$1"
    
    # Check if package is already in our list
    if [[ ! " $PACKAGES " =~ " $pkg " ]]; then
        echo "Adding package: $pkg"
        PACKAGES="$PACKAGES $pkg"
        
        # Get package version
        PKG_VERSION=$(dpkg-query -W -f='${Version}' "$pkg" 2>/dev/null)
        if [ -n "$PKG_VERSION" ]; then
            if [ -n "$CONTAINER_DEPS" ]; then
                CONTAINER_DEPS="$CONTAINER_DEPS;"
            fi
            CONTAINER_DEPS="${CONTAINER_DEPS}${pkg}:${PKG_VERSION}"
        else
            echo "Warning: Could not determine version for package: $pkg"
        fi
    fi
}

# Use ldd to detect shared library dependencies
if [ -f "build/$Container_Bin_Name" ]; then
    echo "Analyzing executable: build/$Container_Bin_Name"
    
    # Get all shared libraries used by the executable
    LIBS=$(ldd "build/$Container_Bin_Name" | grep "=>" | awk '{print $3}' | sort | uniq)
    
    echo "Found shared libraries:"
    echo "$LIBS"
    
    # Find which Debian packages provide these libraries
    for lib in $LIBS; do
        # Skip libraries that don't exist (like linux-vdso.so.1)
        if [ ! -f "$lib" ]; then
            continue
        fi
        
        # Check if we have this library in our known map
        if [ -n "${LIB_TO_PKG_MAP[$lib]}" ]; then
            pkg="${LIB_TO_PKG_MAP[$lib]}"
            echo "Library $lib provided by package (from map): $pkg"
            add_package "$pkg"
        else
            # Find the package that provides this library
            PKG=$(dpkg -S "$lib" 2>/dev/null | cut -d: -f1 | sort | uniq)
            
            if [ -n "$PKG" ]; then
                for pkg in $PKG; do
                    echo "Library $lib provided by package: $pkg"
                    add_package "$pkg"
                done
            else
                echo "Warning: Could not determine package for library: $lib"
                
                # Try to guess based on filename
                base_name=$(basename "$lib")
                if [[ "$base_name" =~ ^lib([^.]+)\.so\. ]]; then
                    lib_name="${BASH_REMATCH[1]}"
                    
                    # Special case for libsasl2.so.2
                    if [[ "$lib_name" == "sasl2" ]]; then
                        potential_pkg="libsasl2-2"
                    # Special case for other common libraries with different naming patterns
                    elif [[ "$lib_name" == "tasn1" ]]; then
                        potential_pkg="libtasn1-6"
                    elif [[ "$lib_name" == "idn2" ]]; then
                        potential_pkg="libidn2-0"
                    else
                        potential_pkg="lib${lib_name}"
                    fi
                    
                    # Check if this package exists
                    if dpkg -l "$potential_pkg" &>/dev/null; then
                        echo "Guessing package for $lib: $potential_pkg"
                        add_package "$potential_pkg"
                    else
                        # Try with a number suffix for common libraries
                        for suffix in {0..9}; do
                            test_pkg="${potential_pkg}${suffix}"
                            if dpkg -l "$test_pkg" &>/dev/null; then
                                echo "Guessing package for $lib: $test_pkg"
                                add_package "$test_pkg"
                                break
                            fi
                        done
                        
                        if [[ ! " $PACKAGES " =~ " $potential_pkg " ]] && [[ ! " $PACKAGES " =~ " ${potential_pkg}[0-9] " ]]; then
                            echo "Could not guess package for $lib"
                        fi
                    fi
                fi
            fi
        fi
    done
    
    # Always add these critical packages for C++ applications
    add_package "libc6"
    add_package "libgcc-s1"
    add_package "libstdc++6"
else
    echo "Warning: Executable not found at build/$Container_Bin_Name"
    echo "Using default dependencies: libc6 libgcc-s1 libstdc++6"
    add_package "libc6"
    add_package "libgcc-s1"
    add_package "libstdc++6"
fi

echo "Detected packages: $PACKAGES"
echo "Container dependencies: $CONTAINER_DEPS"

# 11. Generate dockerfile.dist
echo "Generating dockerfile.dist..."
cat > dockerfile.dist <<EOF
# Generated by build.dist.sh on $(date)
FROM ubuntu:22.04

LABEL description="$Container_Description"
LABEL maintainer="$Container_Mantainer"
LABEL tags="$PROCESSED_CONTAINER_TAGS"
LABEL path="$PROCESSED_CONTAINER_BIN_PATH"

RUN apt-get update && \\
    apt-get install -y --no-install-recommends $PACKAGES && \\
    apt-get clean && rm -rf /var/lib/apt/lists/*

$ENV_VARS
ENV APP_DEPENDENCIES="$CONTAINER_DEPS"

RUN groupadd -g $Container_Group_Id $Container_Group && \\
    useradd -m -u $Container_User_Id -g $Container_Group -s /bin/bash $Container_User && \\
    mkdir -p $PROCESSED_CONTAINER_BIN_PATH && \\
EOF

# Add app folders
for folder in "${PROCESSED_APP_FOLDERS[@]}"; do
  echo "    mkdir -p \"$folder\" && chown $Container_User:$Container_Group \"$folder\" && \\" >> dockerfile.dist
done

# Copy binary and set permissions
cat >> dockerfile.dist <<EOF
    chown -R $Container_User:$Container_Group $PROCESSED_CONTAINER_BIN_PATH

COPY build/$Container_Bin_Name $PROCESSED_CONTAINER_BIN_PATH/$Container_Bin_Name
RUN chown $Container_User:$Container_Group $PROCESSED_CONTAINER_BIN_PATH/$Container_Bin_Name && \\
    chmod 755 $PROCESSED_CONTAINER_BIN_PATH/$Container_Bin_Name

USER $Container_User
WORKDIR $PROCESSED_CONTAINER_BIN_PATH
ENTRYPOINT ["$PROCESSED_CONTAINER_BIN_PATH/$Container_Bin_Name"]
EOF

# 12. Build and tag
echo "Dockerfile generated: dockerfile.dist"
docker build -t "$Container_Name" -f dockerfile.dist .

# Use semicolons as the delimiter for Container_Tags
IFS=';' read -ra TAG_ARRAY <<< "$PROCESSED_CONTAINER_TAGS"
for tag in "${TAG_ARRAY[@]}"; do
    clean_tag=$(echo "$tag" | xargs)
    if [ -n "$clean_tag" ]; then
        echo "Tagging image: $Container_Name:$clean_tag"
        docker tag "$Container_Name" "$Container_Name:$clean_tag"
    fi
done

echo "Docker image '$Container_Name' built successfully!"

# Print build options used
echo ""
echo "Build options used:"
echo "  MySQL: $USE_MYSQL"
echo "  PostgreSQL: $USE_POSTGRESQL"
echo "  SQLite: $USE_SQLITE"
echo "  YAML support: $USE_CPP_YAML"
echo "  Build type: $BUILD_TYPE"
echo "  Build tests: $BUILD_TESTS"
echo "  Build examples: $BUILD_EXAMPLES"
echo "  Debug ConnectionPool: $DEBUG_CONNECTION_POOL"
echo "  Debug TransactionManager: $DEBUG_TRANSACTION_MANAGER"
echo "  Debug SQLite: $DEBUG_SQLITE"
echo "  libdw support: $BACKWARD_HAS_DW"

# Debug information
echo ""
echo "Temporary files created for debugging:"
echo "- .build_env.temp: Expanded values from .dist_build"
echo "- .env_dist.temp: Expanded values from .env_dist"
echo "These files are kept for debugging purposes."
