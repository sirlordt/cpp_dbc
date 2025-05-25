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
  echo "  --build              Build via ./build.sh, logs to build/build-<timestamp>.log"
  echo "  --build-dist         Build via ./build.dist.sh, logs to build/build-dist-<timestamp>.log"
  echo "  --run [name]         Run container"
  echo "  --labels [name]      Show labels with Image ID"
  echo "  --tags [name]        Show tags with Image ID"
  echo "  --env [name]         Show environment variables defined in the image"
  echo "  --clean-build        Remove build directory"
  echo "  --clean-conan-cache  Clear Conan local cache"
  echo "  --test               Build the tests"
  echo "  --run-test           Build (if needed) and run the tests"
  echo "  --ldd                Run ldd on the final executable"
  echo "  --run-bin            Run the final executable"
  echo ""
  echo "Multiple commands can be combined, e.g.:"
  echo "  $(basename "$0") --clean-build --clean-conan-cache --build"
}

cleanup_old_logs() {
  local logs=($(ls -1t build/*.log 2>/dev/null))
  if (( ${#logs[@]} > 4 )); then
    printf '%s\n' "${logs[@]:4}" | xargs rm -f --
  fi
}

cmd_build() {
  local ts=$(date '+%Y-%m-%d-%H-%M-%S_%z')
  local log="build/build-${ts}.log"
  echo "Building project. Output logging to $log."
  mkdir -p build
  cleanup_old_logs
  ./build.sh 2>&1 | tee "$log"
}

cmd_test() {
  echo "Building tests..."
  ./build.sh --test
}

cmd_run_test() {
  echo "Running tests..."
  ./run_test.sh
}

get_bin_name() {
  if [ -f ".dist_build" ]; then
    BIN_NAME=$(awk -F'"' '/^Container_Bin_Name=/{print $2}' .dist_build)
  else
    BIN_NAME="cpp_dbc_demo"  # Default name if .dist_build is not found
  fi
  echo "$BIN_NAME"
}

cmd_ldd() {
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

cmd_build_dist() {
  local ts=$(date '+%Y-%m-%d-%H-%M-%S_%z')
  local log="build/build-dist-${ts}.log"
  echo "Building Docker image. Output logging to $log."
  mkdir -p build
  cleanup_old_logs
  ./build.dist.sh 2>&1 | tee "$log"
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

cmd_clean_build() {
  echo "Removing build directory..."
  rm -rf build
  echo "Build directory removed."
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

# Process multiple commands
container_name=""
exit_code=0

while [[ $# -gt 0 ]]; do
  case "$1" in
    --build)
      cmd_build
      ;;
    --build-dist)
      cmd_build_dist
      ;;
    --run)
      shift
      if [[ "$1" != --* && -n "$1" ]]; then
        container_name="$1"
        shift
      fi
      cmd_run "$container_name" || exit_code=$?
      ;;
    --labels)
      shift
      if [[ "$1" != --* && -n "$1" ]]; then
        container_name="$1"
        shift
      fi
      cmd_labels "$container_name" || exit_code=$?
      ;;
    --tags)
      shift
      if [[ "$1" != --* && -n "$1" ]]; then
        container_name="$1"
        shift
      fi
      cmd_tags "$container_name" || exit_code=$?
      ;;
    --env)
      shift
      if [[ "$1" != --* && -n "$1" ]]; then
        container_name="$1"
        shift
      fi
      cmd_env "$container_name" || exit_code=$?
      ;;
    --clean-build)
      cmd_clean_build
      ;;
    --clean-conan-cache)
      cmd_clean_conan_cache
      ;;
    --test)
      cmd_test
      ;;
    --run-test)
      cmd_run_test
      ;;
    --ldd)
      cmd_ldd || exit_code=$?
      ;;
    --run-bin)
      cmd_run_bin || exit_code=$?
      ;;
    *)
      echo "Unknown option: $1"
      show_usage
      exit 1
      ;;
  esac
  shift
done

exit $exit_code
