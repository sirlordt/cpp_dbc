#!/bin/bash
# Script to automatically detect include paths for enabled database drivers
# This script searches for header files in common locations and adds them to the include path list
# Also extracts Conan package include paths from compile_commands.json

set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"
BUILD_CONFIG="$PROJECT_ROOT/build/.build_config"
COMPILE_COMMANDS="$PROJECT_ROOT/compile_commands.json"

# Base include paths that are always needed
INCLUDE_PATHS=(
    "\${workspaceFolder}/**"
    "\${workspaceFolder}/libs/**"
    "\${workspaceFolder}/build/Debug/generators"
    "\${workspaceFolder}/build/Release/generators"
    "\${workspaceFolder}/build/libs/cpp_dbc/include"
    "\${workspaceFolder}/libs/cpp_dbc/include"
)

# Extract Conan and system include paths from compile_commands.json if it exists
if [ -f "$COMPILE_COMMANDS" ]; then
    # Extract all -isystem paths (these are Conan dependencies and system libraries)
    while IFS= read -r include_path; do
        # Skip if empty
        [[ -z "$include_path" ]] && continue
        # Add to include paths if not already present
        if [[ ! " ${INCLUDE_PATHS[@]} " =~ " ${include_path} " ]]; then
            INCLUDE_PATHS+=("$include_path")
        fi
    done < <(grep -oP '\-isystem\s+\K[^\s]+' "$COMPILE_COMMANDS" 2>/dev/null | sort -u)

    # Also extract -I paths (explicit include directories)
    while IFS= read -r include_path; do
        # Skip if empty or starts with special flags
        [[ -z "$include_path" ]] && continue
        [[ "$include_path" =~ ^- ]] && continue
        # Add to include paths if not already present
        if [[ ! " ${INCLUDE_PATHS[@]} " =~ " ${include_path} " ]]; then
            INCLUDE_PATHS+=("$include_path")
        fi
    done < <(grep -oP '\-I\K[^\s]+' "$COMPILE_COMMANDS" 2>/dev/null | sort -u)
fi

# Add all Conan package include directories (for test/benchmark dependencies)
# This ensures libraries like Catch2 and Benchmark are also available
CONAN_CACHE_DIRS=(
    "$HOME/.conan2/p"
    "/home/dsystems/Desktop/distrobox/containers/home/cpp_dev_env/.conan2/p"
)

for conan_dir in "${CONAN_CACHE_DIRS[@]}"; do
    if [ -d "$conan_dir" ]; then
        while IFS= read -r include_path; do
            # Add to include paths if not already present
            if [[ ! " ${INCLUDE_PATHS[@]} " =~ " ${include_path} " ]]; then
                INCLUDE_PATHS+=("$include_path")
            fi
        done < <(find "$conan_dir" -type d -path "*/p/include" 2>/dev/null)
    fi
done

# Function to search for a header file and add its directory to include paths
find_and_add_include() {
    local header_name=$1
    local search_dirs=("/usr/include" "/usr/local/include")

    for search_dir in "${search_dirs[@]}"; do
        if [ -d "$search_dir" ]; then
            # Find the header and get unique directory paths
            while IFS= read -r header_path; do
                dir_path=$(dirname "$header_path")
                # Add to include paths if not already present
                if [[ ! " ${INCLUDE_PATHS[@]} " =~ " ${dir_path} " ]]; then
                    INCLUDE_PATHS+=("$dir_path")
                fi
            done < <(find "$search_dir" -name "$header_name" 2>/dev/null)
        fi
    done
}

# Detect enabled drivers from build config
if [ -f "$BUILD_CONFIG" ]; then
    source "$BUILD_CONFIG"

    # MySQL includes
    if [ "$USE_MYSQL" = "ON" ]; then
        find_and_add_include "mysql.h"
        find_and_add_include "mysql_version.h"
    fi

    # PostgreSQL includes
    if [ "$USE_POSTGRESQL" = "ON" ]; then
        find_and_add_include "libpq-fe.h"
        find_and_add_include "pg_config.h"
    fi

    # SQLite includes
    if [ "$USE_SQLITE" = "ON" ]; then
        find_and_add_include "sqlite3.h"
    fi

    # Firebird includes
    if [ "$USE_FIREBIRD" = "ON" ]; then
        find_and_add_include "ibase.h"
    fi

    # MongoDB C++ driver includes
    if [ "$USE_MONGODB" = "ON" ]; then
        # MongoDB C++ driver
        find_and_add_include "mongocxx/client.hpp"
        find_and_add_include "bsoncxx/json.hpp"
        # MongoDB C driver
        find_and_add_include "mongoc.h"
        find_and_add_include "bson.h"
    fi

    # ScyllaDB/Cassandra includes
    if [ "$USE_SCYLLADB" = "ON" ]; then
        find_and_add_include "cassandra.h"
        find_and_add_include "dse.h"
    fi

    # Redis includes
    if [ "$USE_REDIS" = "ON" ]; then
        find_and_add_include "hiredis.h"
    fi
else
    echo "⚠️  Warning: $BUILD_CONFIG not found, using default include paths"
fi

# Output the include paths as JSON array
echo "{"
for i in "${!INCLUDE_PATHS[@]}"; do
    if [ $i -eq $((${#INCLUDE_PATHS[@]} - 1)) ]; then
        echo "    \"${INCLUDE_PATHS[$i]}\""
    else
        echo "    \"${INCLUDE_PATHS[$i]}\","
    fi
done
echo "}"
