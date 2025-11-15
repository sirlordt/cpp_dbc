# cpp_dbc Example Project

This is a simple example project that demonstrates how to use the cpp_dbc library with CMake.

## Prerequisites

- CMake 3.15 or higher
- C++ compiler with C++23 support
- cpp_dbc library installed (via .deb package)

## Building the Example

1. Create a build directory:

```bash
mkdir build
cd build
```

2. Run CMake to configure the project:

```bash
cmake ..
```

3. Build the project:

```bash
cmake --build .
```

## Running the Example

After building the project, you can run the example:

```bash
./simple_example
```

## Troubleshooting

If CMake cannot find the cpp_dbc library, make sure that:

1. The cpp_dbc package is installed correctly
2. The CMake configuration files are in `/usr/lib/cmake/cpp_dbc/`

If you're still having issues, you can manually specify the path to the cpp_dbc CMake configuration files:

```bash
cmake -Dcpp_dbc_DIR=/usr/lib/cmake/cpp_dbc ..
```

## Understanding the Example

The example demonstrates:

1. How to find and link against the cpp_dbc library using CMake
2. How to create a ConnectionPool instance
3. Basic error handling

The actual database connection is commented out to allow the example to run without a real database.