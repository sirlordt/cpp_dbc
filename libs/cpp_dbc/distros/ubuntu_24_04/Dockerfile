FROM ubuntu:24.04

# Install build dependencies
RUN apt-get update && apt-get install -y \
    build-essential \
    cmake \
    g++ \
    python3-pip \
    python3-venv \
    python3-full \
    git \
    fakeroot \
    debhelper \
    devscripts \
    lintian \
    dh-make

# Install Conan package manager in a virtual environment
RUN python3 -m venv /opt/venv && \
    /opt/venv/bin/pip install conan && \
    ln -s /opt/venv/bin/conan /usr/local/bin/conan

# Install database development libraries
RUN apt-get install -y \
    __MYSQL_DEV_PKG__ \
    __SQLITE_DEV_PKG__ \
    __POSTGRESQL_DEV_PKG__ \
    __FIREBIRD_DEV_PKG__ \
    __MONGODB_DEV_PKG__ \
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