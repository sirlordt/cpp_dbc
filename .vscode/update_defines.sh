#!/bin/bash
# Script to update c_cpp_properties.json with defines from compile_commands.json
# This ensures IntelliSense always has the correct compilation flags
# Also updates include paths based on enabled drivers

set -e

echo "🔍 Extracting defines from compile_commands.json..."

# Navigate to project root
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"

COMPILE_COMMANDS="$PROJECT_ROOT/compile_commands.json"
CPP_PROPERTIES="$SCRIPT_DIR/c_cpp_properties.json"

if [ ! -f "$COMPILE_COMMANDS" ]; then
    echo "❌ Error: $COMPILE_COMMANDS not found"
    echo "   Run './build.sh' first to generate it"
    exit 1
fi

if [ ! -f "$CPP_PROPERTIES" ]; then
    echo "❌ Error: $CPP_PROPERTIES not found"
    exit 1
fi

# Extract defines from compile_commands.json
DEFINES=$(grep -oP '\-D\K[A-Z_]+=\d+' "$COMPILE_COMMANDS" | sort -u)

echo "📝 Found defines:"
echo "$DEFINES" | sed 's/^/   /'

# Build the JSON array of defines
DEFINES_JSON=""
FIRST=true
while IFS= read -r define; do
    if [ -n "$define" ]; then
        if [ "$FIRST" = true ]; then
            DEFINES_JSON="\"$define\""
            FIRST=false
        else
            DEFINES_JSON="$DEFINES_JSON,\n                \"$define\""
        fi
    fi
done <<< "$DEFINES"

# Detect include paths automatically
echo ""
echo "🔍 Detecting include paths for enabled drivers..."
INCLUDE_PATHS_JSON=$("$SCRIPT_DIR/detect_include_paths.sh")

# Create temporary file with updated c_cpp_properties.json
TMP_FILE=$(mktemp)

# Read the original file and replace both includePath and defines sections
awk -v defines="$DEFINES_JSON" -v include_paths="$INCLUDE_PATHS_JSON" '
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
        # Print all include paths from detected JSON (skip first and last line which are braces)
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

# Replace the original file
mv "$TMP_FILE" "$CPP_PROPERTIES"

echo ""
echo "✅ Successfully updated defines in c_cpp_properties.json"
echo "💡 Tip: Reload VSCode window (Ctrl+Shift+P -> 'Developer: Reload Window') for changes to take effect"
