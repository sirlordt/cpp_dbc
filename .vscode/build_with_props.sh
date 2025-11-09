#!/bin/bash

# Script to extract defines from c_cpp_properties.json and pass them to build.sh

# Check if jq is installed
if ! command -v jq &> /dev/null; then
    echo "Error: jq is required but not installed."
    echo "Please install jq or run build.sh directly with the desired options."
    exit 1
fi

# Extract defines from c_cpp_properties.json
DEFINES=$(jq -r '.configurations[0].defines[]' .vscode/c_cpp_properties.json)

# Initialize build arguments
BUILD_ARGS=""

# Process each define
for DEFINE in $DEFINES; do
    # Extract name and value
    NAME=$(echo $DEFINE | cut -d'=' -f1)
    VALUE=$(echo $DEFINE | cut -d'=' -f2)
    
    # Convert to build.sh arguments
    if [ "$NAME" = "USE_MYSQL" ] && [ "$VALUE" = "1" ]; then
        BUILD_ARGS="$BUILD_ARGS --mysql"
    elif [ "$NAME" = "USE_POSTGRESQL" ] && [ "$VALUE" = "1" ]; then
        BUILD_ARGS="$BUILD_ARGS --postgres"
    fi
done

# Run build.sh with the arguments
echo "Running: ./build.sh $BUILD_ARGS"
./build.sh $BUILD_ARGS