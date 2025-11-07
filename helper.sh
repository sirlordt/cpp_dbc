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
  echo "                           Available options: clean,release,postgres,mysql,sqlite,yaml,test,examples"
  echo "                           Example: --run-build=clean,sqlite,yaml,test"
  echo "  --run-build-dist         Build via ./build.dist.sh, logs to build/run-build-dist-<timestamp>.log"
  echo "  --run-build-dist=OPTIONS Build dist with comma-separated options"
  echo "                           Available options: clean,release,postgres,mysql,sqlite,yaml,test,examples"
  echo "                           Example: --run-build-dist=clean,sqlite,yaml,test"
  echo "  --run-ctr [name]         Run container"
  echo "  --show-labels-ctr [name] Show labels with Image ID"
  echo "  --show-tags-ctr [name]   Show tags with Image ID"
  echo "  --show-env-ctr [name]    Show environment variables defined in the image"
  echo "  --clean-conan-cache      Clear Conan local cache"
  echo "  --run-test               Build (if needed) and run the tests"
  echo "  --run-test=OPTIONS       Run tests with comma-separated options"
  echo "                           Available options: clean,release,rebuild,sqlite,mysql,postgres,valgrind,yaml,auto,asan,ctest,check,run=N"
  echo "                           Example: --run-test=rebuild,sqlite,valgrind,run=3"
  echo "  --ldd-bin-ctr [name]     Run ldd on the executable inside the container"
  echo "  --ldd-build-bin          Run ldd on the final local build/ executable"
  echo "  --run-build-bin          Run the final local build/ executable"
  echo "  --bk-combo-01            Equivalent to --run-test=rebuild,sqlite,postgres,mysql,yaml,valgrind,auto,run=1"
  echo "  --bk-combo-02            Equivalent to --run-test=rebuild,sqlite,postgres,mysql,yaml,auto,run=1"
  echo "  --bk-combo-03            Equivalent to --run-test=sqlite,postgres,mysql,yaml,valgrind,auto,run=1"
  echo "  --bk-combo-04            Equivalent to --run-test=sqlite,postgres,mysql,yaml,auto,run=1"
  echo "  --mc-combo-01            Equivalent to --run-build=clean,postgres,mysql,sqlite,yaml,test,examples"
  echo "  --mc-combo-02            Equivalent to --run-build=postgres,sqlite,mysql,yaml,test,examples"
  echo "  --kfc-combo-01           Equivalent to --run-build-dist=clean,postgres,mysql,sqlite,yaml,test,examples"
  echo "  --kfc-combo-02           Equivalent to --run-build-dist=postgres,sqlite,mysql,yaml,test,examples"
  echo ""
  echo "Multiple commands can be combined, e.g.:"
  echo "  $(basename "$0") --clean-conan-cache --run-build"
}

cleanup_old_logs() {
  local logs=($(ls -1t build/*.log 2>/dev/null))
  if (( ${#logs[@]} > 4 )); then
    printf '%s\n' "${logs[@]:4}" | xargs rm -f --
  fi
}

cmd_run_build() {
  local ts=$(date '+%Y-%m-%d-%H-%M-%S_%z')
  local log="build/run-build-${ts}.log"
  echo "Building project. Output logging to $log."
  mkdir -p build
  cleanup_old_logs
  
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
        echo "Building in release mode"
        ;;
      *)
        echo "Warning: Unknown build option: $opt"
        ;;
    esac
  done

  echo "Running: $build_cmd"
  $build_cmd 2>&1 | tee "$log"
}

cmd_run_test() {
  echo "Running tests..."
  
  # Build command with options
  local run_test_cmd="./run_test.sh"
  
  # Process comma-separated options
  IFS=',' read -ra OPTIONS <<< "$TEST_OPTIONS"
  for opt in "${OPTIONS[@]}"; do
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
        release)
          run_test_cmd="$run_test_cmd --release"
          echo "Building in release mode"
          ;;
        *)
          echo "Warning: Unknown test option: $opt"
          ;;
      esac
    fi
  done
  
  echo "Running: $run_test_cmd"
  $run_test_cmd
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
  local log="build/run-build-dist-${ts}.log"
  echo "Building Docker image. Output logging to $log."
  mkdir -p build
  cleanup_old_logs
  
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
      *)
        echo "Warning: Unknown build option: $opt"
        ;;
    esac
  done

  echo "Running: $build_cmd"
  $build_cmd 2>&1 | tee "$log"
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
    --yaml|--examples|--test|--postgres|--sqlite|--mysql-off|--valgrind|--asan|--ctest)
      cmd_run_build
      ;;
    --mc-combo-01)
      # Equivalent to --run-build=clean,postgres,mysql,sqlite,yaml,test,examples
      BUILD_OPTIONS="clean,postgres,mysql,sqlite,yaml,test,examples"
      cmd_run_build
      ;;
    --mc-combo-02)
      # Equivalent to --run-build=postgres,sqlite,mysql,yaml,test,examples
      BUILD_OPTIONS="postgres,sqlite,mysql,yaml,test,examples"
      cmd_run_build
      ;;
    --bk-combo-01)
      # Equivalent to --run-test=rebuild,sqlite,postgres,mysql,yaml,valgrind,auto,run=1
      TEST_OPTIONS="rebuild,sqlite,postgres,mysql,yaml,valgrind,auto,run=1"
      cmd_run_test
      ;;
    --bk-combo-02)
      # Equivalent to --run-test=rebuild,sqlite,postgres,mysql,yaml,auto,run=1
      TEST_OPTIONS="rebuild,sqlite,postgres,mysql,yaml,auto,run=1"
      cmd_run_test
      ;;
    --bk-combo-03)
      # Equivalent to --run-test=sqlite,postgres,mysql,yaml,valgrind,auto,run=1
      TEST_OPTIONS="sqlite,postgres,mysql,yaml,valgrind,auto,run=1"
      cmd_run_test
      ;;
    --bk-combo-04)
      # Equivalent to --run-test=sqlite,postgres,mysql,yaml,auto,run=1
      TEST_OPTIONS="sqlite,postgres,mysql,yaml,auto,run=1"
      cmd_run_test
      ;;
    --kfc-combo-01)
      # Equivalent to --run-build-dist=clean,postgres,mysql,sqlite,yaml,test,examples
      BUILD_DIST_OPTIONS="clean,postgres,mysql,sqlite,yaml,test,examples"
      cmd_run_build_dist
      ;;
    --kfc-combo-02)
      # Equivalent to --run-build-dist=postgres,sqlite,mysql,yaml,test,examples
      BUILD_DIST_OPTIONS="postgres,sqlite,mysql,yaml,test,examples"
      cmd_run_build_dist
      ;;
    --mc-combo-02)
      # Equivalent to --run-build=postgres,sqlite,yaml,test,examples
      BUILD_OPTIONS="postgres,sqlite,yaml,test,examples"
      cmd_run_build
      ;;
    --bk-combo-01)
      # Equivalent to --run-test=rebuild,sqlite,postgres,mysql,yaml,valgrind,auto,run=1
      TEST_OPTIONS="rebuild,sqlite,postgres,mysql,yaml,valgrind,auto,run=1"
      cmd_run_test
      ;;
    --bk-combo-02)
      # Equivalent to --run-test=rebuild,sqlite,postgres,mysql,yaml,auto,run=1
      TEST_OPTIONS="rebuild,sqlite,postgres,mysql,yaml,auto,run=1"
      cmd_run_test
      ;;
    --bk-combo-03)
      # Equivalent to --run-test=sqlite,postgres,mysql,yaml,auto,run=1
      TEST_OPTIONS="sqlite,postgres,mysql,yaml,auto,run=1"
      cmd_run_test
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
