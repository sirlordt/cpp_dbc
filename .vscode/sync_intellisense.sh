#!/bin/bash
# Script to synchronize IntelliSense with the last build configuration
# Reads build/.build_config and updates c_cpp_properties.json accordingly
# This script is fully dynamic - it processes ALL variables from .build_config

set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"
BUILD_CONFIG="$PROJECT_ROOT/build/.build_config"
CPP_PROPERTIES="$SCRIPT_DIR/c_cpp_properties.json"
COMPILE_COMMANDS="$PROJECT_ROOT/compile_commands.json"

echo "🔄 Synchronizing IntelliSense with build configuration..."

# Check if build config exists
if [ ! -f "$BUILD_CONFIG" ]; then
    echo "❌ Error: build/.build_config not found"
    echo "   Please run './build.sh' first to generate the configuration"
    exit 1
fi

# Load build configuration
echo "📖 Reading build configuration from build/.build_config..."

# Read all variables from .build_config (ignoring comments and empty lines)
DEFINES=""
ENABLED_DRIVERS=()

while IFS='=' read -r var_name var_value; do
    # Skip comments and empty lines
    [[ "$var_name" =~ ^#.*$ ]] && continue
    [[ -z "$var_name" ]] && continue

    # Remove any whitespace
    var_name=$(echo "$var_name" | xargs)
    var_value=$(echo "$var_value" | xargs)

    # Skip if empty after trimming
    [[ -z "$var_name" ]] && continue

    # Convert ON/OFF to 1/0 for defines
    if [ "$var_value" = "ON" ]; then
        numeric_value=1
        # Track enabled drivers
        if [[ "$var_name" =~ ^USE_.*$ ]]; then
            ENABLED_DRIVERS+=("${var_name#USE_}")
        fi
    elif [ "$var_value" = "OFF" ]; then
        numeric_value=0
    else
        # Keep other values as-is (like Debug/Release)
        numeric_value="$var_value"
    fi

    # Build defines array
    if [ -z "$DEFINES" ]; then
        DEFINES="\"$var_name=$numeric_value\""
    else
        DEFINES="$DEFINES,\n                \"$var_name=$numeric_value\""
    fi

    echo "  $var_name=$var_value"
done < <(grep -v '^#' "$BUILD_CONFIG" | grep -v '^[[:space:]]*$')

echo ""

# Detect include paths automatically
echo "🔍 Detecting include paths for enabled drivers..."
INCLUDE_PATHS_JSON=$("$SCRIPT_DIR/detect_include_paths.sh")

# Create c_cpp_properties.json if it doesn't exist
if [ ! -f "$CPP_PROPERTIES" ]; then
    echo "📝 Creating $CPP_PROPERTIES from template..."
    cat > "$CPP_PROPERTIES" << 'EOF'
{
    "configurations": [
        {
            "name": "Linux",
            "includePath": [],
            "compileCommands": "${workspaceFolder}/compile_commands.json",
            "configurationProvider": "ms-vscode.cmake-tools",
            "defines": [],
            "compilerPath": "/usr/bin/g++",
            "cStandard": "c17",
            "cppStandard": "c++23",
            "intelliSenseMode": "linux-gcc-x64",
            "browse": {
                "path": [
                    "${workspaceFolder}",
                    "${workspaceFolder}/libs",
                    "${workspaceFolder}/build"
                ],
                "limitSymbolsToIncludedHeaders": false
            }
        }
    ],
    "version": 4
}
EOF
fi

# Update c_cpp_properties.json with both defines and include paths
echo "✏️  Updating $CPP_PROPERTIES..."

TMP_FILE=$(mktemp)

# Use awk to update both includePath and defines sections
awk -v defines="$DEFINES" -v include_paths="$INCLUDE_PATHS_JSON" '
BEGIN {
    in_defines = 0
    in_include_path = 0
    # Parse include paths JSON into array
    split(include_paths, lines, "\n")
}
{
    # Handle includePath section
    if ($0 ~ /"includePath": \[/) {
        in_include_path = 1
        print "            \"includePath\": ["
        # Print all include paths from detected JSON array (skip opening [ and closing ])
        for (i = 2; i < length(lines); i++) {
            gsub(/^[[:space:]]+/, "", lines[i])  # Remove leading spaces
            if (lines[i] != "") {
                print "                " lines[i]
            }
        }
        next
    }
    if (in_include_path && $0 ~ /\]/) {
        in_include_path = 0
        print "            ],"
        next
    }
    if (in_include_path) {
        next  # Skip old include paths
    }

    # Handle defines section
    if ($0 ~ /"defines": \[/) {
        in_defines = 1
        print "            \"defines\": ["
        printf "                %s\n", defines
        next
    }
    if (in_defines && $0 ~ /\]/) {
        in_defines = 0
        print "            ],"
        next
    }
    if (!in_defines) {
        print $0
    }
}
' "$CPP_PROPERTIES" > "$TMP_FILE"

mv "$TMP_FILE" "$CPP_PROPERTIES"

# Ensure compile_commands.json is in root
if [ -f "$PROJECT_ROOT/build/compile_commands.json" ]; then
    cp "$PROJECT_ROOT/build/compile_commands.json" "$COMPILE_COMMANDS"
    echo "✓ compile_commands.json copied to project root"
fi

echo ""
echo "✅ IntelliSense synchronized successfully!"
echo "💡 Reload VSCode window (Ctrl+Shift+P -> 'Developer: Reload Window') for changes to take effect"

if [ ${#ENABLED_DRIVERS[@]} -gt 0 ]; then
    echo ""
    echo "📊 Enabled drivers:"
    for driver in "${ENABLED_DRIVERS[@]}"; do
        echo "  ✓ $driver"
    done
fi
