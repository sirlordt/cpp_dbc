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
  echo "                                              yaml,auto,asan,ctest,check,run=N,test=Tag1+Tag2+Tag3,"
  echo "                                              debug-pool,debug-txmgr,debug-sqlite,debug-firebird,debug-mongodb,debug-scylladb,debug-redis,debug-all,dw-off,db-driver-thread-safe-off"
  echo "                           Example: --run-test=rebuild,sqlite,valgrind,run=3,test=integration+mysql_real_right_join"
  echo "                           Example: --run-test=rebuild,sqlite,debug-pool,debug-sqlite,test=integration"
  echo "                           Example: --run-test=clean,rebuild,sqlite,mysql,postgres,yaml,valgrind,auto,run=1"
  echo "                           Note: Multiple test tags can be specified using + as separator after test="
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

}

cmd_run_test() {
  local ts=$(date '+%Y-%m-%d-%H-%M-%S_%z')
  # Get current directory
  local current_dir=$(pwd)
  # Ensure the test log directory exists
  mkdir -p "${current_dir}/logs/test"
  local log_file="${current_dir}/logs/test/output-${ts}.log"
  echo "Running tests. Output logging to $log_file."
  
  # Clean up old logs in the test directory, keeping the 4 most recent
  cleanup_old_logs "${current_dir}/logs/test" 4
  
  # Build command with options
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
  
  echo "$0 => running: $run_test_cmd"
  
  # Check if terminal supports colors
  if check_color_support; then
    # Run with colors in terminal but without colors in log file
    # Use unbuffer to preserve colors in terminal output and sed to strip ANSI color codes from log file
    unbuffer $run_test_cmd 2>&1 | tee >(sed -r "s/\x1B\[([0-9]{1,3}(;[0-9]{1,3})*)?[mGK]//g" > "$log_file")
  else
    # Terminal doesn't support colors, just run normally and strip any color codes that might be present
    $run_test_cmd 2>&1 | tee >(sed -r "s/\x1B\[([0-9]{1,3}(;[0-9]{1,3})*)?[mGK]//g" > "$log_file")
  fi
  
  echo ""
  echo "Command runned: $run_test_cmd"
  echo "Log file: $log_file."
  
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

# Helper function to extract executed test cases from log file
extract_executed_tests() {
  local log_file="$1"
  
  # Create a temporary file to store the executed tags
  local temp_file=$(mktemp)
  
  # Extract all filter tags from the log file
  grep -a -o "Filters: \[[^]]*\]" "$log_file" | sed 's/Filters: \[\([^]]*\)\]/\1/' > "$temp_file"

  # For each tag, check if there are any passed or failed tests
  while IFS= read -r tag; do
    # Look for lines with passed/failed after the Filters line for this tag
    if grep -a -A 100 "Filters: \[$tag\]" "$log_file" | grep -a -q "\.cpp:[0-9]\+: passed:\|\.cpp:[0-9]\+: failed:"; then
      echo "$tag"
    fi
  done < "$temp_file"
  
  # Clean up
  rm -f "$temp_file"
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

# Helper function to display test execution table
display_test_execution_table() {
  local log_file="$1"
  
  echo "All available test cases:"
  
  # Get available tests
  local available_tests=$(extract_available_tests "$log_file")
  
  # Get executed tests
  local executed_tests=$(extract_executed_tests "$log_file")
  
  # If no available tests were found, show a message
  if [ -z "$available_tests" ]; then
    echo "  No test cases found in the log file."
    return
  fi
  
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
  
  # Add padding to the lengths
  # For tag column: tag length + 3 (2 for brackets and 1 for space after)
  tag_col_width=$((max_tag_len + 3))
  
  # For description column: description length + 1 (1 space after)
  desc_col_width=$((max_desc_len + 1))
  
  # Calculate total width of first two columns
  total_width=$((tag_col_width + desc_col_width))
  
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
    
    # Check if this tag was executed
    while IFS= read -r executed_tag; do
      if [ "$tag" = "$executed_tag" ]; then
        executed="[Executed]"
        break
      fi
    done <<< "$executed_tests"
    
    # Format the output to match the example in the task description
    printf "  [%-${max_tag_len}s] %-${desc_col_width}s %s\n" "$tag" "$test_name" "$executed"
  done <<< "$available_tests"
}

# Synchronize VSCode IntelliSense with last build configuration
cmd_vscode() {
  local current_dir=$(pwd)
  local vscode_script="${current_dir}/.vscode/sync_intellisense.sh"

  echo "🔄 Synchronizing VSCode IntelliSense with build configuration..."

  if [ ! -f "$vscode_script" ]; then
    echo "❌ Error: $vscode_script not found"
    echo "   Make sure you're in the project root directory"
    return 1
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
