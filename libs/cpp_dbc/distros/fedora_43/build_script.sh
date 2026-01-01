#!/bin/bash
set -e

# We're already in the container's working directory

# Create output directory for the .rpm package
mkdir -p /output

# Create Conan profile first
echo "Creating Conan profile..."
conan profile detect --force

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

# Check environment variables set by build_dist_pkg.sh
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

# Create a symlink for MySQL library to match the expected name
if [ -f /usr/lib64/mysql/libmysqlclient.so ]; then
    ln -sf /usr/lib64/mysql/libmysqlclient.so /usr/lib64/libmysqlclient.so
elif [ -f /usr/lib64/libmysqlclient.so.21 ]; then
    ln -sf /usr/lib64/libmysqlclient.so.21 /usr/lib64/libmysqlclient.so
fi

# Run the build script
./libs/cpp_dbc/build_cpp_dbc.sh $MYSQL_PARAM $POSTGRES_PARAM $SQLITE_PARAM $FIREBIRD_PARAM $MONGODB_PARAM $REDIS_PARAM $YAML_PARAM $DEBUG_PARAM $DW_PARAM $EXAMPLES_PARAM $DB_DRIVER_THREAD_SAFE_PARAM $DEBUG_POOL_PARAM $DEBUG_TXMGR_PARAM $DEBUG_SQLITE_PARAM $DEBUG_FIREBIRD_PARAM $DEBUG_MONGODB_PARAM $DEBUG_REDIS_PARAM $DEBUG_ALL_PARAM

# Now create the RPM package

# Set up RPM build environment
echo "Setting up RPM build environment..."
mkdir -p ~/rpmbuild/{BUILD,BUILDROOT,RPMS,SOURCES,SPECS,SRPMS}

# Create a .rpmmacros file to ensure proper paths
cat > ~/.rpmmacros << EOL
%_topdir    %(echo $HOME)/rpmbuild
%_builddir  %{_topdir}/BUILD
%_rpmdir    %{_topdir}/RPMS
%_sourcedir %{_topdir}/SOURCES
%_specdir   %{_topdir}/SPECS
%_srcrpmdir %{_topdir}/SRPMS
%_buildrootdir %{_topdir}/BUILDROOT
EOL

# Create a tarball of the built library
echo "Creating source tarball..."
TIMESTAMP="__TIMESTAMP__"
# Format timestamp for RPM (replace hyphens with dots)
RPM_VERSION=$(echo "${TIMESTAMP}" | tr '-' '.')
PACKAGE_NAME="cpp-dbc-dev-${TIMESTAMP}"
mkdir -p /tmp/${PACKAGE_NAME}

# Copy the built library files to the package directory
mkdir -p /tmp/${PACKAGE_NAME}/usr/lib
mkdir -p /tmp/${PACKAGE_NAME}/usr/include
mkdir -p /tmp/${PACKAGE_NAME}/usr/share/doc/cpp-dbc-dev
mkdir -p /tmp/${PACKAGE_NAME}/usr/lib/cmake/cpp_dbc

# Copy the library files directly from where CMake installed them
cp -v /app/build/libs/cpp_dbc/lib/libcpp_dbc.a /tmp/${PACKAGE_NAME}/usr/lib/
cp -rv /app/build/libs/cpp_dbc/include/* /tmp/${PACKAGE_NAME}/usr/include/

# Copy documentation files
cp -rv /app/libs/cpp_dbc/docs/* /tmp/${PACKAGE_NAME}/usr/share/doc/cpp-dbc-dev/

# Copy example files
mkdir -p /tmp/${PACKAGE_NAME}/usr/share/doc/cpp-dbc-dev/examples
cp -rv /app/libs/cpp_dbc/examples/* /tmp/${PACKAGE_NAME}/usr/share/doc/cpp-dbc-dev/examples/

# Copy CHANGELOG.md from the root of the project
cp -v /app/CHANGELOG.md /tmp/${PACKAGE_NAME}/usr/share/doc/cpp-dbc-dev/changelog

# Copy CMake files for find_package support
cp -v /app/libs/cpp_dbc/cmake/FindMySQL.cmake /tmp/${PACKAGE_NAME}/usr/lib/cmake/cpp_dbc/
cp -v /app/libs/cpp_dbc/cmake/FindPostgreSQL.cmake /tmp/${PACKAGE_NAME}/usr/lib/cmake/cpp_dbc/
cp -v /app/libs/cpp_dbc/cmake/FindSQLite3.cmake /tmp/${PACKAGE_NAME}/usr/lib/cmake/cpp_dbc/
cp -v /app/libs/cpp_dbc/cmake/FindCPPDBC.cmake /tmp/${PACKAGE_NAME}/usr/lib/cmake/cpp_dbc/
cp -v /app/libs/cpp_dbc/cmake/cpp_dbc-config.cmake /tmp/${PACKAGE_NAME}/usr/lib/cmake/cpp_dbc/
cp -v /app/libs/cpp_dbc/cmake/cpp_dbc-config-version.cmake /tmp/${PACKAGE_NAME}/usr/lib/cmake/cpp_dbc/

# Replace placeholders in the config file with actual values
sed -i "s/@USE_MYSQL@/__USE_MYSQL__/g" /tmp/${PACKAGE_NAME}/usr/lib/cmake/cpp_dbc/cpp_dbc-config.cmake
sed -i "s/@USE_POSTGRESQL@/__USE_POSTGRESQL__/g" /tmp/${PACKAGE_NAME}/usr/lib/cmake/cpp_dbc/cpp_dbc-config.cmake
sed -i "s/@USE_SQLITE@/__USE_SQLITE__/g" /tmp/${PACKAGE_NAME}/usr/lib/cmake/cpp_dbc/cpp_dbc-config.cmake
sed -i "s/@USE_FIREBIRD@/__USE_FIREBIRD__/g" /tmp/${PACKAGE_NAME}/usr/lib/cmake/cpp_dbc/cpp_dbc-config.cmake
sed -i "s/@USE_MONGODB@/__USE_MONGODB__/g" /tmp/${PACKAGE_NAME}/usr/lib/cmake/cpp_dbc/cpp_dbc-config.cmake
sed -i "s/@USE_REDIS@/__USE_REDIS__/g" /tmp/${PACKAGE_NAME}/usr/lib/cmake/cpp_dbc/cpp_dbc-config.cmake
sed -i "s/@USE_CPP_YAML@/__USE_CPP_YAML__/g" /tmp/${PACKAGE_NAME}/usr/lib/cmake/cpp_dbc/cpp_dbc-config.cmake
sed -i "s/@BACKWARD_HAS_DW@/__USE_DW__/g" /tmp/${PACKAGE_NAME}/usr/lib/cmake/cpp_dbc/cpp_dbc-config.cmake

# Create the spec file for RPM
cat > ~/rpmbuild/SPECS/cpp-dbc-dev.spec << EOL
Name:           cpp-dbc-dev
Version:        ${RPM_VERSION}
Release:        1%{?dist}
Summary:        C++ Database Connectivity Library - Development files

License:        GPLv3
URL:            https://github.com/sirlordt/cpp_dbc
BuildArch:      x86_64
BuildRoot:      %{_tmppath}/%{name}-%{version}-%{release}-root-%(%{__id_u} -n)

%description
A C++ library for database connectivity inspired by JDBC.

This package contains the development files (headers and static libraries)
needed to build applications that use cpp_dbc.

This package was built with:
  __BUILD_FLAGS__

%prep
# No prep needed, files are already in BUILDROOT

%build
# No build needed, files are already built

%install
# Copy files to BUILDROOT during install phase
echo "DEBUG: BUILDROOT = \$RPM_BUILD_ROOT"
mkdir -p \$RPM_BUILD_ROOT/usr/lib
mkdir -p \$RPM_BUILD_ROOT/usr/include
mkdir -p \$RPM_BUILD_ROOT/usr/share/doc/cpp-dbc-dev
mkdir -p \$RPM_BUILD_ROOT/usr/lib/cmake/cpp_dbc

# Copy files from our prepared directory
cp -rv /tmp/${PACKAGE_NAME}/usr/* \$RPM_BUILD_ROOT/usr/
ls -la \$RPM_BUILD_ROOT/usr || echo "BUILDROOT/usr not found"

%files
%defattr(-,root,root,-)
/usr/include/cpp_dbc
/usr/include/cpp_dbc/*
/usr/lib/libcpp_dbc.a
/usr/lib/cmake/cpp_dbc/*
/usr/share/doc/cpp-dbc-dev/*

%changelog
* $(date "+%a %b %d %Y") Tomas R Moreno P <tomasr.morenop@gmail.com> - ${TIMESTAMP}-1
- Initial RPM release
EOL

# Create the directory structure in the BUILDROOT
echo "DEBUG: Current directory: $(pwd)"
echo "DEBUG: Listing rpmbuild directory structure:"
ls -la ~/rpmbuild/
echo "DEBUG: Package name: ${PACKAGE_NAME}"
echo "DEBUG: RPM Version: ${RPM_VERSION}"

# We don't need to copy files to BUILDROOT here anymore
# The %install section in the spec file will handle that
# Just prepare the source tarball for rpmbuild
echo "DEBUG: Listing source files:"
ls -la /tmp/${PACKAGE_NAME}/

# Create a tarball for the source
echo "DEBUG: Creating source tarball"
mkdir -p ~/rpmbuild/SOURCES
tar -czf ~/rpmbuild/SOURCES/cpp-dbc-dev-${RPM_VERSION}.tar.gz -C /tmp ${PACKAGE_NAME}

# List the BUILDROOT directories to verify files were copied
echo "DEBUG: Listing BUILDROOT directory 1 after copy:"
ls -la ${BUILDROOT_DIR1}/
echo "DEBUG: Listing BUILDROOT directory 2 after copy:"
ls -la ${BUILDROOT_DIR2}/

# Build the RPM package
echo "Building RPM package..."
echo "DEBUG: Contents of SPECS directory:"
ls -la ~/rpmbuild/SPECS/
echo "DEBUG: Contents of spec file:"
cat ~/rpmbuild/SPECS/cpp-dbc-dev.spec

# Run rpmbuild with verbose output
echo "DEBUG: Running rpmbuild command"
rpmbuild -vv -bb ~/rpmbuild/SPECS/cpp-dbc-dev.spec

# Find and move the package with timestamp
DISTRO_NAME="__DISTRO_NAME__"
DISTRO_VERSION="__DISTRO_VERSION__"
FINAL_PACKAGE_NAME="cpp-dbc-dev_V${TIMESTAMP}_${DISTRO_NAME}-${DISTRO_VERSION}_x86_64.rpm"

# Find the generated .rpm file
RPM_FILE=$(find ~/rpmbuild/RPMS -name "*.rpm" -type f -print -quit)

if [ -n "$RPM_FILE" ]; then
    echo "Found package: $RPM_FILE"
    cp "$RPM_FILE" "/output/$FINAL_PACKAGE_NAME"
    echo "Package copied to: /output/$FINAL_PACKAGE_NAME"
else
    echo "Error: No .rpm package found!"
    # List files to debug
    echo "Files in RPMS directory:"
    ls -la ~/rpmbuild/RPMS
    exit 1
fi

# Clean up temporary directory
rm -rf /tmp/${PACKAGE_NAME}