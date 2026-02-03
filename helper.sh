#!/usr/bin/env bash
#
# helper.sh
# Utility for building, running, and inspecting your cpp_dbc container.
#

set -e

# Detect if running inside Distrobox container
if [ -z "$DISTROBOX_ENTER_PATH" ]; then
    echo "You are currently on the host operating system."
    echo "This script is designed to run inside a Distrobox container."
    echo "Please enter the Distrobox container first by running:"
    echo "  distrobox enter my_container"
    exit 1
fi

DIST_BUILD_FILE=".dist_build"
DEFAULT_CONTAINER_NAME=""

if [[ -f "$DIST_BUILD_FILE" ]]; then
  DEFAULT_CONTAINER_NAME=$(awk -F'"' '/^Container_Name=/{print $2}' "$DIST_BUILD_FILE")
fi

get_container_name() {
  local arg_name="$1"
  local __resultvar="$2"

  local container_name=""

  if [[ -n "$arg_name" ]]; then
    echo "Using provided container name: '$arg_name'"
    container_name="$arg_name"
  elif [[ -n "$DEFAULT_CONTAINER_NAME" ]]; then
    echo "Container name not specified, using default '$DEFAULT_CONTAINER_NAME' from .dist_build."
    container_name="$DEFAULT_CONTAINER_NAME"
  else
    echo "Error: No container name provided and no default found in .dist_build."
    return 1
  fi

  if ! docker image inspect "$container_name" &>/dev/null; then
    echo "Error: Docker image/container '$container_name' does not exist. Aborting."
    return 1
  fi

  eval $__resultvar="'$container_name'"
  return 0
}

show_usage() {
  echo "Usage: $(basename "$0") COMMAND [COMMAND...] [container_name]"
  echo "Commands:"
  echo "  --run-build              Build via ./build.sh, logs to build/run-build-<timestamp>.log"
  echo "  --run-build=OPTIONS      Build with comma-separated options"
  echo "                           Available options: clean,release,gcc-analyzer,postgres,mysql,mysql-off,sqlite,firebird,mongodb,scylladb,redis,yaml,test,examples,"
  echo "                           debug-pool,debug-txmgr,debug-sqlite,debug-firebird,debug-mongodb,debug-scylladb,debug-redis,debug-all,dw-off,db-driver-thread-safe-off,benchmarks"
  echo "                           Example: --run-build=clean,sqlite,yaml,test,debug-pool"
  echo "  --run-build-dist         Build via ./build.dist.sh, logs to build/run-build-dist-<timestamp>.log"
  echo "  --run-build-dist=OPTIONS Build dist with comma-separated options"
  echo "                           Available options: clean,release,postgres,mysql,mysql-off,sqlite,firebird,mongodb,scylladb,redis,yaml,test,examples,"
  echo "                           debug-pool,debug-txmgr,debug-sqlite,debug-firebird,debug-mongodb,debug-scylladb,debug-redis,debug-all,dw-off,db-driver-thread-safe-off,benchmarks"
  echo "                           Example: --run-build-dist=clean,sqlite,yaml,test,debug-sqlite"
  echo "  --check-test-log         Check the most recent test log file in logs/test/ for failures and memory issues"
  echo "  --check-test-log=PATH    Check the specified test log file for failures and memory issues"
  echo "  --run-ctr [name]         Run container"
  echo "  --show-labels-ctr [name] Show labels with Image ID"
  echo "  --show-tags-ctr [name]   Show tags with Image ID"
  echo "  --show-env-ctr [name]    Show environment variables defined in the image"
  echo "  --clean-conan-cache      Clear Conan local cache"
  echo "  --run-test               Build (if needed) and run the tests"
  echo "  --run-test=OPTIONS       Run tests with comma-separated options"
  echo "                           Available options: clean,release,gcc-analyzer,rebuild,sqlite,firebird,mongodb,scylladb,redis,mysql,mysql-off,postgres,valgrind,"
  echo "                                              yaml,auto,asan,ctest,check,progress,run=N,parallel=N,test=FILTER,"
  echo "                                              debug-pool,debug-txmgr,debug-sqlite,debug-firebird,debug-mongodb,debug-scylladb,debug-redis,debug-all,dw-off,db-driver-thread-safe-off"
  echo "                           Test filter formats (test=FILTER):"
  echo "                             - Wildcard: test=*mysql* matches test names containing 'mysql'"
  echo "                             - Tags: test=tag1+tag2 runs tests with those specific tags"
  echo "                           Parallel execution (parallel=N):"
  echo "                             - Runs N test prefixes (10_, 20_, 21_, etc.) in parallel"
  echo "                             - Each prefix runs independently with separate log files"
  echo "                             - If a prefix fails, it stops but others continue"
  echo "                             - Maximum N is the number of available prefixes"
  echo "                             - Logs saved in logs/test/YYYY-MM-DD-HH-MM-SS/PREFIX_RUNXX.log"
  echo "                           Parallel order (parallel-order=P1_,P2_,...):"
  echo "                             - Specify which prefixes should run first"
  echo "                             - Useful for prioritizing slow tests (e.g., 23_)"
  echo "                             - Example: parallel=5,parallel-order=23_,24_ runs 5 parallel tasks"
  echo "                               with 23_ and 24_ starting first"
  echo "                           Example: --run-test=rebuild,mysql,valgrind,run=1,test=*mysql*"
  echo "                           Example: --run-test=rebuild,sqlite,valgrind,run=3,test=integration+mysql_real_right_join"
  echo "                           Example: --run-test=clean,rebuild,sqlite,mysql,postgres,yaml,valgrind,auto,run=1"
  echo "                           Example: --run-test=rebuild,sqlite,mysql,postgres,yaml,auto,progress,run=1"
  echo "                           Example: --run-test=rebuild,sqlite,postgres,mysql,valgrind,auto,parallel=3,run=2"
  echo "                           Example: --run-test=rebuild,sqlite,mysql,valgrind,auto,parallel=5,parallel-order=23_,24_,run=2"
  echo "                           Note: progress option shows visual progress bar on 2 lines:"
  echo "                                 Line 1: [Run: 1/5] [Test: 3/10] name ██████████░░░░░░░░░░ 30% [Elapsed: 00:05:23]"
  echo "                                 Line 2: [Last: previous_test_name 00:01:45]"
  echo "  --run-benchmarks         Run the benchmarks"
  echo "  --run-benchmarks=OPTIONS Run benchmarks with comma-separated options"
  echo "                           Available options: clean,release,rebuild,sqlite,firebird,mongodb,scylladb,redis,mysql,mysql-off,postgres,"
  echo "                                              yaml,benchmark=Tag1+Tag2+Tag3,memory-usage,base-line,repetitions=3,iterations=1000,"
  echo "                                              debug-pool,debug-txmgr,debug-sqlite,debug-firebird,debug-mongodb,debug-scylladb,debug-redis,debug-all,dw-off,db-driver-thread-safe-off"
  echo "                           Example: --run-benchmarks=mysql,postgresql"
  echo "                           Example: --run-benchmarks=benchmark=update+postgresql"
  echo "                           Example: --run-benchmarks=rebuild,mongodb,yaml,benchmark=mongodb+delete"
  echo "                           Example: --run-benchmarks=clean,rebuild,sqlite,mysql,postgres,yaml,memory-usage"
  echo "                           Example: --run-benchmarks=mysql,postgresql,base-line"
  echo "                           Note: Multiple benchmark tags can be specified using + as separator after benchmark="
  echo "  --ldd-bin-ctr [name]     Run ldd on the executable inside the container"
  echo "  --ldd-build-bin          Run ldd on the final local build/ executable"
  echo "  --run-build-bin          Run the final local build/ executable"
  echo "  --bk-combo-01            Equivalent to --run-test=rebuild,sqlite,postgres,mysql,firebird,mongodb,scylladb,redis,yaml,valgrind,auto,run=1"
  echo "  --bk-combo-02            Equivalent to --run-test=rebuild,sqlite,postgres,mysql,firebird,mongodb,scylladb,redis,yaml,auto,run=1"
  echo "  --bk-combo-03            Equivalent to --run-test=rebuild,sqlite,postgres,mysql,firebird,mongodb,scylladb,redis,yaml,valgrind,auto,run=5"
  echo "  --bk-combo-04            Equivalent to --run-test=rebuild,sqlite,postgres,mysql,firebird,mongodb,scylladb,redis,yaml,auto,run=5"
  echo "  --bk-combo-05            Equivalent to --run-test=clean,rebuild,sqlite,postgres,mysql,firebird,mongodb,scylladb,redis,yaml,valgrind,auto,run=1"
  echo "  --bk-combo-06            Equivalent to --run-test=clean,rebuild,sqlite,postgres,mysql,firebird,mongodb,scylladb,redis,yaml,auto,run=1"
  echo "  --bk-combo-07            Equivalent to --run-test=sqlite,postgres,mysql,firebird,mongodb,scylladb,redis,yaml,valgrind,auto,run=1"
  echo "  --bk-combo-08            Equivalent to --run-test=sqlite,postgres,mysql,firebird,mongodb,scylladb,redis,yaml,auto,run=1"
  echo "  --bk-combo-09            Equivalent to --run-test=clean,rebuild,sqlite,postgres,mysql,firebird,mongodb,scylladb,redis,yaml,valgrind,auto,progress,run=1"
  echo "  --bk-combo-10            Equivalent to --run-test=rebuild,sqlite,postgres,mysql,firebird,mongodb,scylladb,redis,yaml,valgrind,auto,progress,run=5"
  echo "  --bk-combo-11            Equivalent to --run-test=clean,rebuild,sqlite,postgres,mysql,firebird,mongodb,scylladb,redis,yaml,valgrind,auto,progress,parallel=5,run=1"
  echo "  --bk-combo-12            Equivalent to --run-test=rebuild,sqlite,postgres,mysql,firebird,mongodb,scylladb,redis,yaml,valgrind,auto,progress,parallel=5,run=5"
  echo "  --mc-combo-01            Equivalent to --run-build=clean,postgres,mysql,sqlite,firebird,mongodb,scylladb,redis,yaml,test,examples"
  echo "  --mc-combo-02            Equivalent to --run-build=postgres,sqlite,mysql,scylladb,redis,yaml,test,examples"
  echo "  --kfc-combo-01           Equivalent to --run-build-dist=clean,rebuild,sqlite,postgres,mysql,firebird,mongodb,scylladb,redis,yaml,test,examples"
  echo "  --kfc-combo-02           Equivalent to --run-build-dist=rebuild,sqlite,postgres,mysql,firebird,mongodb,scylladb,redis,yaml,test,examples"
  echo "  --dk-combo-01            Equivalent to --run-benchmarks=rebuild,sqlite,postgres,mysql,firebird,mongodb,scylladb,redis,yaml,memory-usage,base-line"
  echo "  --dk-combo-02            Equivalent to --run-benchmarks=clean,rebuild,sqlite,postgres,mysql,firebird,mongodb,scylladb,redis,yaml,memory-usage,base-line"
  echo "  --vscode                 Synchronize VSCode IntelliSense with last build configuration"
  echo ""
  echo "Multiple commands can be combined, e.g.:"
  echo "  $(basename "$0") --clean-conan-cache --run-build"
  echo "  $(basename "$0") --run-build --vscode"
}

cleanup_old_logs() {
  local log_path="$1"
  local max_logs="$2"
  
  # Default values if not provided
  if [ -z "$log_path" ]; then
    echo "Error: Log path not provided to cleanup_old_logs"
    return 1
  fi
  
  if [ -z "$max_logs" ]; then
    max_logs=4
  fi
  
  # Ensure the log directory exists
  mkdir -p "$(dirname "$log_path")"
  
  # Get all log files in the directory, sorted by modification time (newest first)
  local logs=($(ls -1t "${log_path}"/*.log 2>/dev/null))
  
  # If we have more logs than the maximum allowed, remove the oldest ones
  if (( ${#logs[@]} > max_logs )); then
    echo "Cleaning up old logs in ${log_path}, keeping ${max_logs} most recent logs..."
    printf '%s\n' "${logs[@]:$max_logs}" | xargs rm -f --
  fi
}

# Function to check if terminal supports colors
check_color_support() {
  if [ -t 1 ] && [ -n "$TERM" ] && [ "$TERM" != "dumb" ]; then
    return 0  # Terminal supports colors
  else
    return 1  # Terminal doesn't support colors
  fi
}

# Auto-sync VSCode IntelliSense configuration if c_cpp_properties.json doesn't exist
# This ensures new developers get a working IntelliSense setup after first build
auto_sync_vscode_if_missing() {
  local current_dir=$(pwd)
  local cpp_properties="${current_dir}/.vscode/c_cpp_properties.json"

  if [ ! -f "$cpp_properties" ]; then
    echo ""
    echo "📝 VSCode IntelliSense configuration not found."
    echo "🔄 Auto-generating c_cpp_properties.json for first-time setup..."
    echo ""
    cmd_vscode
  fi
}

cmd_run_build() {

  local ts=$(date '+%Y-%m-%d-%H-%M-%S_%z')
  # Get current directory
  local current_dir=$(pwd)
  # Ensure the build directory exists
  mkdir -p "${current_dir}/logs/build"
  local log_file="${current_dir}/logs/build/output-${ts}.log"
  echo "Building project. Output logging to $log_file."

  # Clean up old logs in the build directory, keeping the 4 most recent
  cleanup_old_logs "${current_dir}/logs/build" 4

  # Build command with options
  local build_cmd="./build.sh"
  
  # Process comma-separated options
  IFS=',' read -ra OPTIONS <<< "$BUILD_OPTIONS"
  for opt in "${OPTIONS[@]}"; do
    case "$opt" in
      clean)
        build_cmd="$build_cmd --clean"
        echo "Cleaning build directories"
        ;;
      postgres)
        build_cmd="$build_cmd --postgres"
        echo "Enabling PostgreSQL support"
        ;;
      mysql)
        build_cmd="$build_cmd --mysql"
        echo "Enabling MySQL support"
        ;;
      mysql-off)
        build_cmd="$build_cmd --mysql-off"
        echo "Disabling MySQL support"
        ;;
      sqlite)
        build_cmd="$build_cmd --sqlite"
        echo "Enabling SQLite support"
        ;;
      firebird)
        build_cmd="$build_cmd --firebird"
        echo "Enabling Firebird SQL support"
        ;;
      mongodb)
        build_cmd="$build_cmd --mongodb"
        echo "Enabling MongoDB support"
        ;;
      scylladb)
        build_cmd="$build_cmd --scylladb"
        echo "Enabling ScyllaDB support"
        ;;
      redis)
        build_cmd="$build_cmd --redis"
        echo "Enabling Redis support"
        ;;
      yaml)
        build_cmd="$build_cmd --yaml"
        echo "Enabling YAML support"
        ;;
      test)
        build_cmd="$build_cmd --test"
        echo "Building tests"
        ;;
      benchmarks)
        build_cmd="$build_cmd --benchmarks"
        echo "Building benchmarks"
        ;;
      examples)
        build_cmd="$build_cmd --examples"
        echo "Building examples"
        ;;
      release)
        build_cmd="$build_cmd --release"
        echo "Building in release mode"
        ;;
      gcc-analyzer)
        build_cmd="$build_cmd --gcc-analyzer"
        echo "Enabling GCC Static Analyzer"
        ;;
      debug-pool)
        build_cmd="$build_cmd --debug-pool"
        echo "Enabling debug output for ConnectionPool"
        ;;
      debug-txmgr)
        build_cmd="$build_cmd --debug-txmgr"
        echo "Enabling debug output for TransactionManager"
        ;;
      debug-sqlite)
        build_cmd="$build_cmd --debug-sqlite"
        echo "Enabling debug output for SQLite driver"
        ;;
      debug-firebird)
        build_cmd="$build_cmd --debug-firebird"
        echo "Enabling debug output for Firebird driver"
        ;;
      debug-mongodb)
        build_cmd="$build_cmd --debug-mongodb"
        echo "Enabling debug output for MongoDB driver"
        ;;
      debug-scylladb)
        build_cmd="$build_cmd --debug-scylladb"
        echo "Enabling debug output for ScyllaDB driver"
        ;;
      debug-redis)
        build_cmd="$build_cmd --debug-redis"
        echo "Enabling debug output for Redis driver"
        ;;
      debug-all)
        build_cmd="$build_cmd --debug-all"
        echo "Enabling all debug output"
        ;;
      dw-off)
        build_cmd="$build_cmd --dw-off"
        echo "Disabling libdw support for stack traces"
        ;;
      db-driver-thread-safe-off)
        build_cmd="$build_cmd --db-driver-thread-safe-off"
        echo "Disabling thread-safe database driver operations"
        ;;
      *)
        echo "Warning: Unknown build option: $opt"
        ;;
    esac
  done

  echo "Running: $build_cmd"
  
  # Check if terminal supports colors
  if check_color_support; then
    # Run with colors in terminal but without colors in log file
    # Use unbuffer to preserve colors in terminal output and sed to strip ANSI color codes from log file
    unbuffer $build_cmd 2>&1 | tee >(sed -r "s/\x1B\[([0-9]{1,3}(;[0-9]{1,3})*)?[mGK]//g" > "$log_file")
  else
    # Terminal doesn't support colors, just run normally and strip any color codes that might be present
    $build_cmd 2>&1 | tee >(sed -r "s/\x1B\[([0-9]{1,3}(;[0-9]{1,3})*)?[mGK]//g" > "$log_file")
  fi

  echo ""
  echo "Command runned: $build_cmd"
  echo "Log file: $log_file."

  # Auto-sync VSCode configuration if this is the first build
  auto_sync_vscode_if_missing

}

cmd_run_test() {
  local ts=$(date '+%Y-%m-%d-%H-%M-%S_%z')
  # Get current directory
  local current_dir=$(pwd)

  # Check if parallel mode is requested
  local parallel_count=0
  local parallel_order=""
  if [[ "$TEST_OPTIONS" =~ parallel=([0-9]+) ]]; then
    parallel_count="${BASH_REMATCH[1]}"
    # Remove parallel= from options (it will be passed separately)
    TEST_OPTIONS=$(echo "$TEST_OPTIONS" | sed -E "s/parallel=[0-9]+,?//g" | sed -E "s/,+/,/g" | sed -E "s/^,|,$//g")
  fi
  # Check if parallel-order is specified (e.g., parallel-order=23_,24_ or parallel-order=23_)
  # Also accept "paralel-order" (single 'l') as an alias
  # The pattern matches prefixes like 23_ or 23_,24_,25_ (numbers followed by underscore)
  if [[ "$TEST_OPTIONS" =~ parall?el-order=([0-9]+_)(,[0-9]+_)* ]]; then
    # Extract the full match including all prefixes (try both spellings)
    parallel_order=$(echo "$TEST_OPTIONS" | grep -oE 'parall?el-order=[0-9]+_(,[0-9]+_)*' | sed -E 's/parall?el-order=//')
    # Remove parallel-order= or paralel-order= from options
    TEST_OPTIONS=$(echo "$TEST_OPTIONS" | sed -E "s/parall?el-order=[0-9]+_(,[0-9]+_)*,?//g" | sed -E "s/,+/,/g" | sed -E "s/^,|,$//g")
  fi

  # Build command with options (same for both parallel and normal mode)
  local run_test_cmd="./run_test.sh"

  # First, extract all test tags from the options string
  local test_tags=""
  if [[ "$TEST_OPTIONS" =~ test= ]]; then
    # Extract everything after test= up to the next option with =
    local test_part=$(echo "$TEST_OPTIONS" | grep -o "test=[^=]*" | sed 's/,.*//')
    
    # If there are plus signs after test=, we need to handle them specially
    if [[ "$TEST_OPTIONS" =~ test=([^,]+) ]]; then
      # Get everything after test= up to the next option with =
      local full_test_part=$(echo "$TEST_OPTIONS" | grep -o "test=[^=]*" | sed 's/,run=.*//')
      # Remove the "test=" prefix
      test_tags="${full_test_part#test=}"
    else
      # Simple case - just one tag
      test_tags="${test_part#test=}"
    fi
    
    # Remove test tags and any following test tags from the options string
    TEST_OPTIONS=$(echo "$TEST_OPTIONS" | sed -E "s/test=[^,]+//g")
    # Clean up any double commas that might have been created
    TEST_OPTIONS=$(echo "$TEST_OPTIONS" | sed -E "s/,,/,/g")
    # Remove trailing comma if present
    TEST_OPTIONS=$(echo "$TEST_OPTIONS" | sed -E "s/,$//g")
    
    echo "Debug: Extracted test tags: '$test_tags'"
    echo "Debug: Remaining options: '$TEST_OPTIONS'"
  fi
  
  # Process remaining comma-separated options
  local run_test_cmd="./run_test.sh"
  IFS=',' read -ra OPTIONS <<< "$TEST_OPTIONS"
  
  for opt in "${OPTIONS[@]}"; do
    # Skip empty options
    if [[ -z "$opt" ]]; then
      continue
    fi
    
    # Check if option is run=N format
    if [[ "$opt" =~ ^run=[1-9][0-9]*$ ]]; then
      # Extract the number after run=
      local run_count="${opt#run=}"
      run_test_cmd="$run_test_cmd --run=$run_count"
      echo "Setting run count to $run_count"
    else
      case "$opt" in
        clean)
          run_test_cmd="$run_test_cmd --clean"
          echo "Cleaning build directories"
          ;;
        rebuild)
          run_test_cmd="$run_test_cmd --rebuild"
          ;;
        sqlite)
          run_test_cmd="$run_test_cmd --sqlite"
          ;;
        firebird)
          run_test_cmd="$run_test_cmd --firebird"
          ;;
        mongodb)
          run_test_cmd="$run_test_cmd --mongodb"
          ;;
        scylladb)
          run_test_cmd="$run_test_cmd --scylladb"
          ;;
        redis)
          run_test_cmd="$run_test_cmd --redis"
          ;;
        mysql)
          run_test_cmd="$run_test_cmd --mysql"
          ;;
        mysql-off)
          run_test_cmd="$run_test_cmd --mysql-off"
          ;;
        postgres)
          run_test_cmd="$run_test_cmd --postgres"
          ;;
        valgrind)
          run_test_cmd="$run_test_cmd --valgrind"
          ;;
        yaml)
          run_test_cmd="$run_test_cmd --yaml"
          ;;
        yaml-off)
          run_test_cmd="$run_test_cmd --yaml-off"
          ;;
        auto)
          run_test_cmd="$run_test_cmd --auto"
          ;;
        asan)
          run_test_cmd="$run_test_cmd --asan"
          ;;
        ctest)
          run_test_cmd="$run_test_cmd --ctest"
          ;;
        check)
          run_test_cmd="$run_test_cmd --check"
          ;;
        progress)
          run_test_cmd="$run_test_cmd --progress"
          echo "Enabling progress bar display"
          ;;
        debug-pool)
          run_test_cmd="$run_test_cmd --debug-pool"
          echo "Enabling debug output for ConnectionPool"
          ;;
        debug-txmgr)
          run_test_cmd="$run_test_cmd --debug-txmgr"
          echo "Enabling debug output for TransactionManager"
          ;;
        debug-sqlite)
          run_test_cmd="$run_test_cmd --debug-sqlite"
          echo "Enabling debug output for SQLite driver"
          ;;
        debug-firebird)
          run_test_cmd="$run_test_cmd --debug-firebird"
          echo "Enabling debug output for Firebird driver"
          ;;
        debug-mongodb)
          run_test_cmd="$run_test_cmd --debug-mongodb"
          echo "Enabling debug output for MongoDB driver"
          ;;
        debug-scylladb)
          run_test_cmd="$run_test_cmd --debug-scylladb"
          echo "Enabling debug output for ScyllaDB driver"
          ;;
        debug-redis)
          run_test_cmd="$run_test_cmd --debug-redis"
          echo "Enabling debug output for Redis driver"
          ;;
        debug-all)
          run_test_cmd="$run_test_cmd --debug-all"
          echo "Enabling all debug output"
          ;;
        dw-off)
          run_test_cmd="$run_test_cmd --dw-off"
          echo "Disabling libdw support for stack traces"
          ;;
        db-driver-thread-safe-off)
          run_test_cmd="$run_test_cmd --db-driver-thread-safe-off"
          echo "Disabling thread-safe database driver operations"
          ;;
        release)
          run_test_cmd="$run_test_cmd --release"
          echo "Building in release mode"
          ;;
        gcc-analyzer)
          run_test_cmd="$run_test_cmd --gcc-analyzer"
          echo "Enabling GCC Static Analyzer"
          ;;
        *)
          echo "Warning: Unknown test option: $opt"
          ;;
      esac
    fi
  done
  
  # Add the test tags if they exist
  if [ -n "$test_tags" ]; then
    echo "Debug: Final test tags: '$test_tags'"
    run_test_cmd="$run_test_cmd --run-test=$test_tags"
    echo "Setting test tags to: $test_tags"
  fi

  # If parallel mode is enabled, use the parallel runner
  if [ "$parallel_count" -gt 0 ]; then
    echo "Using parallel test runner with $parallel_count concurrent test prefixes"

    # Convert run_test.sh command to run_test_cpp_dbc.sh arguments
    # Remove "./run_test.sh" prefix (with or without trailing space) and trim leading whitespace
    local test_args="${run_test_cmd#./run_test.sh}"
    test_args="${test_args# }"  # Remove leading space if present

    # Build the parallel test command
    local parallel_cmd="./run_test_parallel.sh"
    parallel_cmd="$parallel_cmd --parallel=$parallel_count"
    if [ -n "$parallel_order" ]; then
      parallel_cmd="$parallel_cmd --parallel-order=$parallel_order"
    fi
    parallel_cmd="$parallel_cmd $test_args"

    echo "Running: $parallel_cmd"
    $parallel_cmd
    return $?
  fi

  # Normal (non-parallel) test execution continues below
  # Ensure the test log directory exists
  mkdir -p "${current_dir}/logs/test"
  local log_file="${current_dir}/logs/test/output-${ts}.log"
  echo "Running tests. Output logging to $log_file."

  # Clean up old logs in the test directory, keeping the 4 most recent
  cleanup_old_logs "${current_dir}/logs/test" 4

  echo "$0 => running: $run_test_cmd"

  # Create a temporary file to capture raw output
  local raw_log_file=$(mktemp)

  # Function to process raw log into final log file
  process_raw_log() {
    if [ -f "$raw_log_file" ]; then
      # Sync to ensure all data is written to disk
      sync

      # # DEBUG: Show raw log stats before processing
      # echo ""
      # echo "=== DEBUG: Raw log file stats ==="
      # echo "Raw log file: $raw_log_file"
      # echo "Raw log size: $(wc -c < "$raw_log_file") bytes"
      # echo "Raw log lines: $(wc -l < "$raw_log_file")"
      # echo "Filters count in raw: $(grep -c 'Filters:' "$raw_log_file" 2>/dev/null || echo 0)"
      # echo "All Test Runs in raw: $(grep -c 'All Test Runs Completed' "$raw_log_file" 2>/dev/null || echo 0)"
      # echo "================================="

      # Process the raw log:
      # 1. Remove null bytes (can be introduced by terminal scroll region commands)
      # 2. Strip ANSI escape sequences
      # 3. Remove carriage returns
      # 4. Filter out progress bar lines
      tr -d '\0' < "$raw_log_file" | sed -r "s/\x1B\[[0-9;]*[a-zA-Z]//g; s/\x1B\[[@-~]//g; s/\r//g" | grep -v '@@PROGRESS@@' > "$log_file"

      # # DEBUG: Show processed log stats
      # echo ""
      # echo "=== DEBUG: Processed log file stats ==="
      # echo "Processed log file: $log_file"
      # echo "Processed log size: $(wc -c < "$log_file") bytes"
      # echo "Processed log lines: $(wc -l < "$log_file")"
      # echo "Filters count in processed: $(grep -c 'Filters:' "$log_file" 2>/dev/null || echo 0)"
      # echo "All Test Runs in processed: $(grep -c 'All Test Runs Completed' "$log_file" 2>/dev/null || echo 0)"
      # echo "========================================"

      # Clean up temporary raw log file (comment out to keep for debugging)
      rm -f "$raw_log_file"
      # echo "DEBUG: Raw log kept at: $raw_log_file"
    fi
  }

  # Set trap to process log even on interruption (Ctrl+C)
  trap 'process_raw_log; exit 130' INT TERM

  # Check if terminal supports colors
  if check_color_support; then
    # Run with colors in terminal, capture raw output to temp file
    # Use unbuffer to preserve colors in terminal output
    # The key fix: use tee with -i to ignore interrupt signals and ensure complete write
    unbuffer $run_test_cmd 2>&1 | tee -i "$raw_log_file"
  else
    # Terminal doesn't support colors, just run normally
    $run_test_cmd 2>&1 | tee -i "$raw_log_file"
  fi

  # Ensure all data is flushed to disk before processing
  sync
  sleep 0.5

  # Process the raw log (normal exit)
  process_raw_log

  # Remove the trap
  trap - INT TERM

  echo ""
  echo "Command runned: $run_test_cmd"
  echo "Log file: $log_file."

  # Auto-sync VSCode configuration if this is the first build
  auto_sync_vscode_if_missing

  # Automatically check the test log after running tests
  # Ejecutar check-test-log como un comando separado para asegurar la misma salida
  echo ""
  echo "========== Checking Test Log for Issues =========="
  # Usar bash -c para ejecutar el comando en un nuevo shell
  bash -c "./helper.sh --check-test-log=\"$log_file\""
  echo "========== End of Test Log Check =========="
}

# Helper function to extract available test cases from log file
extract_available_tests() {
  local log_file="$1"
  local in_test_section=0
  local test_description=""
  local test_tag=""
  
  # Create a temporary file to store the test cases
  local temp_file=$(mktemp)
  
  # Read the log file line by line
  while IFS= read -r line; do
    # Start capturing after "All available test cases:"
    if [[ "$line" == *"All available test cases:"* ]]; then
      in_test_section=1
      continue
    fi
    
    # Stop capturing when we reach the test case count line
    if [[ $in_test_section -eq 1 && "$line" =~ [0-9]+[[:space:]]test[[:space:]]cases ]]; then
      break
    fi
    
    # Process lines in the test section
    if [[ $in_test_section -eq 1 ]]; then
      # Skip empty lines or lines with valgrind output
      if [[ -z "$line" || "$line" =~ ^--[0-9]+ || "$line" =~ REDIR ]]; then
        continue
      fi
      
      # Test description lines start with two spaces and don't have brackets
      if [[ "$line" =~ ^[[:space:]][[:space:]][^[:space:]\[] ]]; then
        test_description=$(echo "$line" | sed -e 's/^[[:space:]]*//')
      # Tag lines start with spaces and contain tags in brackets
      elif [[ "$line" =~ ^[[:space:]]+\[([^\]]+)\] ]]; then
        test_tag="${BASH_REMATCH[1]}"
        # Output the test description and tag to the temp file
        if [ -n "$test_description" ]; then
          echo "$test_description|$test_tag" >> "$temp_file"
        fi
      fi
    fi
  done < "$log_file"
  
  # Output the contents of the temp file
  cat "$temp_file"
  
  # Clean up
  rm -f "$temp_file"
}

# Helper function to count total runs in log file
count_total_runs() {
  local log_file="$1"

  # Primary method: Look for "(Run X of Y)" patterns and get the max Y value
  # This is the most reliable way to detect the number of runs
  local max_runs=$(grep -o "(Run [0-9]* of [0-9]*)" "$log_file" 2>/dev/null | sed 's/.* of \([0-9]*\))/\1/' | sort -rn | head -1)
  if [ -n "$max_runs" ] && [ "$max_runs" -gt 0 ]; then
    echo "$max_runs"
    return
  fi

  # Fallback: Check if "All Test Runs Completed" exists (indicates at least 1 run completed)
  if grep -q "All Test Runs Completed" "$log_file" 2>/dev/null; then
    echo "1"
    return
  fi

  # Default: assume 1 run
  echo "1"
}

# Helper function to extract executed test cases from log file for a specific run
# Returns: tag|line_number|duration_seconds for each executed test
# Parameters:
#   $1 - log_file path
#   $2 - run_number (optional, if not provided returns all runs aggregated)
extract_executed_tests() {
  local log_file="$1"
  local run_number="${2:-}"

  # Create a temporary file to store the executed tags with line numbers
  local temp_file=$(mktemp)

  # Extract all filter tags from the log file with line numbers
  # Format: line_number:Filters: [tag]
  grep -a -n "Filters: \[[^]]*\]" "$log_file" > "$temp_file" 2>/dev/null || true

  # Track which tags we've already seen (for run filtering)
  declare -A tag_count

  # For each tag, check if there are any passed or failed tests
  while IFS= read -r line; do
    [ -z "$line" ] && continue
    # Extract line number and tag from the grep output
    local line_number=$(echo "$line" | cut -d: -f1)
    local tag=$(echo "$line" | sed 's/.*Filters: \[\([^]]*\)\]/\1/')

    # Track occurrence count for this tag
    tag_count[$tag]=$((${tag_count[$tag]:-0} + 1))
    local current_occurrence=${tag_count[$tag]}

    # If run_number is specified, only process tags from that run
    if [ -n "$run_number" ] && [ "$current_occurrence" -ne "$run_number" ]; then
      continue
    fi

    # Look for lines with passed/failed starting from this specific line_number
    # and stopping at the next "Filters:" boundary (or end of file)
    # This ensures we only inspect tests for this specific Filters block
    if sed -n "${line_number},\$p" "$log_file" | sed '/^.*Filters: \[/{ 1!q }' | grep -a -q "\.cpp:[0-9]\+: passed:\|\.cpp:[0-9]\+: failed:"; then
      # Try to find the duration marker for this tag at the specific occurrence
      local duration=""
      if [ -n "$run_number" ]; then
        # Get the Nth occurrence of the duration marker for this tag
        duration=$(grep -a "@@TEST_DURATION:${tag}:" "$log_file" 2>/dev/null | sed -n "${run_number}p" | sed 's/.*@@TEST_DURATION:[^:]*:\([0-9]*\)@@.*/\1/')
      else
        # Get the first occurrence (legacy behavior)
        duration=$(grep -a "@@TEST_DURATION:${tag}:" "$log_file" 2>/dev/null | head -1 | sed 's/.*@@TEST_DURATION:[^:]*:\([0-9]*\)@@.*/\1/')
      fi
      if [ -z "$duration" ]; then
        duration="0"
      fi
      echo "${tag}|${line_number}|${duration}"
    fi
  done < "$temp_file"

  # Clean up
  rm -f "$temp_file"
}

# Helper function to extract total duration for each test across all runs
# Returns: tag|total_duration_seconds for each executed test
extract_total_durations() {
  local log_file="$1"

  # Get all unique tags
  local tags=$(grep -a "@@TEST_DURATION:" "$log_file" 2>/dev/null | sed 's/.*@@TEST_DURATION:\([^:]*\):.*/\1/' | sort -u)

  for tag in $tags; do
    # Sum all durations for this tag
    local total=0
    while IFS= read -r duration; do
      [ -z "$duration" ] && continue
      total=$((total + duration))
    done < <(grep -a "@@TEST_DURATION:${tag}:" "$log_file" 2>/dev/null | sed 's/.*@@TEST_DURATION:[^:]*:\([0-9]*\)@@.*/\1/')
    echo "${tag}|${total}"
  done
}

# Helper function to check for test failures in log file
check_test_failures() {
  local log_file="$1"
  local found_issues=0
  
  echo "Checking for test failures..."
  # Pattern to match actual test failures in Catch2 output
  # We're looking for lines that match the Catch2 test failure pattern
  # Typically these start with a file path and line number followed by "failed:"
  failed_lines=$(grep -a -n "\.cpp:[0-9]\+: failed:" "$log_file")
  
  if [ -n "$failed_lines" ]; then
    # Show the log file path and line number for each failure
    while IFS= read -r line; do
      line_num=$(echo "$line" | cut -d':' -f1)
      echo "[failed:] => $log_file:$line_num"
    done <<< "$failed_lines"
    
    found_issues=1
  else
    echo "No test failures found."
  fi
  
  return $found_issues
}

# Helper function to check for memory leaks in log file
check_memory_leaks() {
  local log_file="$1"
  local found_issues=0
  
  echo "Checking for memory leaks..."
  # Look for LEAK SUMMARY lines from Valgrind
  leak_lines=$(grep -a -n "LEAK SUMMARY:" "$log_file")
  
  if [ -n "$leak_lines" ]; then
    leak_found=0
    # For each LEAK SUMMARY line, check if it indicates actual leaks
    while IFS= read -r line; do
      line_num=$(echo "$line" | cut -d':' -f1)
      # Get the next few lines after the LEAK SUMMARY to check for actual leak details
      leak_context=$(tail -n +$line_num "$log_file" | head -n 5)
      
      # Check if there are any definite, possible, or indirect leaks
      if echo "$leak_context" | grep -q "definitely lost\|indirectly lost\|possibly lost" && \
         ! echo "$leak_context" | grep -q "definitely lost: 0 bytes\|indirectly lost: 0 bytes\|possibly lost: 0 bytes"; then
        # Find the source file and line number from nearby context if possible
        # For simplicity, we'll just use the log file and line number
        echo "[LEAK SUMMARY:] => $log_file:$line_num"
        found_issues=1
        leak_found=1
      fi
    done <<< "$leak_lines"
    
    if [ $leak_found -eq 0 ]; then
      echo "LEAK SUMMARY found but no actual memory leaks detected."
    fi
  else
    echo "No memory leak summaries found."
  fi
  
  return $found_issues
}

# Helper function to check for valgrind errors in log file
check_valgrind_errors() {
  local log_file="$1"
  local found_issues=0
  
  echo "Checking for valgrind errors..."
  # Extract error counts from ERROR SUMMARY lines
  error_lines=$(grep -a -n "ERROR SUMMARY:" "$log_file")
  
  if [ -n "$error_lines" ]; then
    found_error=0
    while IFS= read -r line; do
      line_num=$(echo "$line" | cut -d':' -f1)
      error_text=$(echo "$line" | cut -d':' -f2-)
      
      # Extract error and context counts
      if [[ "$error_text" =~ ([0-9]+)[[:space:]]+errors[[:space:]]+from[[:space:]]+([0-9]+)[[:space:]]+contexts ]]; then
        errors="${BASH_REMATCH[1]}"
        contexts="${BASH_REMATCH[2]}"
        
        # Only report if errors or contexts are greater than 0
        if [ "$errors" -gt 0 ] || [ "$contexts" -gt 0 ]; then
          echo "[ERROR SUMMARY: $errors errors from $contexts contexts] => $log_file:$line_num"
          found_issues=1
          found_error=1
        fi
      fi
    done <<< "$error_lines"
    
    if [ $found_error -eq 0 ]; then
      echo "ERROR SUMMARY found but no errors reported."
    fi
  else
    echo "No valgrind error summaries found."
  fi
  
  return $found_issues
}

# Helper function to format seconds as HH:MM:SS
format_duration() {
  local seconds=$1
  local hours=$((seconds / 3600))
  local minutes=$(((seconds % 3600) / 60))
  local secs=$((seconds % 60))
  printf "%02d:%02d:%02d" $hours $minutes $secs
}

# Helper function to display a single test execution table
# Parameters:
#   $1 - log_file
#   $2 - available_tests (pre-extracted)
#   $3 - executed_tests (pre-extracted for specific run)
#   $4 - max_tag_len
#   $5 - desc_col_width
#   $6 - relative_log_file
#   $7 - title (e.g., "Run 1" or "Total")
#   $8 - total_durations (optional, for total summary)
display_single_table() {
  local log_file="$1"
  local available_tests="$2"
  local executed_tests="$3"
  local max_tag_len="$4"
  local desc_col_width="$5"
  local relative_log_file="$6"
  local title="$7"
  local total_durations="${8:-}"

  echo ""
  echo "=== $title ==="

  # Calculate total time for this run
  local total_seconds=0

  # Process each available test for display
  while IFS= read -r test_line; do
    # Skip empty lines
    if [ -z "$test_line" ]; then
      continue
    fi

    # Split the line by the pipe character
    IFS='|' read -r test_name tag <<< "$test_line"

    # Default to not executed
    local executed="[Not Executed]"
    local log_location=""

    # Check if this tag was executed (format: tag|line_number|duration)
    while IFS= read -r executed_entry; do
      [ -z "$executed_entry" ] && continue
      # Split by pipe to get tag, line number, and duration
      local exec_tag=$(echo "$executed_entry" | cut -d'|' -f1)
      local exec_line=$(echo "$executed_entry" | cut -d'|' -f2)
      local exec_duration=$(echo "$executed_entry" | cut -d'|' -f3)

      if [ "$tag" = "$exec_tag" ]; then
        # Format duration as HH:MM:SS
        local duration_str=$(format_duration "${exec_duration:-0}")
        executed="[Executed ${duration_str}]"
        log_location=" => ${relative_log_file}:${exec_line}"
        total_seconds=$((total_seconds + exec_duration))
        break
      fi
    done <<< "$executed_tests"

    # For total summary, use total_durations if provided
    if [ -n "$total_durations" ]; then
      while IFS= read -r total_entry; do
        [ -z "$total_entry" ] && continue
        local total_tag=$(echo "$total_entry" | cut -d'|' -f1)
        local total_dur=$(echo "$total_entry" | cut -d'|' -f2)

        if [ "$tag" = "$total_tag" ]; then
          local duration_str=$(format_duration "${total_dur:-0}")
          executed="[Executed ${duration_str}]"
          log_location=""  # No specific line for total
          total_seconds=$((total_seconds + total_dur))
          break
        fi
      done <<< "$total_durations"
    fi

    # Format the output with optional log location for executed tests
    printf "  [%-${max_tag_len}s] %-${desc_col_width}s %s%s\n" "$tag" "$test_name" "$executed" "$log_location"
  done <<< "$available_tests"

  # Show total time for this table
  local total_str=$(format_duration "$total_seconds")
  echo "  ─────────────────────────────────────────────────────────────────────"
  printf "  Total time: %s (%d seconds)\n" "$total_str" "$total_seconds"
}

# Helper function to display test execution table (with per-run support)
display_test_execution_table() {
  local log_file="$1"

  # Convert absolute path to relative path from project root
  local project_root=$(pwd)
  local relative_log_file="${log_file#$project_root/}"
  # Add ./ prefix if it's a relative path without it
  if [[ "$relative_log_file" != /* ]] && [[ "$relative_log_file" != ./* ]]; then
    relative_log_file="./${relative_log_file}"
  fi

  # Get available tests
  local available_tests=$(extract_available_tests "$log_file")

  # If no available tests were found, show a message
  if [ -z "$available_tests" ]; then
    echo "  No test cases found in the log file."
    return
  fi

  # Count total runs
  local total_runs=$(count_total_runs "$log_file")

  echo "All available test cases (${total_runs} run(s) detected):"

  # First pass: find the longest tag and description
  local max_tag_len=0
  local max_desc_len=0

  while IFS= read -r test_line; do
    # Skip empty lines
    if [ -z "$test_line" ]; then
      continue
    fi

    # Split the line by the pipe character
    IFS='|' read -r test_name tag <<< "$test_line"

    # Calculate lengths
    local tag_len=${#tag}
    local desc_len=${#test_name}

    # Update max lengths if needed
    if [ $tag_len -gt $max_tag_len ]; then
      max_tag_len=$tag_len
    fi

    if [ $desc_len -gt $max_desc_len ]; then
      max_desc_len=$desc_len
    fi
  done <<< "$available_tests"

  # Add padding to the description column length (1 space after)
  local desc_col_width=$((max_desc_len + 1))

  # Display a table for each run
  for ((run=1; run<=total_runs; run++)); do
    local executed_tests=$(extract_executed_tests "$log_file" "$run")
    display_single_table "$log_file" "$available_tests" "$executed_tests" "$max_tag_len" "$desc_col_width" "$relative_log_file" "Run $run of $total_runs"
  done

  # If there are multiple runs, display total summary
  if [ "$total_runs" -gt 1 ]; then
    local total_durations=$(extract_total_durations "$log_file")
    # For total, we pass empty executed_tests and use total_durations instead
    display_single_table "$log_file" "$available_tests" "" "$max_tag_len" "$desc_col_width" "$relative_log_file" "TOTAL (All $total_runs runs)" "$total_durations"
  fi
}

# Synchronize VSCode IntelliSense with last build configuration
cmd_vscode() {
  local current_dir=$(pwd)
  local vscode_script="${current_dir}/.vscode/sync_intellisense.sh"
  local build_config="${current_dir}/build/.build_config"
  local generate_config_script="${current_dir}/libs/cpp_dbc/generate_build_config.sh"

  echo "🔄 Synchronizing VSCode IntelliSense with build configuration..."

  if [ ! -f "$vscode_script" ]; then
    echo "❌ Error: $vscode_script not found"
    echo "   Make sure you're in the project root directory"
    return 1
  fi

  # Ensure .build_config exists, regenerate if needed
  if [ ! -f "$build_config" ]; then
    echo "⚠️  build/.build_config not found, attempting to regenerate from CMakeCache.txt..."

    if [ ! -f "$generate_config_script" ]; then
      echo "❌ Error: $generate_config_script not found"
      echo "   Cannot regenerate build configuration"
      return 1
    fi

    # Make sure the script is executable
    if [ ! -x "$generate_config_script" ]; then
      chmod +x "$generate_config_script"
    fi

    # Call generate_build_config.sh without parameters (auto-detect from CMakeCache.txt)
    "$generate_config_script" || {
      echo "❌ Error: Failed to regenerate build configuration"
      echo "   Please run './build.sh' or './helper.sh --run-test' first"
      return 1
    }
    echo ""
  fi

  if [ ! -x "$vscode_script" ]; then
    chmod +x "$vscode_script"
  fi

  "$vscode_script" || return $?

  echo ""
  echo "✅ VSCode IntelliSense synchronized successfully!"
  echo "💡 Reload VSCode window to apply changes: Ctrl+Shift+P -> 'Developer: Reload Window'"

  return 0
}

# Main function to check test logs for specific patterns
cmd_check_test_log() {
  local log_file="$1"
  local current_dir=$(pwd)
  
  # If no log file is specified, find the most recent one in logs/test/
  if [ -z "$log_file" ]; then
    # Ensure the test log directory exists
    if [ ! -d "${current_dir}/logs/test" ]; then
      echo "Error: Test log directory ${current_dir}/logs/test does not exist."
      return 1
    fi
    
    # Find the most recent log file by name (which includes timestamp)
    # Sort by filename in reverse order to get the most recent timestamp first
    log_file=$(find "${current_dir}/logs/test" -name "output-*.log" | sort -r | head -n1)
    
    if [ -z "$log_file" ]; then
      echo "Error: No test log files found in ${current_dir}/logs/test/"
      return 1
    fi
    
    echo "Using most recent test log file: $log_file"
  else
    # Check if the specified log file exists
    if [ ! -f "$log_file" ]; then
      echo "Error: Specified log file '$log_file' does not exist."
      return 1
    fi
  fi
  
  # Set found_issues flag to track if any issues were found
  local found_issues=0
  local check_result=0
  
  echo "Checking test log file: $log_file"
  echo "----------------------------------------"
  
  # Display test execution table
  display_test_execution_table "$log_file"
  echo "----------------------------------------"
  
  # Check for test failures
  check_test_failures "$log_file"
  check_result=$?
  if [ $check_result -eq 1 ]; then
    found_issues=1
  fi
  echo "----------------------------------------"
  
  # Check for memory leaks
  check_memory_leaks "$log_file"
  check_result=$?
  if [ $check_result -eq 1 ]; then
    found_issues=1
  fi
  echo "----------------------------------------"
  
  # Check for valgrind errors
  check_valgrind_errors "$log_file"
  check_result=$?
  if [ $check_result -eq 1 ]; then
    found_issues=1
  fi
  
  echo "----------------------------------------"
  echo "Log check completed."
  
  # Return success regardless of whether issues were found
  # The found_issues flag can be used in the future if needed
  return 0
}

# Function to run benchmarks
cmd_run_benchmarks() {
  local ts=$(date '+%Y-%m-%d-%H-%M-%S_%z')
  # Get current directory
  local current_dir=$(pwd)
  # Ensure the benchmark log directory exists
  mkdir -p "${current_dir}/logs/benchmark"
  local log_file="${current_dir}/logs/benchmark/output-${ts}.log"
  echo "Running benchmarks. Output logging to $log_file."
  
  # Clean up old logs in the benchmark directory, keeping the 4 most recent
  cleanup_old_logs "${current_dir}/logs/benchmark" 4
  
  # Build command with options
  local run_benchmark_cmd="./libs/cpp_dbc/run_benchmarks_cpp_dbc.sh"
  
  # Flag to track if base-line option is present
  local create_baseline=false
  
  # First, extract all benchmark tags from the options string
  local benchmark_tags=""
  if [[ "$BENCHMARK_OPTIONS" =~ benchmark= ]]; then
    # Extract everything after benchmark= up to the next option with =
    local benchmark_part=$(echo "$BENCHMARK_OPTIONS" | grep -o "benchmark=[^=]*" | sed 's/,.*//')
    
    # If there are plus signs after benchmark=, we need to handle them specially
    if [[ "$BENCHMARK_OPTIONS" =~ benchmark=([^,]+) ]]; then
      # Get everything after benchmark= up to the next option with =
      local full_benchmark_part=$(echo "$BENCHMARK_OPTIONS" | grep -o "benchmark=[^=]*" | sed 's/,run=.*//')
      # Remove the "benchmark=" prefix
      benchmark_tags="${full_benchmark_part#benchmark=}"
    else
      # Simple case - just one tag
      benchmark_tags="${benchmark_part#benchmark=}"
    fi
    
    # Remove benchmark tags and any following benchmark tags from the options string
    BENCHMARK_OPTIONS=$(echo "$BENCHMARK_OPTIONS" | sed -E "s/benchmark=[^,]+//g")
    # Clean up any double commas that might have been created
    BENCHMARK_OPTIONS=$(echo "$BENCHMARK_OPTIONS" | sed -E "s/,,/,/g")
    # Remove trailing comma if present
    BENCHMARK_OPTIONS=$(echo "$BENCHMARK_OPTIONS" | sed -E "s/,$//g")
    
    echo "Debug: Extracted benchmark tags: '$benchmark_tags'"
    echo "Debug: Remaining options: '$BENCHMARK_OPTIONS'"
  fi
  
  # Process remaining comma-separated options
  if [ -n "$BENCHMARK_OPTIONS" ]; then
    # Split by comma
    IFS=',' read -ra OPTIONS <<< "$BENCHMARK_OPTIONS"
    
    # Check for specific database options
    for opt in "${OPTIONS[@]}"; do
      # Skip empty options
      if [[ -z "$opt" ]]; then
        continue
      fi
      
      case "$opt" in
        base-line)
          create_baseline=true
          echo "Will create benchmark baseline after running benchmarks"
          ;;
        memory-usage)
          run_benchmark_cmd="$run_benchmark_cmd --memory-usage"
          echo "Enabling memory usage tracking"
          ;;
        mysql)
          run_benchmark_cmd="$run_benchmark_cmd --mysql"
          echo "Running MySQL benchmarks only"
          ;;
        mysql-off)
          run_benchmark_cmd="$run_benchmark_cmd --mysql-off"
          echo "Disabling MySQL benchmarks"
          ;;
        postgres)
          run_benchmark_cmd="$run_benchmark_cmd --postgresql"
          echo "Running PostgreSQL benchmarks only"
          ;;
        sqlite)
          run_benchmark_cmd="$run_benchmark_cmd --sqlite"
          echo "Running SQLite benchmarks only"
          ;;
        firebird)
          run_benchmark_cmd="$run_benchmark_cmd --firebird"
          echo "Running Firebird benchmarks only"
          ;;
        mongodb)
          run_benchmark_cmd="$run_benchmark_cmd --mongodb"
          echo "Running MongoDB benchmarks only"
          ;;
        scylladb)
          run_benchmark_cmd="$run_benchmark_cmd --scylladb"
          echo "Running ScyllaDB benchmarks only"
          ;;
        redis)
            run_benchmark_cmd="$run_benchmark_cmd --redis"
            echo "Running Redis benchmarks only"
            ;;
        iterations=*)
            # Extract the number after iterations=
            local iterations_value="${opt#iterations=}"
            run_benchmark_cmd="$run_benchmark_cmd --iterations=$iterations_value"
            echo "Setting iterations to $iterations_value"
            ;;
        repetitions=*)
            # Extract the number after repetitions=
            local repetitions_value="${opt#repetitions=}"
            run_benchmark_cmd="$run_benchmark_cmd --repetitions=$repetitions_value"
            echo "Setting repetitions to $repetitions_value"
            ;;
        clean)
          run_benchmark_cmd="$run_benchmark_cmd --clean"
          echo "Cleaning build directories"
          ;;
        rebuild)
          run_benchmark_cmd="$run_benchmark_cmd --rebuild"
          ;;
        release)
          run_benchmark_cmd="$run_benchmark_cmd --release"
          echo "Building in release mode"
          ;;
        yaml)
          run_benchmark_cmd="$run_benchmark_cmd --yaml"
          echo "Enabling YAML support"
          ;;
        debug-pool)
          run_benchmark_cmd="$run_benchmark_cmd --debug-pool"
          echo "Enabling debug output for ConnectionPool"
          ;;
        debug-txmgr)
          run_benchmark_cmd="$run_benchmark_cmd --debug-txmgr"
          echo "Enabling debug output for TransactionManager"
          ;;
        debug-sqlite)
          run_benchmark_cmd="$run_benchmark_cmd --debug-sqlite"
          echo "Enabling debug output for SQLite driver"
          ;;
        debug-firebird)
          run_benchmark_cmd="$run_benchmark_cmd --debug-firebird"
          echo "Enabling debug output for Firebird driver"
          ;;
        debug-mongodb)
          run_benchmark_cmd="$run_benchmark_cmd --debug-mongodb"
          echo "Enabling debug output for MongoDB driver"
          ;;
        debug-scylladb)
          run_benchmark_cmd="$run_benchmark_cmd --debug-scylladb"
          echo "Enabling debug output for ScyllaDB driver"
          ;;
        debug-redis)
          run_benchmark_cmd="$run_benchmark_cmd --debug-redis"
          echo "Enabling debug output for Redis driver"
          ;;
        debug-all)
          run_benchmark_cmd="$run_benchmark_cmd --debug-all"
          echo "Enabling all debug output"
          ;;
        dw-off)
          run_benchmark_cmd="$run_benchmark_cmd --dw-off"
          echo "Disabling libdw support for stack traces"
          ;;
        db-driver-thread-safe-off)
          run_benchmark_cmd="$run_benchmark_cmd --db-driver-thread-safe-off"
          echo "Disabling thread-safe database driver operations"
          ;;
        *)
          echo "Warning: Unknown benchmark option: $opt"
          ;;
      esac
    done
  else
    echo "Running all benchmarks"
  fi
  
  # Add the benchmark tags if they exist
  if [ -n "$benchmark_tags" ]; then
    echo "Debug: Final benchmark tags: '$benchmark_tags'"
    run_benchmark_cmd="$run_benchmark_cmd --run-benchmark=$benchmark_tags"
    echo "Setting benchmark tags to: $benchmark_tags"
  fi
  
  echo "Running: $run_benchmark_cmd"
  
  # Check if terminal supports colors
  if check_color_support; then
    # Run with colors in terminal but without colors in log file
    # Use unbuffer to preserve colors in terminal output and sed to strip ANSI color codes from log file
    unbuffer $run_benchmark_cmd 2>&1 | tee >(sed -r "s/\x1B\[([0-9]{1,3}(;[0-9]{1,3})*)?[mGK]//g" > "$log_file")
  else
    # Terminal doesn't support colors, just run normally and strip any color codes that might be present
    $run_benchmark_cmd 2>&1 | tee >(sed -r "s/\x1B\[([0-9]{1,3}(;[0-9]{1,3})*)?[mGK]//g" > "$log_file")
  fi
  
  echo ""
  echo "Command runned: $run_benchmark_cmd"
  echo "Log file: $log_file."
  
  # If base-line option is present, run the create_benchmark_cpp_dbc_base_line.sh script
  if [ "$create_baseline" = true ]; then
    echo ""
    echo "Creating benchmark baseline..."
    local create_baseline_cmd="./libs/cpp_dbc/create_benchmark_cpp_dbc_base_line.sh --log-file=\"$log_file\""
    echo "Running: $create_baseline_cmd"
    
    # Execute the create_benchmark_cpp_dbc_base_line.sh script
    ./libs/cpp_dbc/create_benchmark_cpp_dbc_base_line.sh --log-file="$log_file"
    
    echo "Baseline creation completed."
  fi
}

get_bin_name() {
  if [ -f ".dist_build" ]; then
    BIN_NAME=$(awk -F'"' '/^Container_Bin_Name=/{print $2}' .dist_build)
  else
    BIN_NAME="cpp_dbc_demo"  # Default name if .dist_build is not found
  fi
  echo "$BIN_NAME"
}

cmd_ldd_bin() {
  local bin_name=$(get_bin_name)
  local bin_path="build/${bin_name}"
  
  if [ ! -f "$bin_path" ]; then
    echo "Error: Executable '$bin_path' not found. Build the project first."
    return 1
  fi
  
  echo "Running ldd on $bin_path:"
  ldd "$bin_path"
}

cmd_run_bin() {
  local bin_name=$(get_bin_name)
  local bin_path="build/${bin_name}"
  
  if [ ! -f "$bin_path" ]; then
    echo "Error: Executable '$bin_path' not found. Build the project first."
    return 1
  fi
  
  echo "Running $bin_path:"
  "$bin_path"
}

cmd_run_build_dist() {

  local ts=$(date '+%Y-%m-%d-%H-%M-%S_%z')
  # Get current directory
  local current_dir=$(pwd)
  # Ensure the build directory exists
  mkdir -p "${current_dir}/logs/build_dist"
  local log_file="${current_dir}/logs/build_dist/dist_output-${ts}.log"
  echo "Building Docker image. Output logging to $log_file."

  # Clean up old logs in the build_dist directory, keeping the 4 most recent
  cleanup_old_logs "${current_dir}/logs/build_dist" 4

  # Build command with options
  local build_cmd="./build.dist.sh"
  
  # Process comma-separated options
  IFS=',' read -ra OPTIONS <<< "$BUILD_DIST_OPTIONS"
  for opt in "${OPTIONS[@]}"; do
    case "$opt" in
      clean)
        build_cmd="$build_cmd --clean"
        echo "Cleaning build directories"
        ;;
      postgres)
        build_cmd="$build_cmd --postgres"
        echo "Enabling PostgreSQL support"
        ;;
      mysql)
        build_cmd="$build_cmd --mysql"
        echo "Enabling MySQL support"
        ;;
      mysql-off)
        build_cmd="$build_cmd --mysql-off"
        echo "Disabling MySQL support"
        ;;
      sqlite)
        build_cmd="$build_cmd --sqlite"
        echo "Enabling SQLite support"
        ;;
      firebird)
        build_cmd="$build_cmd --firebird"
        echo "Enabling Firebird SQL support"
        ;;
      mongodb)
        build_cmd="$build_cmd --mongodb"
        echo "Enabling MongoDB support"
        ;;
      scylladb)
        build_cmd="$build_cmd --scylladb"
        echo "Enabling ScyllaDB support"
        ;;
      redis)
        build_cmd="$build_cmd --redis"
        echo "Enabling Redis support"
        ;;
      yaml)
        build_cmd="$build_cmd --yaml"
        echo "Enabling YAML support"
        ;;
      test)
        build_cmd="$build_cmd --test"
        echo "Building tests"
        ;;
      examples)
        build_cmd="$build_cmd --examples"
        echo "Building examples"
        ;;
      release)
        build_cmd="$build_cmd --release"
        echo "Building release mode"
        ;;
      debug-pool)
        build_cmd="$build_cmd --debug-pool"
        echo "Enabling debug output for ConnectionPool"
        ;;
      debug-txmgr)
        build_cmd="$build_cmd --debug-txmgr"
        echo "Enabling debug output for TransactionManager"
        ;;
      debug-sqlite)
        build_cmd="$build_cmd --debug-sqlite"
        echo "Enabling debug output for SQLite driver"
        ;;
      debug-firebird)
        build_cmd="$build_cmd --debug-firebird"
        echo "Enabling debug output for Firebird driver"
        ;;
      debug-mongodb)
        build_cmd="$build_cmd --debug-mongodb"
        echo "Enabling debug output for MongoDB driver"
        ;;
      debug-scylladb)
        build_cmd="$build_cmd --debug-scylladb"
        echo "Enabling debug output for ScyllaDB driver"
        ;;
      debug-redis)
        build_cmd="$build_cmd --debug-redis"
        echo "Enabling debug output for Redis driver"
        ;;
      debug-all)
        build_cmd="$build_cmd --debug-all"
        echo "Enabling all debug output"
        ;;
      dw-off)
        build_cmd="$build_cmd --dw-off"
        echo "Disabling libdw support for stack traces"
        ;;
      db-driver-thread-safe-off)
        build_cmd="$build_cmd --db-driver-thread-safe-off"
        echo "Disabling thread-safe database driver operations"
        ;;
      *)
        echo "Warning: Unknown build option: $opt"
        ;;
    esac
  done

  echo "Running: $build_cmd"
  
  # Check if terminal supports colors
  if check_color_support; then
    # Run with colors in terminal but without colors in log file
    # Use unbuffer to preserve colors in terminal output and sed to strip ANSI color codes from log file
    unbuffer $build_cmd 2>&1 | tee >(sed -r "s/\x1B\[([0-9]{1,3}(;[0-9]{1,3})*)?[mGK]//g" > "$log_file")
  else
    # Terminal doesn't support colors, just run normally and strip any color codes that might be present
    $build_cmd 2>&1 | tee >(sed -r "s/\x1B\[([0-9]{1,3}(;[0-9]{1,3})*)?[mGK]//g" > "$log_file")
  fi

  echo ""
  echo "Command runned: $build_cmd"
  echo "Log file: $log_file."

}

cmd_run() {
  local name
  if ! get_container_name "$1" name; then
    echo "Cannot run container without valid name."
    exit 1
  fi
  docker run --rm "$name"
}

cmd_labels() {
  local name
  if ! get_container_name "$1" name; then
    echo "Cannot fetch labels without valid container name."
    exit 1
  fi
  echo "Image ID: $(docker images --no-trunc --quiet "$name" | head -n1)"
  docker inspect --format='{{range $k,$v := .Config.Labels}}{{$k}}={{$v}}{{println}}{{end}}' "$name"
}

cmd_tags() {
  local name
  if ! get_container_name "$1" name; then
    echo "Cannot fetch tags without valid container name."
    exit 1
  fi
  docker images --format 'Tag: {{.Tag}} (ID: {{.ID}})' "$name"
}

cmd_env() {
  local name
  if ! get_container_name "$1" name; then
    echo "Cannot fetch environment variables without valid container name."
    exit 1
  fi
  echo "Environment variables in image '$name':"
  docker inspect --format='{{range .Config.Env}}{{println .}}{{end}}' "$name"
}

cmd_ldd() {
  local name
  if ! get_container_name "$1" name; then
    echo "Cannot run ldd in container without valid name."
    exit 1
  fi
  
  # Get the container binary path from the image labels
  local bin_path=$(docker inspect --format='{{index .Config.Labels "path"}}' "$name" 2>/dev/null)
  local bin_name=$(get_bin_name)
  
  if [ -z "$bin_path" ]; then
    echo "Warning: Could not find binary path from container labels. Using default path."
    bin_path="/usr/local/bin/cpp_dbc"
  fi
  
  # Ensure bin_path ends with a slash
  if [[ "$bin_path" != */ ]]; then
    bin_path="${bin_path}/"
  fi
  
  local container_bin_path="${bin_path}${bin_name}"
  
  echo "Running ldd on $container_bin_path inside container $name:"
  docker run --rm --entrypoint ldd "$name" "$container_bin_path"
}


cmd_clean_conan_cache() {
  echo "Cleaning Conan cache..."
  conan remove "*" -c
  echo "Conan cache cleared."
}

if [[ $# -lt 1 ]]; then
  show_usage
  exit 1
fi

# Initialize variables
container_name=""
exit_code=0
USE_YAML="0"
BUILD_EXAMPLES="0"
BUILD_TEST="0"
USE_POSTGRES="0"
USE_SQLITE="0"
USE_MYSQL="1"
USE_VALGRIND="0"
USE_ASAN="0"
USE_CTEST="0"
TEST_OPTIONS=""
BUILD_DIST_OPTIONS=""

# Process arguments
args=("$@")
i=0
while [ $i -lt ${#args[@]} ]; do
  case "${args[$i]}" in
    --yaml)
      USE_YAML="1"
      echo "Setting YAML support to enabled"
      ;;
    --postgres)
      USE_POSTGRES="1"
      echo "Setting PostgreSQL support to enabled"
      ;;
    --sqlite)
      USE_SQLITE="1"
      echo "Setting SQLite support to enabled"
      ;;
    --mysql-off)
      USE_MYSQL="0"
      echo "Setting MySQL support to disabled"
      ;;
    --examples)
      BUILD_EXAMPLES="1"
      echo "Setting examples build to enabled"
      ;;
    --test)
      BUILD_TEST="1"
      echo "Setting test build to enabled"
      ;;
    --valgrind)
      USE_VALGRIND="1"
      echo "Setting Valgrind to enabled"
      ;;
    --asan)
      USE_ASAN="1"
      echo "Setting Address Sanitizer to enabled"
      ;;
    --ctest)
      USE_CTEST="1"
      echo "Setting CTest to enabled"
      ;;
    *)
      # Other arguments will be processed in the next loop
      ;;
  esac
  i=$((i+1))
done

# Process commands
i=0
while [ $i -lt ${#args[@]} ]; do
  case "${args[$i]}" in
    --run-build=*)
      BUILD_OPTIONS="${args[$i]#*=}"
      cmd_run_build
      ;;
    --run-build)
      cmd_run_build
      ;;
    --run-build-dist=*)
      BUILD_DIST_OPTIONS="${args[$i]#*=}"
      cmd_run_build_dist
      ;;
    --run-build-dist)
      cmd_run_build_dist
      ;;
    --run-ctr)
      i=$((i+1))
      if [ $i -lt ${#args[@]} ] && [[ "${args[$i]}" != --* ]]; then
        container_name="${args[$i]}"
        i=$((i+1))
      fi
      cmd_run "$container_name" || exit_code=$?
      ;;
    --show-labels-ctr)
      i=$((i+1))
      if [ $i -lt ${#args[@]} ] && [[ "${args[$i]}" != --* ]]; then
        container_name="${args[$i]}"
        i=$((i+1))
      fi
      cmd_labels "$container_name" || exit_code=$?
      ;;
    --show-tags-ctr)
      i=$((i+1))
      if [ $i -lt ${#args[@]} ] && [[ "${args[$i]}" != --* ]]; then
        container_name="${args[$i]}"
        i=$((i+1))
      fi
      cmd_tags "$container_name" || exit_code=$?
      ;;
    --show-env-ctr)
      i=$((i+1))
      if [ $i -lt ${#args[@]} ] && [[ "${args[$i]}" != --* ]]; then
        container_name="${args[$i]}"
        i=$((i+1))
      fi
      cmd_env "$container_name" || exit_code=$?
      ;;
    --clean-conan-cache)
      cmd_clean_conan_cache
      ;;
    --run-test=*)
      TEST_OPTIONS="${args[$i]#*=}"
      cmd_run_test
      ;;
    --run-test)
      cmd_run_test
      ;;
    --ldd-bin-ctr)
      i=$((i+1))
      if [ $i -lt ${#args[@]} ] && [[ "${args[$i]}" != --* ]]; then
        container_name="${args[$i]}"
        i=$((i+1))
      fi
      cmd_ldd "$container_name" || exit_code=$?
      ;;
    --ldd-build-bin)
      cmd_ldd_bin || exit_code=$?
      ;;
    --run-build-bin)
      cmd_run_bin || exit_code=$?
      ;;
    --mc-combo-01)
      # Equivalent to --run-build=clean,postgres,mysql,sqlite,firebird,mongodb,scylladb,redis,yaml,test,examples
      BUILD_OPTIONS="clean,postgres,mysql,sqlite,firebird,mongodb,scylladb,redis,yaml,test,examples"
      cmd_run_build
      ;;
    --mc-combo-02)
      # Equivalent to --run-build=postgres,sqlite,mysql,scylladb,redis,yaml,test,examples
      BUILD_OPTIONS="postgres,mysql,sqlite,firebird,mongodb,scylladb,redis,yaml,test,examples"
      cmd_run_build
      ;;
    --bk-combo-01)
      # Equivalent to --run-test=rebuild,sqlite,postgres,mysql,firebird,mongodb,scylladb,redis,yaml,valgrind,auto,run=1
      TEST_OPTIONS="rebuild,sqlite,postgres,mysql,firebird,mongodb,scylladb,redis,yaml,valgrind,auto,run=1"
      cmd_run_test
      ;;
    --bk-combo-02)
      # Equivalent to --run-test=rebuild,sqlite,postgres,mysql,firebird,mongodb,scylladb,redis,yaml,auto,run=1
      TEST_OPTIONS="rebuild,sqlite,postgres,mysql,firebird,mongodb,scylladb,redis,yaml,auto,run=1"
      cmd_run_test
      ;;
    --bk-combo-03)
      # Equivalent to --run-test=rebuild,sqlite,postgres,mysql,firebird,mongodb,scylladb,redis,yaml,valgrind,auto,run=5
      TEST_OPTIONS="clean,rebuild,sqlite,postgres,mysql,firebird,mongodb,scylladb,redis,yaml,valgrind,auto,run=5"
      cmd_run_test
      ;;
    --bk-combo-04)
      # Equivalent to --run-test=rebuild,sqlite,postgres,mysql,firebird,mongodb,scylladb,redis,yaml,auto,run=5
      TEST_OPTIONS="clean,rebuild,sqlite,postgres,mysql,firebird,mongodb,scylladb,redis,yaml,auto,run=5"
      cmd_run_test
      ;;
    --bk-combo-05)
      # Equivalent to --run-test=rebuild,sqlite,postgres,mysql,firebird,mongodb,scylladb,redis,yaml,valgrind,auto,run=1
      TEST_OPTIONS="clean,rebuild,sqlite,postgres,mysql,firebird,mongodb,scylladb,redis,yaml,valgrind,auto,run=1"
      cmd_run_test
      ;;
    --bk-combo-06)
      # Equivalent to --run-test=rebuild,sqlite,postgres,mysql,firebird,mongodb,scylladb,redis,yaml,auto,run=1
      TEST_OPTIONS="clean,rebuild,sqlite,postgres,mysql,firebird,mongodb,scylladb,redis,yaml,auto,run=1"
      cmd_run_test
      ;;
    --bk-combo-07)
      # Equivalent to --run-test=sqlite,postgres,mysql,firebird,mongodb,scylladb,redis,yaml,valgrind,auto,run=1
      TEST_OPTIONS="sqlite,postgres,mysql,firebird,mongodb,scylladb,redis,yaml,valgrind,auto,run=1"
      cmd_run_test
      ;;
    --bk-combo-08)
      # Equivalent to --run-test=sqlite,postgres,mysql,firebird,mongodb,scylladb,redis,yaml,auto,run=1
      TEST_OPTIONS="sqlite,postgres,mysql,firebird,mongodb,scylladb,redis,yaml,auto,run=1"
      cmd_run_test
      ;;
    --bk-combo-09)
      # Equivalent to --run-test=clean,rebuild,sqlite,postgres,mysql,firebird,mongodb,scylladb,redis,yaml,valgrind,auto,progress,run=1
      TEST_OPTIONS="clean,rebuild,sqlite,postgres,mysql,firebird,mongodb,scylladb,redis,yaml,valgrind,auto,progress,run=1"
      cmd_run_test
      ;;
    --bk-combo-10)
      # Equivalent to --run-test=rebuild,sqlite,postgres,mysql,firebird,mongodb,scylladb,redis,yaml,valgrind,auto,progress,run=5
      TEST_OPTIONS="rebuild,sqlite,postgres,mysql,firebird,mongodb,scylladb,redis,yaml,valgrind,auto,progress,run=5"
      cmd_run_test
      ;;
    --bk-combo-11)
      # Equivalent to --run-test=clean,rebuild,sqlite,postgres,mysql,firebird,mongodb,scylladb,redis,yaml,valgrind,auto,progress,parallel=5,run=1
      TEST_OPTIONS="clean,rebuild,sqlite,postgres,mysql,firebird,mongodb,scylladb,redis,yaml,valgrind,auto,progress,parallel=5,run=1"
      cmd_run_test
      ;;
    --bk-combo-12)
      # Equivalent to --run-test=rebuild,sqlite,postgres,mysql,firebird,mongodb,scylladb,redis,yaml,valgrind,auto,progress,parallel=5,run=5
      TEST_OPTIONS="rebuild,sqlite,postgres,mysql,firebird,mongodb,scylladb,redis,yaml,valgrind,auto,progress,parallel=5,run=5"
      cmd_run_test
      ;;
    --kfc-combo-01)
      # Equivalent to --run-build-dist=clean,rebuild,sqlite,postgres,mysql,firebird,mongodb,scylladb,redis,yaml,test,examples
      BUILD_DIST_OPTIONS="clean,rebuild,sqlite,postgres,mysql,firebird,mongodb,scylladb,redis,yaml,test,examples"
      cmd_run_build_dist
      ;;
    --kfc-combo-02)
      # Equivalent to --run-build-dist=rebuild,sqlite,postgres,mysql,firebird,mongodb,scylladb,redis,yaml,test,examples
      BUILD_DIST_OPTIONS="rebuild,sqlite,postgres,mysql,firebird,mongodb,scylladb,redis,yaml,test,examples"
      cmd_run_build_dist
      ;;
    --dk-combo-01)
      # Equivalent to --run-benchmarks=rebuild,sqlite,postgres,mysql,firebird,mongodb,scylladb,redis,yaml,memory-usage,base-line
      BENCHMARK_OPTIONS="rebuild,sqlite,postgres,mysql,firebird,mongodb,scylladb,redis,yaml,memory-usage,base-line"
      cmd_run_benchmarks
      ;;
    --dk-combo-02)
      # Equivalent to --run-benchmarks=clean,rebuild,sqlite,postgres,mysql,firebird,mongodb,scylladb,redis,yaml,memory-usage,base-line
      BENCHMARK_OPTIONS="clean,rebuild,sqlite,postgres,mysql,firebird,mongodb,scylladb,redis,yaml,memory-usage,base-line"
      cmd_run_benchmarks
      ;;
    --vscode)
      cmd_vscode || exit_code=$?
      ;;
    --check-test-log=*)
      log_file="${args[$i]#*=}"
      cmd_check_test_log "$log_file" || exit_code=$?
      ;;
    --check-test-log)
      cmd_check_test_log "" || exit_code=$?
      ;;
    --run-benchmarks=*)
      BENCHMARK_OPTIONS="${args[$i]#*=}"
      cmd_run_benchmarks
      ;;
    --run-benchmarks)
      cmd_run_benchmarks
      ;;
    *)
      echo "Unknown option: ${args[$i]}"
      show_usage
      exit 1
      ;;
  esac
  i=$((i+1))
done

exit $exit_code
