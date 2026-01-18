#!/bin/bash
# Script to regenerate compile_commands.json for IntelliSense
# This uses the main build.sh to ensure consistency
#
# Usage:
#   ./regenerate_intellisense.sh              # Use saved parameters from last build
#   ./regenerate_intellisense.sh [options]    # Build with specific options

set -e

# Navigate to project root
cd "$(dirname "$0")/.."

BUILD_ARGS_FILE="build/.build_args"

# Determine which parameters to use
if [ $# -eq 0 ]; then
    # No parameters provided - check if we have saved build args
    if [ -f "$BUILD_ARGS_FILE" ]; then
        BUILD_PARAMS=$(cat "$BUILD_ARGS_FILE")
        echo "🔄 No parameters provided - using saved parameters from last build"
        echo "📖 Build parameters: $BUILD_PARAMS"
        echo ""
    else
        echo "⚠️  No saved build parameters found and no parameters provided"
        echo "   Using default build settings (MySQL only)"
        BUILD_PARAMS=""
    fi
else
    # Parameters provided - use them
    BUILD_PARAMS="$@"
    echo "🔄 Building with provided parameters: $BUILD_PARAMS"
    echo ""
fi

# Build the project
echo "📦 Building project to generate compile_commands.json..."
./build.sh $BUILD_PARAMS

# Copy compile_commands.json from build directory to project root
if [ -f "build/compile_commands.json" ]; then
    cp build/compile_commands.json .
    echo "✓ compile_commands.json copied to project root"
else
    echo "⚠️  compile_commands.json not found in build directory"
    echo "   Trying to generate it manually..."

    # Try to regenerate it
    cd build
    cmake .. -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
    cd ..

    if [ -f "build/compile_commands.json" ]; then
        cp build/compile_commands.json .
        echo "✓ compile_commands.json generated and copied"
    else
        echo "✗ Failed to generate compile_commands.json"
        exit 1
    fi
fi

# Extract and show current compilation defines
echo ""
echo "📋 Current compilation defines:"
grep -o "\-D[A-Z_]*=[0-9]" compile_commands.json | sort -u | sed 's/^-D/  /'

echo ""
echo "✅ IntelliSense should now work correctly!"
echo "💡 Tip: Reload VSCode window (Ctrl+Shift+P -> 'Developer: Reload Window') for changes to take effect"
