#!/bin/bash

# Script to run the config_integration_example

# Get the directory of this script
SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"

# Go to the script directory
cd "$SCRIPT_DIR"

# Check if the example was built
if [ ! -f "../build/examples/config_integration_example" ]; then
    echo "Example not built. Building the library with examples and YAML support..."
    cd ..
    ./build_cpp_dbc.sh --examples --yaml
    cd "$SCRIPT_DIR"
    
    # Check again if the example was built
    if [ ! -f "../build/examples/config_integration_example" ]; then
        echo "Failed to build the example. Please build the library with examples and YAML support:"
        echo "./build_cpp_dbc.sh --examples --yaml"
        exit 1
    fi
fi

echo "Running config_integration_example with example_config.yml..."
../build/examples/config_integration_example example_config.yml