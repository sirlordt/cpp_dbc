#!/bin/bash
set -e

# We're already in the container's working directory

# Create output directory for the .deb package
mkdir -p /output

# Create Conan profile first
echo "Creating Conan profile..."
/opt/venv/bin/conan profile detect --force

# Run the build_cpp_dbc.sh script with MySQL-only parameters
echo "Building cpp_dbc library with MySQL support only..."
./libs/cpp_dbc/build_cpp_dbc.sh --mysql --postgres-off --sqlite-off --yaml-off --debug --dw-off

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
Build-Depends: debhelper (>= 10), cmake, g++, default-libmysqlclient-dev
Standards-Version: 4.5.0
Homepage: https://github.com/sirlordt/cpp_dbc

Package: cpp-dbc
Architecture: amd64
Depends: \${shlibs:Depends}, \${misc:Depends}, default-libmysqlclient-dev
Description: C++ Database Connectivity Library
 A C++ library for database connectivity inspired by JDBC.
 MySQL-only version.
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
	
	# Copy the library files directly from where CMake installed them
	cp -v /app/build/libs/cpp_dbc/lib/libcpp_dbc.a \${CURDIR}/debian/cpp-dbc/usr/lib/
	cp -rv /app/build/libs/cpp_dbc/include/* \${CURDIR}/debian/cpp-dbc/usr/include/
EOL

chmod +x debian/rules

# Create changelog
cat > debian/changelog << EOL
cpp-dbc (1.0.0-1) noble; urgency=medium

  * Initial release with MySQL-only support
  * Closes: #12345 (fictitious bug number for package compliance)

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
TIMESTAMP=$(date +"%Y-%m-%d-%H-%M-%S")
PACKAGE_NAME="cpp_dbc_amd64_${TIMESTAMP}.deb"

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