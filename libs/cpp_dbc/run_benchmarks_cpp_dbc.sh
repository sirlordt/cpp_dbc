#!/bin/bash

# Script to run the cpp_dbc benchmarks
# This script assumes you have already built the project with CPP_DBC_BUILD_BENCHMARKS=ON

# Default values
USE_MYSQL=ON
USE_POSTGRESQL=OFF
USE_SQLITE=OFF
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
BACKWARD_HAS_DW=ON

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
        --debug-all)
            DEBUG_CONNECTION_POOL=ON
            DEBUG_TRANSACTION_MANAGER=ON
            DEBUG_SQLITE=ON
            shift
            ;;
        --dw-off)
            BACKWARD_HAS_DW=OFF
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
            echo "  --debug-all            Enable all debug output"
            echo "  --dw-off               Disable libdw support for stack traces"
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
echo "  YAML support: $USE_CPP_YAML"
echo "  Build type: $BUILD_TYPE"
echo "  Clean build: $CLEAN_BUILD"
echo "  Rebuild: $REBUILD"
echo "  Benchmark min time: $BENCHMARK_MIN_TIME seconds"
echo "  Benchmark repetitions: $BENCHMARK_REPETITIONS"
echo "  Debug ConnectionPool: $DEBUG_CONNECTION_POOL"
echo "  Debug TransactionManager: $DEBUG_TRANSACTION_MANAGER"
echo "  Debug SQLite: $DEBUG_SQLITE"
echo "  libdw support: $BACKWARD_HAS_DW"
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
    
    if [ "$BACKWARD_HAS_DW" = "OFF" ]; then
        BUILD_OPTIONS="$BUILD_OPTIONS --dw-off"
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

# Run the benchmarks
echo "Running cpp_dbc benchmarks..."
echo "=============================="

# Prepare benchmark options
BENCHMARK_OPTIONS="--benchmark_min_time=${BENCHMARK_MIN_TIME}s --benchmark_repetitions=${BENCHMARK_REPETITIONS}"

# If benchmark tags are specified, use them directly
if [ -n "$BENCHMARK_TAGS" ]; then
    echo "Running benchmarks with tags: $BENCHMARK_TAGS"
    
    # Map tag formats from the previous Catch2 format to Google Benchmark format
    # Convert formats like "update+postgresql" to regex patterns like "BM.*Update.*PostgreSQL"
    # This is an approximate conversion
    FILTER=""
    
    if [[ "$BENCHMARK_TAGS" == *"mysql"* ]]; then
        FILTER="${FILTER}MySQL|"
    fi
    
    if [[ "$BENCHMARK_TAGS" == *"postgresql"* ]]; then
        FILTER="${FILTER}PostgreSQL|"
    fi
    
    if [[ "$BENCHMARK_TAGS" == *"sqlite"* ]]; then
        FILTER="${FILTER}SQLite|"
    fi
    
    if [[ "$BENCHMARK_TAGS" == *"insert"* ]]; then
        FILTER="${FILTER}.*Insert.*|"
    fi
    
    if [[ "$BENCHMARK_TAGS" == *"update"* ]]; then
        FILTER="${FILTER}.*Update.*|"
    fi
    
    if [[ "$BENCHMARK_TAGS" == *"delete"* ]]; then
        FILTER="${FILTER}.*Delete.*|"
    fi
    
    if [[ "$BENCHMARK_TAGS" == *"select"* ]]; then
        FILTER="${FILTER}.*Select.*|"
    fi
    
    # Remove trailing pipe if it exists
    FILTER=${FILTER%|}
    
    echo "Using benchmark filter: $FILTER"
    ${BENCHMARK_EXECUTABLE} --benchmark_filter="$FILTER" $BENCHMARK_OPTIONS
    echo ""
    echo "=============================="
else
    # MySQL benchmarks
    if [ "$USE_MYSQL" = "ON" ]; then
        echo "Running MySQL benchmarks..."
        # Pattern to match the standard BM_MySQL_* benchmark names
        ${BENCHMARK_EXECUTABLE} --benchmark_filter="BM_MySQL" $BENCHMARK_OPTIONS
        echo ""
        echo "=============================="
    fi

    # PostgreSQL benchmarks
    if [ "$USE_POSTGRESQL" = "ON" ]; then
        echo "Running PostgreSQL benchmarks..."
        # Pattern to match the standard BM_PostgreSQL_* benchmark names
        ${BENCHMARK_EXECUTABLE} --benchmark_filter="BM_PostgreSQL" $BENCHMARK_OPTIONS
        echo ""
        echo "=============================="
    fi

    # SQLite benchmarks
    if [ "$USE_SQLITE" = "ON" ]; then
        echo "Running SQLite benchmarks..."
        # Pattern to match the standard BM_SQLite_* benchmark names
        ${BENCHMARK_EXECUTABLE} --benchmark_filter="BM_SQLite" $BENCHMARK_OPTIONS
        echo ""
        echo "=============================="
    fi
fi

echo "Benchmarks completed!"