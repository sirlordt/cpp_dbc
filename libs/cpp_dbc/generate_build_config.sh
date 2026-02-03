#!/bin/bash

# generate_build_config.sh
# Script to generate build/.build_config file with current build configuration
# This file is used by VSCode IntelliSense synchronization (.vscode/sync_intellisense.sh)
#
# Usage:
#   1. With parameters (called from build scripts):
#      ./generate_build_config.sh --mysql=ON --postgres=OFF --sqlite=ON ...
#
#   2. Without parameters (auto-detect from CMakeCache.txt):
#      ./generate_build_config.sh
#
# When called without parameters, it searches for CMakeCache.txt in known locations
# and uses the most recent one to extract build configuration.

set -e

# Determine script directory and project root
SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )"
PROJECT_ROOT="$( cd "${SCRIPT_DIR}/../.." &> /dev/null && pwd )"
BUILD_CONFIG_FILE="${PROJECT_ROOT}/build/.build_config"

# Default values
USE_MYSQL=ON
USE_POSTGRESQL=OFF
USE_SQLITE=OFF
USE_FIREBIRD=OFF
USE_MONGODB=OFF
USE_SCYLLADB=OFF
USE_REDIS=OFF
USE_CPP_YAML=ON
BUILD_TYPE=Debug
DB_DRIVER_THREAD_SAFE=ON
BACKWARD_HAS_DW=ON
DEBUG_CONNECTION_POOL=OFF
DEBUG_TRANSACTION_MANAGER=OFF
DEBUG_SQLITE=OFF
DEBUG_FIREBIRD=OFF
DEBUG_MONGODB=OFF
DEBUG_SCYLLADB=OFF
DEBUG_REDIS=OFF
DEBUG_ALL=OFF

# Flag to track if we received any parameters
RECEIVED_PARAMS=false

# Parse command line arguments
for arg in "$@"
do
    RECEIVED_PARAMS=true
    case $arg in
        --mysql=*)
            USE_MYSQL="${arg#*=}"
            shift
            ;;
        --postgres=*|--postgresql=*)
            USE_POSTGRESQL="${arg#*=}"
            shift
            ;;
        --sqlite=*)
            USE_SQLITE="${arg#*=}"
            shift
            ;;
        --firebird=*)
            USE_FIREBIRD="${arg#*=}"
            shift
            ;;
        --mongodb=*)
            USE_MONGODB="${arg#*=}"
            shift
            ;;
        --scylladb=*)
            USE_SCYLLADB="${arg#*=}"
            shift
            ;;
        --redis=*)
            USE_REDIS="${arg#*=}"
            shift
            ;;
        --yaml=*|--cpp-yaml=*)
            USE_CPP_YAML="${arg#*=}"
            shift
            ;;
        --build-type=*)
            BUILD_TYPE="${arg#*=}"
            shift
            ;;
        --db-driver-thread-safe=*)
            DB_DRIVER_THREAD_SAFE="${arg#*=}"
            shift
            ;;
        --backward-has-dw=*)
            BACKWARD_HAS_DW="${arg#*=}"
            shift
            ;;
        --debug-pool=*)
            DEBUG_CONNECTION_POOL="${arg#*=}"
            shift
            ;;
        --debug-txmgr=*)
            DEBUG_TRANSACTION_MANAGER="${arg#*=}"
            shift
            ;;
        --debug-sqlite=*)
            DEBUG_SQLITE="${arg#*=}"
            shift
            ;;
        --debug-firebird=*)
            DEBUG_FIREBIRD="${arg#*=}"
            shift
            ;;
        --debug-mongodb=*)
            DEBUG_MONGODB="${arg#*=}"
            shift
            ;;
        --debug-scylladb=*)
            DEBUG_SCYLLADB="${arg#*=}"
            shift
            ;;
        --debug-redis=*)
            DEBUG_REDIS="${arg#*=}"
            shift
            ;;
        --debug-all=*)
            DEBUG_ALL="${arg#*=}"
            shift
            ;;
        --help)
            echo "Usage: $0 [options]"
            echo "Options:"
            echo "  --mysql=ON|OFF                 Enable/disable MySQL support"
            echo "  --postgres=ON|OFF              Enable/disable PostgreSQL support"
            echo "  --sqlite=ON|OFF                Enable/disable SQLite support"
            echo "  --firebird=ON|OFF              Enable/disable Firebird support"
            echo "  --mongodb=ON|OFF               Enable/disable MongoDB support"
            echo "  --scylladb=ON|OFF              Enable/disable ScyllaDB support"
            echo "  --redis=ON|OFF                 Enable/disable Redis support"
            echo "  --yaml=ON|OFF                  Enable/disable YAML support"
            echo "  --build-type=Debug|Release     Set build type"
            echo "  --db-driver-thread-safe=ON|OFF Enable/disable thread-safe operations"
            echo "  --backward-has-dw=ON|OFF       Enable/disable libdw for stack traces"
            echo "  --debug-pool=ON|OFF            Enable/disable ConnectionPool debug"
            echo "  --debug-txmgr=ON|OFF           Enable/disable TransactionManager debug"
            echo "  --debug-sqlite=ON|OFF          Enable/disable SQLite debug"
            echo "  --debug-firebird=ON|OFF        Enable/disable Firebird debug"
            echo "  --debug-mongodb=ON|OFF         Enable/disable MongoDB debug"
            echo "  --debug-scylladb=ON|OFF        Enable/disable ScyllaDB debug"
            echo "  --debug-redis=ON|OFF           Enable/disable Redis debug"
            echo "  --debug-all=ON|OFF             Enable/disable all debug output"
            echo ""
            echo "If no parameters are provided, the script will attempt to read configuration"
            echo "from CMakeCache.txt files in known build directories."
            exit 0
            ;;
        *)
            echo "Unknown option: $arg"
            echo "Use --help to see available options"
            exit 1
            ;;
    esac
done

# If no parameters were provided, try to read from CMakeCache.txt
if [ "$RECEIVED_PARAMS" = false ]; then
    echo "No parameters provided. Attempting to auto-detect configuration from CMakeCache.txt..."

    # List of possible CMakeCache.txt locations (in order of preference)
    CMAKE_CACHE_LOCATIONS=(
        "${PROJECT_ROOT}/build/libs/cpp_dbc/build/CMakeCache.txt"
        "${PROJECT_ROOT}/build/CMakeCache.txt"
    )

    # Find the most recent CMakeCache.txt
    MOST_RECENT_CACHE=""
    MOST_RECENT_TIME=0

    for cache_file in "${CMAKE_CACHE_LOCATIONS[@]}"; do
        if [ -f "$cache_file" ]; then
            file_time=$(stat -c %Y "$cache_file" 2>/dev/null || stat -f %m "$cache_file" 2>/dev/null || echo 0)
            if [ "$file_time" -gt "$MOST_RECENT_TIME" ]; then
                MOST_RECENT_TIME=$file_time
                MOST_RECENT_CACHE="$cache_file"
            fi
        fi
    done

    if [ -n "$MOST_RECENT_CACHE" ]; then
        echo "✓ Found CMakeCache.txt: $MOST_RECENT_CACHE"

        # Extract values from CMakeCache.txt
        # CMakeCache.txt format: VARIABLE:TYPE=VALUE
        extract_cmake_var() {
            local var_name="$1"
            local default_value="$2"
            local value=$(grep "^${var_name}:" "$MOST_RECENT_CACHE" 2>/dev/null | cut -d= -f2)
            if [ -z "$value" ]; then
                echo "$default_value"
            else
                # Convert BOOL values to ON/OFF
                if [ "$value" = "1" ] || [ "$value" = "TRUE" ] || [ "$value" = "True" ] || [ "$value" = "true" ]; then
                    echo "ON"
                elif [ "$value" = "0" ] || [ "$value" = "FALSE" ] || [ "$value" = "False" ] || [ "$value" = "false" ]; then
                    echo "OFF"
                else
                    echo "$value"
                fi
            fi
        }

        USE_MYSQL=$(extract_cmake_var "USE_MYSQL" "$USE_MYSQL")
        USE_POSTGRESQL=$(extract_cmake_var "USE_POSTGRESQL" "$USE_POSTGRESQL")
        USE_SQLITE=$(extract_cmake_var "USE_SQLITE" "$USE_SQLITE")
        USE_FIREBIRD=$(extract_cmake_var "USE_FIREBIRD" "$USE_FIREBIRD")
        USE_MONGODB=$(extract_cmake_var "USE_MONGODB" "$USE_MONGODB")
        USE_SCYLLADB=$(extract_cmake_var "USE_SCYLLADB" "$USE_SCYLLADB")
        USE_REDIS=$(extract_cmake_var "USE_REDIS" "$USE_REDIS")
        USE_CPP_YAML=$(extract_cmake_var "USE_CPP_YAML" "$USE_CPP_YAML")
        BUILD_TYPE=$(extract_cmake_var "CMAKE_BUILD_TYPE" "$BUILD_TYPE")
        DB_DRIVER_THREAD_SAFE=$(extract_cmake_var "DB_DRIVER_THREAD_SAFE" "$DB_DRIVER_THREAD_SAFE")
        BACKWARD_HAS_DW=$(extract_cmake_var "BACKWARD_HAS_DW" "$BACKWARD_HAS_DW")
        DEBUG_CONNECTION_POOL=$(extract_cmake_var "DEBUG_CONNECTION_POOL" "$DEBUG_CONNECTION_POOL")
        DEBUG_TRANSACTION_MANAGER=$(extract_cmake_var "DEBUG_TRANSACTION_MANAGER" "$DEBUG_TRANSACTION_MANAGER")
        DEBUG_SQLITE=$(extract_cmake_var "DEBUG_SQLITE" "$DEBUG_SQLITE")
        DEBUG_FIREBIRD=$(extract_cmake_var "DEBUG_FIREBIRD" "$DEBUG_FIREBIRD")
        DEBUG_MONGODB=$(extract_cmake_var "DEBUG_MONGODB" "$DEBUG_MONGODB")
        DEBUG_SCYLLADB=$(extract_cmake_var "DEBUG_SCYLLADB" "$DEBUG_SCYLLADB")
        DEBUG_REDIS=$(extract_cmake_var "DEBUG_REDIS" "$DEBUG_REDIS")
        DEBUG_ALL=$(extract_cmake_var "DEBUG_ALL" "$DEBUG_ALL")

        echo "✓ Configuration extracted from CMakeCache.txt"
    else
        echo "⚠️  No CMakeCache.txt found in known locations."

        # Try to read existing .build_config if it exists
        if [ -f "$BUILD_CONFIG_FILE" ]; then
            echo "✓ Using existing $BUILD_CONFIG_FILE"
            # Nothing to do - file already exists with previous configuration
            exit 0
        else
            echo "❌ Error: Cannot auto-detect build configuration"
            echo "   No CMakeCache.txt found and no existing .build_config"
            echo ""
            echo "   Please run one of the following commands first to build the project:"
            echo "   - ./build.sh [options]"
            echo "   - ./helper.sh --run-build[=options]"
            echo "   - ./helper.sh --run-test[=options]"
            echo ""
            echo "   Or call this script with explicit parameters (use --help for details)"
            exit 1
        fi
    fi
fi

# Ensure build directory exists
mkdir -p "${PROJECT_ROOT}/build"

# Generate .build_config file
echo "Generating $BUILD_CONFIG_FILE..."

cat > "$BUILD_CONFIG_FILE" << EOF
# Build configuration - Auto-generated by generate_build_config.sh
# This file is used to keep VSCode IntelliSense in sync with build settings
USE_MYSQL=$USE_MYSQL
USE_POSTGRESQL=$USE_POSTGRESQL
USE_SQLITE=$USE_SQLITE
USE_FIREBIRD=$USE_FIREBIRD
USE_MONGODB=$USE_MONGODB
USE_SCYLLADB=$USE_SCYLLADB
USE_REDIS=$USE_REDIS
USE_CPP_YAML=$USE_CPP_YAML
BUILD_TYPE=$BUILD_TYPE
DB_DRIVER_THREAD_SAFE=$DB_DRIVER_THREAD_SAFE
BACKWARD_HAS_DW=$BACKWARD_HAS_DW
DEBUG_CONNECTION_POOL=$DEBUG_CONNECTION_POOL
DEBUG_TRANSACTION_MANAGER=$DEBUG_TRANSACTION_MANAGER
DEBUG_SQLITE=$DEBUG_SQLITE
DEBUG_FIREBIRD=$DEBUG_FIREBIRD
DEBUG_MONGODB=$DEBUG_MONGODB
DEBUG_SCYLLADB=$DEBUG_SCYLLADB
DEBUG_REDIS=$DEBUG_REDIS
DEBUG_ALL=$DEBUG_ALL
EOF

echo "✓ Build configuration saved to build/.build_config"
echo ""
echo "Configuration summary:"
echo "  MySQL: $USE_MYSQL"
echo "  PostgreSQL: $USE_POSTGRESQL"
echo "  SQLite: $USE_SQLITE"
echo "  Firebird: $USE_FIREBIRD"
echo "  MongoDB: $USE_MONGODB"
echo "  ScyllaDB: $USE_SCYLLADB"
echo "  Redis: $USE_REDIS"
echo "  YAML: $USE_CPP_YAML"
echo "  Build Type: $BUILD_TYPE"
