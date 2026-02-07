# Shell Script Dependencies

This document describes the relationships and call hierarchy between shell scripts in the cpp_dbc project.

## Quick Reference Diagram

```text
┌─────────────────────────────────────────────────────────────────────────────┐
│                              USER ENTRY POINTS                               │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                              │
│   helper.sh ──────────────────────────────────────────────────────────────┐ │
│       │                                                                   │ │
│       ├── sources: scripts/common/functions.sh                            │ │
│       │                                                                   │ │
│       ├──[--run-build]────────► build.sh                                  │ │
│       │                            │                                      │ │
│       │                            └──► libs/cpp_dbc/build_cpp_dbc.sh     │ │
│       │                                    │                              │ │
│       │                                    ├──► download_and_setup_cassandra_driver.sh │
│       │                                    └──► generate_build_config.sh  │ │
│       │                                                                   │ │
│       ├──[--run-build-dist]───► build.dist.sh                             │ │
│       │                            │                                      │ │
│       │                            └──► libs/cpp_dbc/build_dist_pkg.sh    │ │
│       │                                    │                              │ │
│       │                                    └──► distros/<distro>/build_script.sh │
│       │                                                                   │ │
│       ├──[--run-test]─────────► run_test.sh                               │ │
│       │                            │                                      │ │
│       │                            └──► libs/cpp_dbc/run_test_cpp_dbc.sh  │ │
│       │                                                                   │ │
│       ├──[--run-test parallel]─► run_test_parallel.sh                     │ │
│       │                            │                                      │ │
│       │                            └──► run_test.sh (multiple instances)  │ │
│       │                                                                   │ │
│       ├──[--run-benchmarks]───► libs/cpp_dbc/run_benchmarks_cpp_dbc.sh    │ │
│       │                                                                   │ │
│       ├──[--dk-combo-XX]──────► libs/cpp_dbc/create_benchmark_cpp_dbc_base_line.sh │
│       │                                                                   │ │
│       └──[--vscode]───────────► .vscode/sync_intellisense.sh              │ │
│                                    │                                      │ │
│                                    ├──► .vscode/detect_include_paths.sh   │ │
│                                    └──► .vscode/update_defines.sh         │ │
│                                                                           │ │
└───────────────────────────────────────────────────────────────────────────┘ │
                                                                              │
```

## Script Categories

### 1. Entry Point Scripts (Root Level)

| Script | Purpose | Typical Usage |
|--------|---------|---------------|
| `helper.sh` | Main orchestrator for all operations | `./helper.sh --run-build`, `./helper.sh --run-test=...` |
| `build.sh` | Build the main project | `./build.sh --mysql --postgres` |
| `build.dist.sh` | Build distribution packages | `./build.dist.sh` |
| `run_test.sh` | Run tests with various options | `./run_test.sh --rebuild --mysql` |
| `run_test_parallel.sh` | Parallel test execution | Called by `helper.sh` with `parallel=N` |

### 2. Library-Specific Scripts (`libs/cpp_dbc/`)

| Script | Purpose | Called By |
|--------|---------|-----------|
| `build_cpp_dbc.sh` | Build the cpp_dbc library only | `build.sh` |
| `build_test_cpp_dbc.sh` | Build library with test configuration | `run_test.sh` |
| `run_test_cpp_dbc.sh` | Execute library tests | `run_test.sh` |
| `run_benchmarks_cpp_dbc.sh` | Execute benchmarks | `helper.sh --run-benchmarks` |
| `generate_build_config.sh` | Generate `.build_config` file | `build.sh`, `build_cpp_dbc.sh` |
| `download_and_setup_cassandra_driver.sh` | Install ScyllaDB driver from source | `build_cpp_dbc.sh`, `build_test_cpp_dbc.sh` |
| `build_dist_pkg.sh` | Build distribution packages | `build.dist.sh` |
| `create_benchmark_cpp_dbc_base_line.sh` | Create benchmark baselines | `helper.sh --dk-combo-XX` |
| `compare_benchmark_cpp_dbc_base_line.sh` | Compare benchmarks to baseline | Manual usage |

### 3. VSCode Integration Scripts (`.vscode/`)

| Script | Purpose | Called By |
|--------|---------|-----------|
| `sync_intellisense.sh` | Sync IntelliSense with build config | `helper.sh --vscode` |
| `regenerate_intellisense.sh` | Rebuild IntelliSense from scratch | Manual usage |
| `detect_include_paths.sh` | Auto-detect system include paths | `sync_intellisense.sh` |
| `update_defines.sh` | Update preprocessor defines | `sync_intellisense.sh` |
| `build_with_props.sh` | Build with custom properties | Manual usage |

### 4. Distribution Scripts (`libs/cpp_dbc/distros/<distro>/`)

| Script | Purpose | Called By |
|--------|---------|-----------|
| `build_script.sh` | Build inside Docker container | `build_dist_pkg.sh` |
| `Dockerfile` | Define container environment | `build_dist_pkg.sh` |

### 5. Shared Functions Library (`scripts/common/`)

The `scripts/common/functions.sh` file is the **central location for all shared bash functions**.

**DRY Principle**: Any function that is used by more than one script MUST be placed here to avoid code duplication.

| Script | Purpose | Sourced By |
|--------|---------|------------|
| `scripts/common/functions.sh` | Shared functions (colors, logging, validation, etc.) | `helper.sh`, `run_test_parallel.sh`, `run_examples.sh` |

#### Available Functions

| Function | Description |
|----------|-------------|
| **Colors** | |
| `check_color_support` | Returns 0 if terminal supports colors |
| `_init_colors` | Initializes color variables (RED, GREEN, YELLOW, BLUE, CYAN, NC, etc.) |
| `refresh_term_size` | Refresh terminal dimensions after resize |
| `cursor_to`, `cursor_hide`, `cursor_show` | Cursor manipulation |
| **Time/Duration** | |
| `format_duration` | Format seconds as HH:MM:SS |
| `format_elapsed_time` | Format seconds as human-readable (e.g., "1h 23m 45s") |
| **Validation** | |
| `validate_numeric` | Validate that a value is a non-negative integer |
| **Log Management** | |
| `cleanup_old_logs` | Remove old log files, keeping N most recent |
| **Valgrind** | |
| `check_valgrind_errors` | Check log file for Valgrind errors (silent) |
| `check_valgrind_errors_verbose` | Check log file for Valgrind errors (with output) |
| **Print Utilities** | |
| `print_color`, `print_success`, `print_error`, `print_warning`, `print_info` | Colored output helpers |
| `print_line`, `print_header` | Formatting helpers |

#### Usage Example

```bash
#!/bin/bash
# my_script.sh

SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )"
source "$SCRIPT_DIR/scripts/common/functions.sh"

# Now you can use all shared functions
print_info "Starting process..."
if check_color_support; then
    echo -e "${GREEN}Colors are supported${NC}"
fi
```

#### Adding New Shared Functions

When adding new functions to `scripts/common/functions.sh`:

1. Add clear documentation with usage example
2. Group with related functions using section headers
3. Use the guard pattern (already in place) to prevent multiple inclusions
4. Test with all scripts that source it

### 6. Utility Scripts (Root Level)

| Script | Purpose | Called By |
|--------|---------|-----------|
| `generate_dbexception_code.sh` | Generate unique error codes | Manual usage |
| `check_dbexception_codes.sh` | Validate error code uniqueness | Manual usage |
| `update_extern.sh` | Update external dependencies | Manual usage |
| `sonar_qube_issues_fetch.sh` | Fetch SonarQube issues | Manual usage |

### 7. Example Scripts (`libs/cpp_dbc/examples/`)

| Script | Purpose | Called By |
|--------|---------|-----------|
| `run_examples.sh` | Discover and run compiled examples | Manual usage |

## Detailed Call Chains

### Build Chain

```text
helper.sh --run-build=OPTIONS
    │
    └─► build.sh
            │
            ├─► libs/cpp_dbc/build_cpp_dbc.sh
            │       │
            │       ├─► [if USE_SCYLLADB] download_and_setup_cassandra_driver.sh
            │       │
            │       └─► cmake, make
            │
            └─► libs/cpp_dbc/generate_build_config.sh
                    │
                    └─► writes: build/.build_config
```

### Test Chain

```text
helper.sh --run-test=OPTIONS
    │
    ├─► [if parallel=N] run_test_parallel.sh
    │       │
    │       └─► run_test.sh (N parallel instances)
    │
    └─► [if not parallel] run_test.sh
            │
            ├─► [if rebuild] libs/cpp_dbc/build_test_cpp_dbc.sh
            │       │
            │       ├─► [if USE_SCYLLADB] download_and_setup_cassandra_driver.sh
            │       │
            │       └─► cmake, make
            │
            └─► libs/cpp_dbc/run_test_cpp_dbc.sh
                    │
                    └─► ctest or direct test execution
```

### Distribution Build Chain

```text
helper.sh --run-build-dist=OPTIONS
    │
    └─► build.dist.sh
            │
            └─► libs/cpp_dbc/build_dist_pkg.sh
                    │
                    ├─► docker build (uses distros/<distro>/Dockerfile)
                    │
                    └─► docker run (executes distros/<distro>/build_script.sh)
                            │
                            └─► builds inside container
```

### VSCode Sync Chain

```text
helper.sh --vscode
    │
    └─► .vscode/sync_intellisense.sh
            │
            ├─► .vscode/detect_include_paths.sh
            │       │
            │       └─► detects: system includes, Conan paths
            │
            ├─► .vscode/update_defines.sh
            │       │
            │       └─► reads: compile_commands.json
            │
            └─► writes: .vscode/c_cpp_properties.json
```

## Adding a New Driver: Scripts to Update

When adding a new database driver, update these scripts in order:

### Phase 2 Scripts (Build Configuration)

1. **`libs/cpp_dbc/build_cpp_dbc.sh`**
   - Add option parsing (`--newdriver`, `--newdriver-off`)
   - Add help text
   - Add CMake variable (`-DUSE_NEWDRIVER=$USE_NEWDRIVER`)
   - Add dependency check (call setup script if needed)

2. **`libs/cpp_dbc/build_test_cpp_dbc.sh`**
   - Same changes as `build_cpp_dbc.sh`

3. **`libs/cpp_dbc/run_test_cpp_dbc.sh`**
   - Add driver-specific test filtering options

4. **`libs/cpp_dbc/run_benchmarks_cpp_dbc.sh`**
   - Add driver-specific benchmark options

5. **`libs/cpp_dbc/build_dist_pkg.sh`**
   - Add driver option handling
   - Add package dependency placeholders

6. **`helper.sh`** (root)
   - Update `show_usage()` function
   - Update `cmd_run_build()` function
   - Update `cmd_run_test()` function
   - Update `cmd_run_benchmarks()` function
   - Update `--bk-combo-XX` shortcuts
   - Update `--mc-combo-XX` shortcuts
   - Update `--dk-combo-XX` shortcuts

7. **`run_test.sh`** (root)
   - Add option parsing for new driver

8. **`build.sh`** (root)
   - Add option parsing for new driver
   - Add CMake variable passing

9. **Distribution scripts** (for each `libs/cpp_dbc/distros/<distro>/`):
   - `Dockerfile`: Add package placeholder
   - `build_script.sh`: Add parameter handling

### Optional Scripts

10. **`libs/cpp_dbc/download_and_setup_<driver>_driver.sh`** (if no package available)
    - Create new setup script for source compilation
    - Call from `build_cpp_dbc.sh` and `build_test_cpp_dbc.sh`

## Common Patterns

### Option Parsing Pattern

All scripts use similar option parsing:

```bash
# Default values
USE_NEWDRIVER=OFF

# Parse arguments
while [[ $# -gt 0 ]]; do
    case $1 in
        --newdriver|--newdriver-on)
            USE_NEWDRIVER=ON
            shift
            ;;
        --newdriver-off)
            USE_NEWDRIVER=OFF
            shift
            ;;
        *)
            shift
            ;;
    esac
done
```

### CMake Variable Pattern

```bash
# Pass to CMake
cmake ... -DUSE_NEWDRIVER=$USE_NEWDRIVER ...
```

### Dependency Check Pattern

```bash
if [ "$USE_NEWDRIVER" = "ON" ]; then
    if ! pkg-config --exists newdriver 2>/dev/null; then
        echo "NewDriver not found. Running setup script..."
        bash "${SCRIPT_DIR}/download_and_setup_newdriver_driver.sh"
    fi
fi
```

## Validation Commands

After updating scripts, verify with:

```bash
# Verify build works
./helper.sh --mc-combo-01

# Verify tests work
./helper.sh --bk-combo-01

# Verify benchmarks work
./helper.sh --dk-combo-01
```
