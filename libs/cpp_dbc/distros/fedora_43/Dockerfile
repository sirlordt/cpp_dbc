FROM fedora:43

# Install build dependencies
RUN dnf -y update && dnf -y install \
    gcc-c++ \
    cmake \
    make \
    python3-pip \
    python3-devel \
    git \
    rpm-build \
    rpmdevtools \
    rpmlint \
    fedora-packager

# Install Conan package manager
RUN python3 -m pip install conan && \
    ln -s /usr/local/bin/conan /usr/bin/conan

# Install database development libraries
RUN dnf -y install \
    __MYSQL_DEV_PKG__ \
    __SQLITE_DEV_PKG__ \
    __POSTGRESQL_DEV_PKG__ \
    __FIREBIRD_DEV_PKG__ \
    __MONGODB_DEV_PKG__ \
    __REDIS_DEV_PKG__ \
    __LIBDW_DEV_PKG__

# Create working directory
WORKDIR /app

# Copy the source code
COPY . /app/

# Copy build script
COPY build/build_script.sh /build_script.sh
RUN chmod +x /build_script.sh

# Set the entrypoint to the build script
ENTRYPOINT ["/build_script.sh"]