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
  echo "Usage: $(basename "$0") COMMAND [container_name]"
  echo "Commands:"
  echo "  --build              Build via ./build.sh, logs to build/build-<timestamp>.log"
  echo "  --build-dist         Build via ./build.dist.sh, logs to build/build-dist-<timestamp>.log"
  echo "  --run [name]         Run container"
  echo "  --labels [name]      Show labels with Image ID"
  echo "  --tags [name]        Show tags with Image ID"
  echo "  --env [name]         Show environment variables defined in the image"
  echo "  --clean-build        Remove build directory"
  echo "  --clean-conan-cache  Clear Conan local cache"
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

case "$1" in
  --build)             shift; cmd_build "$@" ;;
  --build-dist)        shift; cmd_build_dist "$@" ;;
  --run)               shift; cmd_run "$@" ;;
  --labels)            shift; cmd_labels "$@" ;;
  --tags)              shift; cmd_tags "$@" ;;
  --env)               shift; cmd_env "$@" ;;
  --clean-build)       shift; cmd_clean_build ;;
  --clean-conan-cache) shift; cmd_clean_conan_cache ;;
  *)                   show_usage; exit 1 ;;
esac
