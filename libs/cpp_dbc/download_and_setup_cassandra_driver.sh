#!/bin/bash
set -e

show_usage() {
    echo "Usage: $0 [--help|-h]"
    echo ""
    echo "Download and install the Cassandra C/C++ driver (version 2.17.1)."
    echo ""
    echo "This script will:"
    echo "  - Check if the driver is already installed"
    echo "  - Install build dependencies (cmake, make, g++, libuv, openssl, zlib)"
    echo "  - Download and build the driver from source"
    echo "  - Install to /usr/local"
    echo ""
    echo "Options:"
    echo "  --help, -h    Show this help message"
    echo ""
    echo "Note: This script requires root privileges or sudo."
}

# Handle --help argument
if [[ "$1" == "--help" ]] || [[ "$1" == "-h" ]]; then
    show_usage
    exit 0
fi

# Function to check if a command exists
command_exists() {
    command -v "$1" >/dev/null 2>&1
}

# Check if we are root
if [ "$(id -u)" -eq 0 ]; then
    SUDO=""
else
    if command_exists sudo; then
        SUDO="sudo"
    else
        echo "Error: This script requires root privileges or sudo."
        exit 1
    fi
fi

# Check if driver is already installed
echo "Checking for existing Cassandra driver installation..."

DRIVER_FOUND=0

# Check pkg-config
if command_exists pkg-config; then
    if pkg-config --exists cassandra; then
        echo "Cassandra driver found via pkg-config."
        DRIVER_FOUND=1
    elif pkg-config --exists cassandra_static; then
        echo "Cassandra static driver found via pkg-config."
        DRIVER_FOUND=1
    fi
fi

# Check headers
HEADER_FOUND=0
if [ -f "/usr/local/include/cassandra.h" ] || [ -f "/usr/include/cassandra.h" ]; then
    HEADER_FOUND=1
fi

# Check libraries
LIB_FOUND=0
# Check for .so, .so.2, .a
if ls /usr/local/lib/libcassandra.* 1> /dev/null 2>&1 || \
   ls /usr/lib/libcassandra.* 1> /dev/null 2>&1 || \
   ls /usr/lib/x86_64-linux-gnu/libcassandra.* 1> /dev/null 2>&1 || \
   ls /usr/local/lib64/libcassandra.* 1> /dev/null 2>&1 || \
   ls /usr/lib64/libcassandra.* 1> /dev/null 2>&1; then
    LIB_FOUND=1
fi

if [ $DRIVER_FOUND -eq 1 ] || ( [ $HEADER_FOUND -eq 1 ] && [ $LIB_FOUND -eq 1 ] ); then
    echo "Cassandra C/C++ driver appears to be already installed."
    exit 0
fi

echo "Cassandra C/C++ driver not found or incomplete. Starting installation..."

# Install dependencies
echo "Installing build dependencies..."
if command_exists apt-get; then
    # Prevent interactive prompts
    export DEBIAN_FRONTEND=noninteractive
    $SUDO apt-get update
    $SUDO apt-get install -y cmake make g++ libuv1-dev libssl-dev zlib1g-dev wget pkg-config
elif command_exists dnf; then
    $SUDO dnf install -y cmake make gcc-c++ libuv-devel openssl-devel zlib-devel wget pkgconf-pkg-config
elif command_exists yum; then
    $SUDO yum install -y cmake make gcc-c++ libuv-devel openssl-devel zlib-devel wget pkgconfig
else
    echo "Warning: Could not detect package manager. Assuming dependencies are installed."
fi

# Create temp directory in /tmp
TEMP_DIR=$(mktemp -d -p /tmp cassandra_driver_build.XXXXXX)
echo "Using temporary directory: $TEMP_DIR"

# Cleanup function
cleanup() {
    if [ -d "$TEMP_DIR" ]; then
        echo "Cleaning up temporary directory..."
        rm -rf "$TEMP_DIR"
    fi
}
trap cleanup EXIT

cd "$TEMP_DIR"

# Download and build
echo "Downloading Cassandra C/C++ driver 2.17.1..."
wget https://github.com/apache/cassandra-cpp-driver/archive/refs/tags/2.17.1.tar.gz -O cassandra-cpp-driver.tar.gz

echo "Extracting..."
tar xzf cassandra-cpp-driver.tar.gz
cd cassandra-cpp-driver-2.17.1

echo "Building Cassandra C/C++ driver..."
mkdir build
cd build
cmake .. -DCMAKE_BUILD_TYPE=Release -DCASS_BUILD_STATIC=ON -DCASS_BUILD_SHARED=ON -DCMAKE_INSTALL_PREFIX=/usr/local
make -j$(nproc)

echo "Installing Cassandra C/C++ driver..."
$SUDO make install

# Update shared library cache
if command_exists ldconfig; then
    $SUDO ldconfig
fi

echo "Cassandra C/C++ driver installation complete."
