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
4. How to use the library's warning flags for code quality

The actual database connection is commented out to allow the example to run without a real database.

## Code Quality and Warning Flags

The cpp_dbc library is built with comprehensive warning flags and compile-time checks. You can adopt these same flags in your project for consistent code quality:

```cmake
# In your CMakeLists.txt
target_compile_options(your_target PRIVATE
    -Wall -Wextra -Wpedantic -Wconversion -Wshadow -Wcast-qual
    -Wformat=2 -Wunused -Werror=return-type -Werror=switch
    -Wdouble-promotion -Wfloat-equal -Wundef -Wpointer-arith -Wcast-align
)
```

These warning flags help catch potential issues:

- `-Wall -Wextra -Wpedantic`: Standard warning flags
- `-Wconversion`: Warns about implicit conversions that may change a value
- `-Wshadow`: Warns when a variable declaration shadows another variable
- `-Wcast-qual`: Warns about casts that remove type qualifiers
- `-Wformat=2`: Enables additional format string checks
- `-Wunused`: Warns about unused variables and functions
- `-Werror=return-type`: Treats missing return statements as errors
- `-Werror=switch`: Treats switch statement issues as errors
- `-Wdouble-promotion`: Warns about implicit float to double promotions
- `-Wfloat-equal`: Warns about floating-point equality comparisons
- `-Wundef`: Warns about undefined identifiers in preprocessor expressions
- `-Wpointer-arith`: Warns about suspicious pointer arithmetic
- `-Wcast-align`: Warns about pointer casts that increase alignment requirements

For best results, follow the same coding practices as the cpp_dbc library:
- Use m_ prefix for member variables to avoid -Wshadow warnings
- Add static_cast<> for numeric conversions to avoid -Wconversion warnings
- Use appropriate integer types (e.g., uint64_t instead of int) for values that shouldn't be negative
- Be careful with exception handling to avoid variable shadowing