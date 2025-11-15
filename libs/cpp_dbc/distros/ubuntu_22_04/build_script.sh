#!/bin/bash
set -e

# We're already in the container's working directory

# Create output directory for the .deb package
mkdir -p /output

# Create Conan profile first
echo "Creating Conan profile..."
/opt/venv/bin/conan profile detect --force

# Run the build_cpp_dbc.sh script with parameters based on build options
echo "Building cpp_dbc library with specified options..."

# Set default parameters
MYSQL_PARAM="--mysql-off"
POSTGRES_PARAM="--postgres-off"
SQLITE_PARAM="--sqlite-off"
YAML_PARAM="--yaml-off"
DEBUG_PARAM="--debug"
DW_PARAM="--dw-off"
EXAMPLES_PARAM=""

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

echo "Using parameters: $MYSQL_PARAM $POSTGRES_PARAM $SQLITE_PARAM $YAML_PARAM $DEBUG_PARAM $DW_PARAM $EXAMPLES_PARAM"
./libs/cpp_dbc/build_cpp_dbc.sh $MYSQL_PARAM $POSTGRES_PARAM $SQLITE_PARAM $YAML_PARAM $DEBUG_PARAM $DW_PARAM $EXAMPLES_PARAM

# Now create the debian package

# Create debian package structure directly in the working directory

# Create debian package structure
mkdir -p debian/source
echo "1.0" > debian/source/format
echo "10" > debian/compat

# Create control file
cat > debian/control << EOL
Source: cpp-dbc
Section: libs
Priority: optional
Maintainer: Tomas R Moreno P <tomasr.morenop@gmail.com>
Build-Depends: debhelper (>= 10), cmake, g++__MYSQL_CONTROL_DEP____POSTGRESQL_CONTROL_DEP____SQLITE_CONTROL_DEP____LIBDW_CONTROL_DEP__
Standards-Version: 4.5.0
Homepage: https://github.com/sirlordt/cpp_dbc

Package: cpp-dbc
Architecture: amd64
Depends: \${shlibs:Depends}, \${misc:Depends}__MYSQL_CONTROL_DEP____POSTGRESQL_CONTROL_DEP____SQLITE_CONTROL_DEP____LIBDW_CONTROL_DEP__
Description: C++ Database Connectivity Library
 A C++ library for database connectivity inspired by JDBC.
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
	mkdir -p \${CURDIR}/debian/cpp-dbc/usr/lib
	mkdir -p \${CURDIR}/debian/cpp-dbc/usr/include
	mkdir -p \${CURDIR}/debian/cpp-dbc/usr/share/doc/cpp-dbc
	
	# Copy the library files directly from where CMake installed them
	cp -v /app/build/libs/cpp_dbc/lib/libcpp_dbc.a \${CURDIR}/debian/cpp-dbc/usr/lib/
	cp -rv /app/build/libs/cpp_dbc/include/* \${CURDIR}/debian/cpp-dbc/usr/include/
	
	# Copy documentation files
	cp -rv /app/libs/cpp_dbc/docs/* \${CURDIR}/debian/cpp-dbc/usr/share/doc/cpp-dbc/
	
	# Copy example files
	mkdir -p \${CURDIR}/debian/cpp-dbc/usr/share/doc/cpp-dbc/examples
	cp -rv /app/libs/cpp_dbc/examples/* \${CURDIR}/debian/cpp-dbc/usr/share/doc/cpp-dbc/examples/
	
	# Copy CHANGELOG.md from the root of the project
	cp -v /app/CHANGELOG.md \${CURDIR}/debian/cpp-dbc/usr/share/doc/cpp-dbc/changelog
EOL

chmod +x debian/rules

# Create changelog
# Use the timestamp for the version
TIMESTAMP="__TIMESTAMP__"
cat > debian/changelog << EOL
cpp-dbc (${TIMESTAMP}-1) jammy; urgency=medium

  * Initial release 

 -- Tomas R Moreno P <tomasr.morenop@gmail.com>  $(date -R)
EOL

# Create copyright
cat > debian/copyright << EOL
Format: https://www.debian.org/doc/packaging-manuals/copyright-format/1.0/
Upstream-Name: cpp-dbc
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
PACKAGE_NAME="cpp_dbc_V${TIMESTAMP}_${DISTRO_NAME}_${DISTRO_VERSION}_amd64.deb"

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