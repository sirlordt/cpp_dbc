#!/bin/bash

# Script to run the cpp_dbc benchmarks
# This script assumes you have already built the project with CPP_DBC_BUILD_BENCHMARKS=ON

# Default values
USE_MYSQL=ON
USE_POSTGRESQL=OFF
USE_SQLITE=OFF
USE_FIREBIRD=OFF
USE_MONGODB=OFF
USE_CPP_YAML=OFF
BENCHMARK_MIN_TIME="0.5"  # Minimum time per iteration for Google Benchmark
BENCHMARK_REPETITIONS=3   # Number of repetitions for Google Benchmark
BENCHMARK_TAGS=""         # Preserved for compatibility
CLEAN_BUILD=false
REBUILD=false
RELEASE_BUILD=false
DEBUG_CONNECTION_POOL=OFF
DEBUG_TRANSACTION_MANAGER=OFF
DEBUG_SQLITE=OFF
DEBUG_FIREBIRD=OFF
DEBUG_MONGODB=OFF
BACKWARD_HAS_DW=ON
USE_MEMORY_USAGE=false    # Whether to use /usr/bin/time for memory usage tracking
DB_DRIVER_THREAD_SAFE=ON

# Parse command-line arguments
while [[ $# -gt 0 ]]; do
    case "$1" in
        --mysql|--mysql-on)
            USE_MYSQL=ON
            shift
            ;;
        --mysql-off)
            USE_MYSQL=OFF
            shift
            ;;
        --postgresql|--postgres|--postgres-on)
            USE_POSTGRESQL=ON
            shift
            ;;
        --postgres-off)
            USE_POSTGRESQL=OFF
            shift
            ;;
        --sqlite|--sqlite-on)
            USE_SQLITE=ON
            shift
            ;;
        --sqlite-off)
            USE_SQLITE=OFF
            shift
            ;;
        --firebird|--firebird-on)
            USE_FIREBIRD=ON
            shift
            ;;
        --firebird-off)
            USE_FIREBIRD=OFF
            shift
            ;;
        --mongodb|--mongodb-on)
            USE_MONGODB=ON
            shift
            ;;
        --mongodb-off)
            USE_MONGODB=OFF
            shift
            ;;
        --min-time=*)
            BENCHMARK_MIN_TIME="${1#*=}"
            shift
            ;;
        --repetitions=*)
            BENCHMARK_REPETITIONS="${1#*=}"
            shift
            ;;
        --run-benchmark=*)
            BENCHMARK_TAGS="${1#*=}"
            shift
            ;;
        --debug-pool)
            DEBUG_CONNECTION_POOL=ON
            shift
            ;;
        --debug-txmgr)
            DEBUG_TRANSACTION_MANAGER=ON
            shift
            ;;
        --debug-sqlite)
            DEBUG_SQLITE=ON
            shift
            ;;
        --debug-firebird)
            DEBUG_FIREBIRD=ON
            shift
            ;;
        --debug-mongodb)
            DEBUG_MONGODB=ON
            shift
            ;;
        --debug-all)
            DEBUG_CONNECTION_POOL=ON
            DEBUG_TRANSACTION_MANAGER=ON
            DEBUG_SQLITE=ON
            DEBUG_FIREBIRD=ON
            DEBUG_MONGODB=ON
            shift
            ;;
        --dw-off)
            BACKWARD_HAS_DW=OFF
            shift
            ;;
        --db-driver-thread-safe-off)
            DB_DRIVER_THREAD_SAFE=OFF
            shift
            ;;
        --memory-usage)
            USE_MEMORY_USAGE=true
            shift
            ;;
        --clean)
            CLEAN_BUILD=true
            shift
            ;;
        --rebuild)
            REBUILD=true
            shift
            ;;
        --release)
            RELEASE_BUILD=true
            shift
            ;;
        --yaml|--yaml-on)
            USE_CPP_YAML=ON
            shift
            ;;
        --yaml-off)
            USE_CPP_YAML=OFF
            shift
            ;;
        --help)
            echo "Usage: $0 [OPTIONS]"
            echo "Options:"
            echo "  --mysql, --mysql-on    Enable MySQL support"
            echo "  --mysql-off            Disable MySQL support"
            echo "  --postgresql, --postgres, --postgres-on  Enable PostgreSQL support"
            echo "  --postgres-off         Disable PostgreSQL support"
            echo "  --sqlite, --sqlite-on  Enable SQLite support"
            echo "  --sqlite-off           Disable SQLite support"
            echo "  --firebird, --firebird-on  Enable Firebird support"
            echo "  --firebird-off         Disable Firebird support"
            echo "  --mongodb, --mongodb-on  Enable MongoDB support"
            echo "  --mongodb-off         Disable MongoDB support"
            echo "  --yaml, --yaml-on      Enable YAML support"
            echo "  --yaml-off             Disable YAML support"
            echo "  --min-time=N           Set minimum time per iteration in seconds (default: 0.5)"
            echo "  --repetitions=N        Set number of benchmark repetitions (default: 5)"
            echo "  --run-benchmark=TAGS   Run only benchmarks with specified tags (e.g. update+postgresql)"
            echo "  --clean                Clean build directories before building"
            echo "  --rebuild              Force rebuild of benchmarks"
            echo "  --release              Build in release mode"
            echo "  --debug-pool           Enable debug output for ConnectionPool"
            echo "  --debug-txmgr          Enable debug output for TransactionManager"
            echo "  --debug-sqlite         Enable debug output for SQLite driver"
            echo "  --debug-firebird       Enable debug output for Firebird driver"
            echo "  --debug-mongodb        Enable debug output for MongoDB driver"
            echo "  --debug-all            Enable all debug output"
            echo "  --dw-off               Disable libdw support for stack traces"
            echo "  --db-driver-thread-safe-off  Disable thread-safe database driver operations"
            echo "  --memory-usage         Track memory usage with /usr/bin/time"
            echo "  --help                 Show this help message"
            exit 0
            ;;
        *)
            echo "Unknown option: $1"
            echo "Use --help to see available options"
            exit 1
            ;;
    esac
done

# Get the directory of this script
SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"

# Set the build directory
BUILD_DIR="${SCRIPT_DIR}/../../build"

# Check if the build directory exists
if [ ! -d "${BUILD_DIR}" ]; then
    echo "Error: Build directory not found at ${BUILD_DIR}"
    echo "Please build the project first with CPP_DBC_BUILD_BENCHMARKS=ON"
    exit 1
fi

# Print configuration
echo "Benchmark configuration:"
echo "  MySQL support: $USE_MYSQL"
echo "  PostgreSQL support: $USE_POSTGRESQL"
echo "  SQLite support: $USE_SQLITE"
echo "  Firebird support: $USE_FIREBIRD"
echo "  MongoDB support: $USE_MONGODB"
echo "  YAML support: $USE_CPP_YAML"
echo "  Build type: $BUILD_TYPE"
echo "  Clean build: $CLEAN_BUILD"
echo "  Rebuild: $REBUILD"
echo "  Benchmark min time: $BENCHMARK_MIN_TIME seconds"
echo "  Benchmark repetitions: $BENCHMARK_REPETITIONS"
echo "  Debug ConnectionPool: $DEBUG_CONNECTION_POOL"
echo "  Debug TransactionManager: $DEBUG_TRANSACTION_MANAGER"
echo "  Debug SQLite: $DEBUG_SQLITE"
echo "  Debug Firebird: $DEBUG_FIREBIRD"
echo "  Debug MongoDB: $DEBUG_MONGODB"
echo "  libdw support: $BACKWARD_HAS_DW"
echo "  DB driver thread-safe: $DB_DRIVER_THREAD_SAFE"
echo "  Memory usage tracking: $USE_MEMORY_USAGE"
if [ -n "$BENCHMARK_TAGS" ]; then
    echo "  Benchmark tags: $BENCHMARK_TAGS"
fi
echo ""

# Handle rebuild if requested
if [ "$CLEAN_BUILD" = true ] || [ "$REBUILD" = true ]; then
    echo "Rebuilding benchmarks..."
    
    # Get the directory of this script
    SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
    
    # Navigate to the project root directory
    cd "${SCRIPT_DIR}/../.."
    
    # Build options
    BUILD_OPTIONS=""
    
    if [ "$CLEAN_BUILD" = true ]; then
        BUILD_OPTIONS="$BUILD_OPTIONS --clean"
    fi
    
    if [ "$RELEASE_BUILD" = true ]; then
        BUILD_OPTIONS="$BUILD_OPTIONS --release"
    fi
    
    # Check if benchmark tags contain "postgresql" and enable PostgreSQL support if needed
    if [ -n "$BENCHMARK_TAGS" ] && [[ "$BENCHMARK_TAGS" == *"postgresql"* ]]; then
        echo "PostgreSQL benchmark tag detected, ensuring PostgreSQL support is enabled"
        USE_POSTGRESQL=ON
    fi
    
    # Check if benchmark tags contain "firebird" and enable Firebird support if needed
    if [ -n "$BENCHMARK_TAGS" ] && [[ "$BENCHMARK_TAGS" == *"firebird"* ]]; then
        echo "Firebird benchmark tag detected, ensuring Firebird support is enabled"
        USE_FIREBIRD=ON
    fi
    
    # Check if benchmark tags contain "mongodb" and enable MongoDB support if needed
    if [ -n "$BENCHMARK_TAGS" ] && [[ "$BENCHMARK_TAGS" == *"mongodb"* ]]; then
        echo "MongoDB benchmark tag detected, ensuring MongoDB support is enabled"
        USE_MONGODB=ON
    fi
    
    # Add database options
    if [ "$USE_MYSQL" = "ON" ]; then
        BUILD_OPTIONS="$BUILD_OPTIONS --mysql"
    else
        BUILD_OPTIONS="$BUILD_OPTIONS --mysql-off"
    fi
    
    if [ "$USE_POSTGRESQL" = "ON" ]; then
        BUILD_OPTIONS="$BUILD_OPTIONS --postgres"
    else
        BUILD_OPTIONS="$BUILD_OPTIONS --postgres-off"
    fi
    
    if [ "$USE_SQLITE" = "ON" ]; then
        BUILD_OPTIONS="$BUILD_OPTIONS --sqlite"
    else
        BUILD_OPTIONS="$BUILD_OPTIONS --sqlite-off"
    fi
    
    if [ "$USE_FIREBIRD" = "ON" ]; then
        BUILD_OPTIONS="$BUILD_OPTIONS --firebird"
    else
        BUILD_OPTIONS="$BUILD_OPTIONS --firebird-off"
    fi
    
    if [ "$USE_MONGODB" = "ON" ]; then
        BUILD_OPTIONS="$BUILD_OPTIONS --mongodb"
    else
        BUILD_OPTIONS="$BUILD_OPTIONS --mongodb-off"
    fi
    
    if [ "$USE_CPP_YAML" = "ON" ]; then
        BUILD_OPTIONS="$BUILD_OPTIONS --yaml"
    fi
    
    # Add debug options
    if [ "$DEBUG_CONNECTION_POOL" = "ON" ]; then
        BUILD_OPTIONS="$BUILD_OPTIONS --debug-pool"
    fi
    
    if [ "$DEBUG_TRANSACTION_MANAGER" = "ON" ]; then
        BUILD_OPTIONS="$BUILD_OPTIONS --debug-txmgr"
    fi
    
    if [ "$DEBUG_SQLITE" = "ON" ]; then
        BUILD_OPTIONS="$BUILD_OPTIONS --debug-sqlite"
    fi
    
    if [ "$DEBUG_FIREBIRD" = "ON" ]; then
        BUILD_OPTIONS="$BUILD_OPTIONS --debug-firebird"
    fi
    
    if [ "$DEBUG_MONGODB" = "ON" ]; then
        BUILD_OPTIONS="$BUILD_OPTIONS --debug-mongodb"
    fi
    
    if [ "$BACKWARD_HAS_DW" = "OFF" ]; then
        BUILD_OPTIONS="$BUILD_OPTIONS --dw-off"
    fi
    
    if [ "$DB_DRIVER_THREAD_SAFE" = "OFF" ]; then
        BUILD_OPTIONS="$BUILD_OPTIONS --db-driver-thread-safe-off"
    fi
    
    # Always include benchmarks
    BUILD_OPTIONS="$BUILD_OPTIONS --benchmarks"
    
    # Run the build script
    echo "Running: ./build.sh $BUILD_OPTIONS"
    ./build.sh $BUILD_OPTIONS
    
    # Return to the original directory
    cd - > /dev/null
fi

# Check if the benchmark executable exists
BENCHMARK_EXECUTABLE="${BUILD_DIR}/libs/cpp_dbc/build/benchmark/cpp_dbc_benchmarks"
if [ ! -f "${BENCHMARK_EXECUTABLE}" ]; then
    echo "Error: Benchmark executable not found at ${BENCHMARK_EXECUTABLE}"
    echo "Please build the project with CPP_DBC_BUILD_BENCHMARKS=ON"
    exit 1
fi

# Check if memory usage tracking is requested
if [ "$USE_MEMORY_USAGE" = true ]; then
    echo "Checking for /usr/bin/time..."
    if ! command -v /usr/bin/time &> /dev/null; then
        echo "/usr/bin/time not found. Attempting to install..."
        
        # Check if we're on a Debian-based system
        if command -v apt-get &> /dev/null; then
            echo "Debian-based system detected. Installing time package..."
            # Try to update, but continue even if it fails
            sudo apt-get update || echo "apt-get update failed, but continuing with installation attempt..."
            
            # Try to install time
            if sudo apt-get install -y time; then
                echo "time package installed successfully."
            else
                echo "Error: Failed to install time package."
                echo "Please install time package manually and try again."
                exit 1
            fi
        else
            echo "Error: /usr/bin/time is not installed and this doesn't appear to be a Debian-based system."
            echo "Please install time package manually and try again."
            exit 1
        fi
    else
        echo "/usr/bin/time is available."
    fi
fi

# Run the benchmarks
echo "Running cpp_dbc benchmarks..."
echo "=============================="

# Prepare benchmark options
BENCHMARK_OPTIONS="--benchmark_min_time=${BENCHMARK_MIN_TIME}s --benchmark_repetitions=${BENCHMARK_REPETITIONS}"

# If benchmark tags are specified, use them directly
if [ -n "$BENCHMARK_TAGS" ]; then
    echo "Running benchmarks with tags: $BENCHMARK_TAGS"
    
    # Split the benchmark tags by "+" and build a regex pattern that requires ALL conditions
    IFS='+' read -ra TAG_PARTS <<< "$BENCHMARK_TAGS"
    
    # Start with an empty filter
    FILTER="^BM_"
    
    # Process each tag part and add it to the filter
    for part in "${TAG_PARTS[@]}"; do
        # Convert the part to lowercase for case-insensitive comparison
        lower_part=$(echo "$part" | tr '[:upper:]' '[:lower:]')
        
        case "$lower_part" in
            "mysql")
                FILTER="${FILTER}.*MySQL"
                ;;
            "postgresql")
                FILTER="${FILTER}.*PostgreSQL"
                ;;
            "sqlite")
                FILTER="${FILTER}.*SQLite"
                ;;
            "firebird")
                FILTER="${FILTER}.*Firebird"
                ;;
            "mongodb")
                FILTER="${FILTER}.*MongoDB"
                ;;
            "insert")
                FILTER="${FILTER}.*Insert"
                ;;
            "update")
                FILTER="${FILTER}.*Update"
                ;;
            "delete")
                FILTER="${FILTER}.*Delete"
                ;;
            "select")
                FILTER="${FILTER}.*Select"
                ;;
            "small")
                FILTER="${FILTER}.*Small"
                ;;
            "large")
                FILTER="${FILTER}.*Large"
                ;;
            "findone")
                FILTER="${FILTER}.*FindOne"
                ;;
            "find")
                # Only add this if it's not part of FindOne
                if [[ "$BENCHMARK_TAGS" != *"findone"* ]]; then
                    FILTER="${FILTER}.*Find"
                fi
                ;;
            *)
                # Handle any other specific tag by capitalizing first letter
                capitalized=$(echo "$part" | sed 's/\<./\u&/g')
                FILTER="${FILTER}.*$capitalized"
                ;;
        esac
    done
    
    # Add the end of the regex pattern
    FILTER="${FILTER}.*\$"
    
    # Print the final filter for debugging
    echo "DEBUG: Final benchmark filter regex: \"$FILTER\""
    
    echo "Using benchmark filter: $FILTER"
    if [ "$USE_MEMORY_USAGE" = true ]; then
        echo "Running with memory usage tracking..."
        /usr/bin/time -v ${BENCHMARK_EXECUTABLE} --benchmark_filter="$FILTER" $BENCHMARK_OPTIONS
    else
        ${BENCHMARK_EXECUTABLE} --benchmark_filter="$FILTER" $BENCHMARK_OPTIONS
    fi
    echo ""
    echo "=============================="
else
    # MySQL benchmarks
    if [ "$USE_MYSQL" = "ON" ]; then
        echo "Running MySQL benchmarks..."
        # Pattern to match the standard BM_MySQL_* benchmark names
        if [ "$USE_MEMORY_USAGE" = true ]; then
            echo "Running with memory usage tracking..."
            /usr/bin/time -v ${BENCHMARK_EXECUTABLE} --benchmark_filter="BM_MySQL" $BENCHMARK_OPTIONS
        else
            ${BENCHMARK_EXECUTABLE} --benchmark_filter="BM_MySQL" $BENCHMARK_OPTIONS
        fi
        echo ""
        echo "=============================="
    fi

    # PostgreSQL benchmarks
    if [ "$USE_POSTGRESQL" = "ON" ]; then
        echo "Running PostgreSQL benchmarks..."
        # Pattern to match the standard BM_PostgreSQL_* benchmark names
        if [ "$USE_MEMORY_USAGE" = true ]; then
            echo "Running with memory usage tracking..."
            /usr/bin/time -v ${BENCHMARK_EXECUTABLE} --benchmark_filter="BM_PostgreSQL" $BENCHMARK_OPTIONS
        else
            ${BENCHMARK_EXECUTABLE} --benchmark_filter="BM_PostgreSQL" $BENCHMARK_OPTIONS
        fi
        echo ""
        echo "=============================="
    fi

    # SQLite benchmarks
    if [ "$USE_SQLITE" = "ON" ]; then
        echo "Running SQLite benchmarks..."
        # Pattern to match the standard BM_SQLite_* benchmark names
        if [ "$USE_MEMORY_USAGE" = true ]; then
            echo "Running with memory usage tracking..."
            /usr/bin/time -v ${BENCHMARK_EXECUTABLE} --benchmark_filter="BM_SQLite" $BENCHMARK_OPTIONS
        else
            ${BENCHMARK_EXECUTABLE} --benchmark_filter="BM_SQLite" $BENCHMARK_OPTIONS
        fi
        echo ""
        echo "=============================="
    fi

    # Firebird benchmarks
    if [ "$USE_FIREBIRD" = "ON" ]; then
        echo "Running Firebird benchmarks..."
        # Pattern to match the standard BM_Firebird_* benchmark names
        if [ "$USE_MEMORY_USAGE" = true ]; then
            echo "Running with memory usage tracking..."
            /usr/bin/time -v ${BENCHMARK_EXECUTABLE} --benchmark_filter="BM_Firebird" $BENCHMARK_OPTIONS
        else
            ${BENCHMARK_EXECUTABLE} --benchmark_filter="BM_Firebird" $BENCHMARK_OPTIONS
        fi
        echo ""
        echo "=============================="
    fi
    
    # MongoDB benchmarks
    if [ "$USE_MONGODB" = "ON" ]; then
        echo "Running MongoDB benchmarks..."
        # Pattern to match the standard BM_MongoDB_* benchmark names
        if [ "$USE_MEMORY_USAGE" = true ]; then
            echo "Running with memory usage tracking..."
            /usr/bin/time -v ${BENCHMARK_EXECUTABLE} --benchmark_filter="BM_MongoDB" $BENCHMARK_OPTIONS
        else
            ${BENCHMARK_EXECUTABLE} --benchmark_filter="BM_MongoDB" $BENCHMARK_OPTIONS
        fi
        echo ""
        echo "=============================="
    fi
fi

echo "Benchmarks completed!"

# No need for a summary message since output is displayed directly to the screen