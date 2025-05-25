#!/bin/bash

# Script to run the configuration example

# Get the directory of this script
SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )"
PROJECT_ROOT="$( cd "${SCRIPT_DIR}/../../.." &> /dev/null && pwd )"

# Check if the config example executable exists
CONFIG_EXAMPLE="${PROJECT_ROOT}/build/libs/cpp_dbc/build/config_example"

# Debug information
echo "Looking for executable at: ${CONFIG_EXAMPLE}"
echo "PROJECT_ROOT is: ${PROJECT_ROOT}"
if [ ! -f "${CONFIG_EXAMPLE}" ]; then
    echo "Error: config_example executable not found at ${CONFIG_EXAMPLE}"
    echo "Please build the library with examples first:"
    echo "  ./build_cpp_dbc.sh --examples --yaml"
    exit 1
fi

# Run the example with the example configuration file
echo "Running configuration example with example_config.yml..."
"${CONFIG_EXAMPLE}" "${SCRIPT_DIR}/example_config.yml"

echo ""
echo "Example completed."