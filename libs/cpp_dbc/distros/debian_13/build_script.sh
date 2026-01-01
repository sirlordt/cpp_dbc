#!/bin/bash
set -e

# We're already in the container's working directory

# Create output directory for the .deb package
mkdir -p /output

# Apply MySQL fix patch for Debian 13
echo "Applying MySQL fix patch for Debian 13..."
cd /app
patch -p1 < /app/libs/cpp_dbc/distros/debian_13/mysql_fix.patch

# Create Conan profile first
echo "Creating Conan profile..."
/opt/venv/bin/conan profile detect --force

# Run the build_cpp_dbc.sh script with parameters based on build options
echo "Building cpp_dbc library with specified options..."

# Set default parameters
MYSQL_PARAM="--mysql-off"
POSTGRES_PARAM="--postgres-off"
SQLITE_PARAM="--sqlite-off"
FIREBIRD_PARAM="--firebird-off"
MONGODB_PARAM="--mongodb-off"
REDIS_PARAM="--redis-off"
YAML_PARAM="--yaml-off"
DEBUG_PARAM="--debug"
DW_PARAM="--dw-off"
EXAMPLES_PARAM=""
DB_DRIVER_THREAD_SAFE_PARAM=""
DEBUG_POOL_PARAM=""
DEBUG_TXMGR_PARAM=""
DEBUG_SQLITE_PARAM=""
DEBUG_FIREBIRD_PARAM=""
DEBUG_MONGODB_PARAM=""
DEBUG_REDIS_PARAM=""
DEBUG_ALL_PARAM=""

# Check environment variables set by build_dist_deb.sh
if [ "__USE_MYSQL__" = "ON" ]; then
    MYSQL_PARAM="--mysql"
else
    MYSQL_PARAM="--mysql-off"
fi

if [ "__USE_POSTGRESQL__" = "ON" ]; then
    POSTGRES_PARAM="--postgres"
else
    POSTGRES_PARAM="--postgres-off"
fi

if [ "__USE_SQLITE__" = "ON" ]; then
    SQLITE_PARAM="--sqlite"
else
    SQLITE_PARAM="--sqlite-off"
fi

if [ "__USE_FIREBIRD__" = "ON" ]; then
    FIREBIRD_PARAM="--firebird"
else
    FIREBIRD_PARAM="--firebird-off"
fi

if [ "__USE_MONGODB__" = "ON" ]; then
    MONGODB_PARAM="--mongodb"
else
    MONGODB_PARAM="--mongodb-off"
fi

if [ "__USE_REDIS__" = "ON" ]; then
    REDIS_PARAM="--redis"
else
    REDIS_PARAM="--redis-off"
fi

if [ "__USE_CPP_YAML__" = "ON" ]; then
    YAML_PARAM="--yaml"
else
    YAML_PARAM="--yaml-off"
fi

if [ "__USE_DW__" = "ON" ]; then
    DW_PARAM="--dw"
else
    DW_PARAM="--dw-off"
fi

# Set debug parameter based on BUILD_TYPE
if [ "__BUILD_TYPE__" = "Debug" ]; then
    DEBUG_PARAM="--debug"
else
    DEBUG_PARAM="--release"
fi

# Set examples parameter if needed
if [ "__BUILD_EXAMPLES__" = "ON" ]; then
    EXAMPLES_PARAM="--examples"
fi

# Set DB driver thread-safe parameter if needed
if [ "__DB_DRIVER_THREAD_SAFE__" = "OFF" ]; then
    DB_DRIVER_THREAD_SAFE_PARAM="--db-driver-thread-safe-off"
fi

# Set debug options
if [ "__DEBUG_CONNECTION_POOL__" = "ON" ]; then
    DEBUG_POOL_PARAM="--debug-pool"
fi

if [ "__DEBUG_TRANSACTION_MANAGER__" = "ON" ]; then
    DEBUG_TXMGR_PARAM="--debug-txmgr"
fi

if [ "__DEBUG_SQLITE__" = "ON" ]; then
    DEBUG_SQLITE_PARAM="--debug-sqlite"
fi

if [ "__DEBUG_FIREBIRD__" = "ON" ]; then
    DEBUG_FIREBIRD_PARAM="--debug-firebird"
fi

if [ "__DEBUG_MONGODB__" = "ON" ]; then
    DEBUG_MONGODB_PARAM="--debug-mongodb"
fi

if [ "__DEBUG_REDIS__" = "ON" ]; then
    DEBUG_REDIS_PARAM="--debug-redis"
fi

if [ "__DEBUG_ALL__" = "ON" ]; then
    DEBUG_ALL_PARAM="--debug-all"
fi

echo "Using parameters: $MYSQL_PARAM $POSTGRES_PARAM $SQLITE_PARAM $FIREBIRD_PARAM $MONGODB_PARAM $REDIS_PARAM $YAML_PARAM $DEBUG_PARAM $DW_PARAM $EXAMPLES_PARAM $DB_DRIVER_THREAD_SAFE_PARAM $DEBUG_POOL_PARAM $DEBUG_TXMGR_PARAM $DEBUG_SQLITE_PARAM $DEBUG_FIREBIRD_PARAM $DEBUG_MONGODB_PARAM $DEBUG_REDIS_PARAM $DEBUG_ALL_PARAM"
./libs/cpp_dbc/build_cpp_dbc.sh $MYSQL_PARAM $POSTGRES_PARAM $SQLITE_PARAM $FIREBIRD_PARAM $MONGODB_PARAM $REDIS_PARAM $YAML_PARAM $DEBUG_PARAM $DW_PARAM $EXAMPLES_PARAM $DB_DRIVER_THREAD_SAFE_PARAM $DEBUG_POOL_PARAM $DEBUG_TXMGR_PARAM $DEBUG_SQLITE_PARAM $DEBUG_FIREBIRD_PARAM $DEBUG_MONGODB_PARAM $DEBUG_REDIS_PARAM $DEBUG_ALL_PARAM

# Now create the debian package

# Create debian package structure directly in the working directory

# Create debian package structure
mkdir -p debian/source
echo "1.0" > debian/source/format
echo "10" > debian/compat

# Create control file
cat > debian/control << EOL
Source: cpp-dbc-dev
Section: libs
Priority: optional
Maintainer: Tomas R Moreno P <tomasr.morenop@gmail.com>
Build-Depends: debhelper (>= 10), cmake, g++__MYSQL_CONTROL_DEP____POSTGRESQL_CONTROL_DEP____SQLITE_CONTROL_DEP____FIREBIRD_CONTROL_DEP____MONGODB_CONTROL_DEP____LIBDW_CONTROL_DEP__
Standards-Version: 4.5.0
Homepage: https://github.com/sirlordt/cpp_dbc

Package: cpp-dbc-dev
Architecture: amd64
Depends: \${shlibs:Depends}, \${misc:Depends}__MYSQL_CONTROL_DEP____POSTGRESQL_CONTROL_DEP____SQLITE_CONTROL_DEP____FIREBIRD_CONTROL_DEP____MONGODB_CONTROL_DEP____LIBDW_CONTROL_DEP__
Description: C++ Database Connectivity Library - Development files
 A C++ library for database connectivity inspired by JDBC.
 .
 This package contains the development files (headers and static libraries)
 needed to build applications that use cpp_dbc.
 .
 This package was built with:
   __BUILD_FLAGS__
EOL

# Create rules file
cat > debian/rules << EOL
#!/usr/bin/make -f

%:
	dh \$@

override_dh_auto_configure:
	# Skip configure as we already built the library

override_dh_auto_build:
	# Skip build as we already built the library

override_dh_auto_install:
	# Copy the built library files to the package directory
	mkdir -p \${CURDIR}/debian/cpp-dbc-dev/usr/lib
	mkdir -p \${CURDIR}/debian/cpp-dbc-dev/usr/include
	mkdir -p \${CURDIR}/debian/cpp-dbc-dev/usr/share/doc/cpp-dbc-dev
	mkdir -p \${CURDIR}/debian/cpp-dbc-dev/usr/lib/cmake/cpp_dbc
	
	# Copy the library files directly from where CMake installed them
	cp -v /app/build/libs/cpp_dbc/lib/libcpp_dbc.a \${CURDIR}/debian/cpp-dbc-dev/usr/lib/
	cp -rv /app/build/libs/cpp_dbc/include/* \${CURDIR}/debian/cpp-dbc-dev/usr/include/
	
	# Copy documentation files
	cp -rv /app/libs/cpp_dbc/docs/* \${CURDIR}/debian/cpp-dbc-dev/usr/share/doc/cpp-dbc-dev/
	
	# Copy example files
	mkdir -p \${CURDIR}/debian/cpp-dbc-dev/usr/share/doc/cpp-dbc-dev/examples
	cp -rv /app/libs/cpp_dbc/examples/* \${CURDIR}/debian/cpp-dbc-dev/usr/share/doc/cpp-dbc-dev/examples/
	
	# Copy CHANGELOG.md from the root of the project
	cp -v /app/CHANGELOG.md \${CURDIR}/debian/cpp-dbc-dev/usr/share/doc/cpp-dbc-dev/changelog
	
	# Copy CMake files for find_package support
	cp -v /app/libs/cpp_dbc/cmake/FindMySQL.cmake \${CURDIR}/debian/cpp-dbc-dev/usr/lib/cmake/cpp_dbc/
	cp -v /app/libs/cpp_dbc/cmake/FindPostgreSQL.cmake \${CURDIR}/debian/cpp-dbc-dev/usr/lib/cmake/cpp_dbc/
	cp -v /app/libs/cpp_dbc/cmake/FindSQLite3.cmake \${CURDIR}/debian/cpp-dbc-dev/usr/lib/cmake/cpp_dbc/
	cp -v /app/libs/cpp_dbc/cmake/FindCPPDBC.cmake \${CURDIR}/debian/cpp-dbc-dev/usr/lib/cmake/cpp_dbc/
	cp -v /app/libs/cpp_dbc/cmake/cpp_dbc-config.cmake \${CURDIR}/debian/cpp-dbc-dev/usr/lib/cmake/cpp_dbc/
	cp -v /app/libs/cpp_dbc/cmake/cpp_dbc-config-version.cmake \${CURDIR}/debian/cpp-dbc-dev/usr/lib/cmake/cpp_dbc/
	
	# Replace placeholders in the config file with actual values
	sed -i "s/@USE_MYSQL@/__USE_MYSQL__/g" \${CURDIR}/debian/cpp-dbc-dev/usr/lib/cmake/cpp_dbc/cpp_dbc-config.cmake
	sed -i "s/@USE_POSTGRESQL@/__USE_POSTGRESQL__/g" \${CURDIR}/debian/cpp-dbc-dev/usr/lib/cmake/cpp_dbc/cpp_dbc-config.cmake
	sed -i "s/@USE_SQLITE@/__USE_SQLITE__/g" \${CURDIR}/debian/cpp-dbc-dev/usr/lib/cmake/cpp_dbc/cpp_dbc-config.cmake
	sed -i "s/@USE_FIREBIRD@/__USE_FIREBIRD__/g" \${CURDIR}/debian/cpp-dbc-dev/usr/lib/cmake/cpp_dbc/cpp_dbc-config.cmake
	sed -i "s/@USE_MONGODB@/__USE_MONGODB__/g" \${CURDIR}/debian/cpp-dbc-dev/usr/lib/cmake/cpp_dbc/cpp_dbc-config.cmake
	sed -i "s/@USE_REDIS@/__USE_REDIS__/g" \${CURDIR}/debian/cpp-dbc-dev/usr/lib/cmake/cpp_dbc/cpp_dbc-config.cmake
	sed -i "s/@USE_CPP_YAML@/__USE_CPP_YAML__/g" \${CURDIR}/debian/cpp-dbc-dev/usr/lib/cmake/cpp_dbc/cpp_dbc-config.cmake
	sed -i "s/@BACKWARD_HAS_DW@/__USE_DW__/g" \${CURDIR}/debian/cpp-dbc-dev/usr/lib/cmake/cpp_dbc/cpp_dbc-config.cmake
EOL

chmod +x debian/rules

# Create changelog
# Use the timestamp for the version
TIMESTAMP="__TIMESTAMP__"
cat > debian/changelog << EOL
cpp-dbc-dev (${TIMESTAMP}-1) trixie; urgency=medium

  * Initial release 

 -- Tomas R Moreno P <tomasr.morenop@gmail.com>  $(date -R)
EOL

# Create copyright
cat > debian/copyright << EOL
Format: https://www.debian.org/doc/packaging-manuals/copyright-format/1.0/
Upstream-Name: cpp-dbc-dev
Source: https://github.com/sirlordt/cpp_dbc

Files: *
Copyright: 2025 Tomas R Moreno P
License: GNU GPLV3
EOL

# Build the package
debuild -us -uc -b

# Find and move the package with timestamp
# Use the timestamp and distro information passed from the main script
TIMESTAMP="__TIMESTAMP__"
DISTRO_NAME="__DISTRO_NAME__"
DISTRO_VERSION="__DISTRO_VERSION__"
PACKAGE_NAME="cpp-dbc-dev_V${TIMESTAMP}_${DISTRO_NAME}-${DISTRO_VERSION}_amd64.deb"

# Find the generated .deb file
DEB_FILE=$(find .. -name "*.deb" -type f -print -quit)

if [ -n "$DEB_FILE" ]; then
    echo "Found package: $DEB_FILE"
    cp "$DEB_FILE" "/output/$PACKAGE_NAME"
    echo "Package copied to: /output/$PACKAGE_NAME"
else
    echo "Error: No .deb package found!"
    # List files to debug
    echo "Files in current directory:"
    ls -la
    echo "Files in parent directory:"
    ls -la ..
    exit 1
fi

# Clean up temporary directory
# We're already in the container's working directory