# IntelliSense Configuration for C++

This document explains how to configure and maintain IntelliSense working correctly in VSCode for this C++ project.

## Common Problem

Sometimes IntelliSense shows disabled code (in dim colors) or doesn't offer autocompletion when you type `.` because it doesn't correctly detect the active conditional compilation variables (like `USE_MYSQL`, `USE_POSTGRESQL`, etc.).

## Automatic Synchronization System

The project now has an **automatic synchronization system** that keeps VSCode IntelliSense in sync with your build configuration:

1. Build configuration is centrally managed by `libs/cpp_dbc/generate_build_config.sh`
2. When you build the project (via `./build.sh`, `./helper.sh --run-test`, or test scripts):
   - Build parameters are saved to `build/.build_args`
   - Configuration state is saved to `build/.build_config`
   - If `.build_config` is missing, it's auto-regenerated from CMakeCache.txt
3. VSCode scripts read these files to stay synchronized
4. All build methods (main project, library, tests) use the same unified build directory: `build/libs/cpp_dbc/build/`
5. No need to manually specify parameters twice!

### How Build Configuration Works

**Centralized Configuration Management:**

The `libs/cpp_dbc/generate_build_config.sh` script is the single source of truth for generating `build/.build_config`. It supports two modes:

1. **With parameters** (called by build scripts):
   ```bash
   ./libs/cpp_dbc/generate_build_config.sh --mysql=ON --postgres=ON --sqlite=ON ...
   ```

2. **Without parameters** (auto-detection):
   ```bash
   ./libs/cpp_dbc/generate_build_config.sh
   ```
   Searches for CMakeCache.txt in known locations:
   - `build/libs/cpp_dbc/build/CMakeCache.txt` (library/test builds)
   - `build/CMakeCache.txt` (main project builds)

   Uses the most recent one found to extract configuration.

**Unified Build Directory:**

The library and tests share a single build directory (`build/libs/cpp_dbc/build/`) to prevent double compilation:
- Library compiles once and generates `.a` static library
- Tests link against the single compiled library
- Both share the same build configuration and Conan dependencies

**VSCode Path Management:**

The `.vscode/detect_include_paths.sh` script automatically detects include paths for:
- Main project builds: `build/Debug/generators`, `build/Release/generators`
- Library/test builds: `build/libs/cpp_dbc/build`, `build/libs/cpp_dbc/conan/build/{Debug,Release}/generators`
- Conan packages from cache: `~/.conan2/p/*/include`
- System libraries: `/usr/include/*`, `/usr/local/include`

Paths are converted to relative using VSCode variables:
- `${workspaceFolder}` for project-relative paths
- `${userHome}` for user-specific paths (like Conan cache)

## Solution

### Option 1: Quick Sync (Fastest - Recommended)

After building with `./build.sh`, synchronize IntelliSense without rebuilding:

```bash
./.vscode/sync_intellisense.sh
```

This script:
- Reads the configuration from your last build (`build/.build_config`)
- If `.build_config` is missing, auto-regenerates it from CMakeCache.txt
- Detects include paths for enabled drivers and Conan dependencies
- Updates `c_cpp_properties.json` with all compilation defines and include paths
- Shows which drivers are enabled
- **Does NOT rebuild** - very fast!

**Example workflow:**

```bash
# 1. Build with your desired configuration
./build.sh --scylladb --mongodb

# 2. Sync IntelliSense (reads saved config automatically)
./.vscode/sync_intellisense.sh

# 3. Reload VSCode
# Ctrl+Shift+P -> "Developer: Reload Window"
```

### Option 2: Rebuild from Last Config

Rebuild the project using the same parameters as your last build:

```bash
./.vscode/regenerate_intellisense.sh
```

This script:
- Reads `build/.build_args` (parameters from last build)
- Rebuilds the project with those same parameters
- Updates IntelliSense automatically

**Example:**

```bash
# Last time you built with: ./build.sh --scylladb --mongodb
# Now just run (no parameters needed):
./.vscode/regenerate_intellisense.sh

# It will automatically use: --scylladb --mongodb
```

### Option 3: Rebuild with New Config

Rebuild with different parameters:

```bash
# Build with specific options
./.vscode/regenerate_intellisense.sh --postgres --redis

# Or use build.sh directly, then sync
./build.sh --postgres --redis
./.vscode/sync_intellisense.sh
```

### Option 4: Update from compile_commands.json Only

If you've already built the project but IntelliSense still doesn't work correctly:

```bash
./.vscode/update_defines.sh
```

This script extracts definitions from `compile_commands.json` and automatically updates `c_cpp_properties.json`.

### Option 3: Manual

1. Build the project normally:
   ```bash
   ./build.sh [options]
   ```

2. Copy the compilation commands file:
   ```bash
   cp build/compile_commands.json .
   ```

3. Reload VSCode: `Ctrl+Shift+P` → `Developer: Reload Window`

## Configuration Files

### `.vscode/c_cpp_properties.json`
Defines include paths, defines, and IntelliSense configuration. This file must be synchronized with the active compilation options.

**Important:** The `defines` values must match those used by CMake during compilation.

### `.vscode/settings.json`
General VSCode configuration for the project, including:
- Configuration provider for IntelliSense
- Path to `compile_commands.json`
- CMake Tools settings

### `compile_commands.json`
Automatically generated by CMake. Contains the exact compilation commands used for each file. IntelliSense uses this file to understand the compilation context.

## Available Compilation Variables

The project supports the following conditional compilation variables:

| Variable | Default | Description |
|----------|---------|-------------|
| `USE_MYSQL` | ON | MySQL support |
| `USE_POSTGRESQL` | OFF | PostgreSQL support |
| `USE_SQLITE` | OFF | SQLite support |
| `USE_FIREBIRD` | OFF | Firebird SQL support |
| `USE_MONGODB` | OFF | MongoDB support |
| `USE_SCYLLADB` | OFF | ScyllaDB support |
| `USE_REDIS` | OFF | Redis support |
| `USE_CPP_YAML` | OFF | YAML configuration support |
| `DB_DRIVER_THREAD_SAFE` | ON | Thread-safe operations |
| `BACKWARD_HAS_DW` | ON | libdw support for stack traces |

### Debug Variables

| Variable | Default | Description |
|----------|---------|-------------|
| `DEBUG_CONNECTION_POOL` | OFF | Debug for ConnectionPool |
| `DEBUG_TRANSACTION_MANAGER` | OFF | Debug for TransactionManager |
| `DEBUG_SQLITE` | OFF | Debug for SQLite driver |
| `DEBUG_FIREBIRD` | OFF | Debug for Firebird driver |
| `DEBUG_MONGODB` | OFF | Debug for MongoDB driver |
| `DEBUG_SCYLLADB` | OFF | Debug for ScyllaDB driver |
| `DEBUG_REDIS` | OFF | Debug for Redis driver |
| `DEBUG_ALL` | OFF | Debug for all components |

## Recommended Workflow

1. **Before starting to code**, ensure compilation variables are correct:
   ```bash
   # Example: working with ScyllaDB
   ./.vscode/regenerate_intellisense.sh --scylladb
   ```

2. **Reload VSCode** to apply changes:
   - `Ctrl+Shift+P` → `Developer: Reload Window`
   - Or simply restart VSCode

3. **Verify IntelliSense works**:
   - Open a `.cpp` file
   - Type a variable name followed by `.`
   - You should see autocompletion
   - Code inside `#if USE_SCYLLADB` blocks (or whichever you activated) should NOT appear dimmed

4. **If you change compilation options**, repeat from step 1

## Troubleshooting

### IntelliSense still shows dimmed code

1. Verify that `compile_commands.json` exists in the project root:
   ```bash
   ls -la compile_commands.json
   ```

2. Check current definitions:
   ```bash
   grep -o '\-D[A-Z_]*=[0-9]' compile_commands.json | sort -u
   ```

3. Compare with definitions in `.vscode/c_cpp_properties.json`

4. If they don't match, run:
   ```bash
   ./.vscode/update_defines.sh
   ```

### Autocompletion doesn't work

1. Verify that the C++ IntelliSense server is running:
   - `Ctrl+Shift+P` → `C/C++: Reset IntelliSense Database`

2. Check the C++ extension is installed:
   - Must be `ms-vscode.cpptools`
   - Also recommended: `ms-vscode.cmake-tools`

3. Review the output log:
   - `View` → `Output` → Select `C/C++` in the dropdown

### Correct code appears disabled

This indicates that definitions in `c_cpp_properties.json` don't match those used during compilation.

Solution:
```bash
./.vscode/regenerate_intellisense.sh [your-compilation-options]
```

## References

- [VSCode C++ IntelliSense](https://code.visualstudio.com/docs/cpp/c-cpp-properties-schema-reference)
- [CMake compile_commands.json](https://cmake.org/cmake/help/latest/variable/CMAKE_EXPORT_COMPILE_COMMANDS.html)
- [C++ Extension for VSCode](https://marketplace.visualstudio.com/items?itemName=ms-vscode.cpptools)
