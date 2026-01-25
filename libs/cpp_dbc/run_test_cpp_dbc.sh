#!/bin/bash
set -e  # Exit on error

# Script: run_test_cpp_dbc.sh
# Description: Runs the cpp_dbc library tests
# Usage: ./run_test_cpp_dbc.sh [options]
# Options:
#   --yaml, --yaml-on      Enable CPP-YAML support
#   --yaml-off             Disable CPP-YAML support
#   --mysql, --mysql-on    Enable MySQL support (default)
#   --mysql-off            Disable MySQL support
#   --postgres, --postgres-on  Enable PostgreSQL support
#   --postgres-off         Disable PostgreSQL support
#   --sqlite, --sqlite-on  Enable SQLite support
#   --sqlite-off           Disable SQLite support
#   --firebird, --firebird-on  Enable Firebird support
#   --firebird-off         Disable Firebird support
#   --release              Run in Release mode (default: Debug)
#   --gcc-analyzer         Enable GCC Static Analyzer (GCC 10+)
#   --asan                 Enable Address Sanitizer
#   --valgrind             Run tests with Valgrind
#   --auto                 Automatically continue to next test set if tests pass
#   --gssapi-leak-ok       Ignore GSSAPI leaks in PostgreSQL (only with --valgrind)
#   --ctest                Run tests using CTest
#   --check                Check shared library dependencies of test executable
#   --rebuild              Rebuild the test targets before running
#   --list                 List available tests only (does not run tests)
#   --run-test="filter"    Run only tests matching the filter. Two formats supported:
#                          - Wildcard pattern: "*mysql*" matches test names containing "mysql"
#                          - Tags: "tag1+tag2+tag3" runs tests with those specific tags
#   --run=N                Run all test sets N times (default: 1)
#   --debug-pool           Enable debug output for ConnectionPool
#   --debug-txmgr          Enable debug output for TransactionManager
#   --debug-sqlite         Enable debug output for SQLite driver
#   --debug-firebird       Enable debug output for Firebird driver
#   --debug-all            Enable all debug output
#   --dw-off               Disable libdw support for stack traces
#   --db-driver-thread-safe-off  Disable thread-safe database driver operations
#   --help                 Show this help message

# Default values for options
USE_MYSQL=ON
USE_CPP_YAML=OFF
USE_POSTGRESQL=OFF
USE_SQLITE=OFF
USE_FIREBIRD=OFF
USE_MONGODB=OFF
USE_SCYLLADB=OFF
USE_REDIS=OFF
BUILD_TYPE=Debug
ENABLE_GCC_ANALYZER=OFF
ASAN_OPTIONS=""
ENABLE_ASAN=false
RUN_CTEST=false
USE_VALGRIND=false
AUTO_CONTINUE=false
GSSAPI_LEAK_OK=false
CHECK_DEPENDENCIES=false
REBUILD=false
LIST_ONLY=false
RUN_SPECIFIC_TEST=""
DEBUG_CONNECTION_POOL=OFF
DEBUG_TRANSACTION_MANAGER=OFF
DEBUG_SQLITE=OFF
DEBUG_FIREBIRD=OFF
DEBUG_MONGODB=OFF
DEBUG_SCYLLADB=OFF
DEBUG_REDIS=OFF
RUN_COUNT=1
BACKWARD_HAS_DW=ON
DB_DRIVER_THREAD_SAFE=ON
SHOW_PROGRESS=false
SKIP_BUILD=false

# Parse command line arguments
while [[ $# -gt 0 ]]; do
    case $1 in
        --yaml|--yaml-on)
            USE_CPP_YAML=ON
            shift
            ;;
        --yaml-off)
            USE_CPP_YAML=OFF
            shift
            ;;
        --mysql|--mysql-on)
            USE_MYSQL=ON
            shift
            ;;
        --mysql-off)
            USE_MYSQL=OFF
            shift
            ;;
        --postgres|--postgres-on)
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
        --scylladb|--scylladb-on)
            USE_SCYLLADB=ON
            shift
            ;;
        --scylladb-off)
            USE_SCYLLADB=OFF
            shift
            ;;
        --redis|--redis-on)
            USE_REDIS=ON
            shift
            ;;
        --redis-off)
            USE_REDIS=OFF
            shift
            ;;
        --release)
            BUILD_TYPE=Release
            shift
            ;;
        --gcc-analyzer)
            ENABLE_GCC_ANALYZER=ON
            shift
            ;;
        --asan)
            ASAN_OPTIONS="handle_segv=0:allow_user_segv_handler=1:detect_leaks=1:print_stacktrace=1:halt_on_error=0"
            ENABLE_ASAN=true
            shift
            ;;
        --valgrind)
            USE_VALGRIND=true
            shift
            ;;
        --auto)
            AUTO_CONTINUE=true
            shift
            ;;
        --gssapi-leak-ok)
            GSSAPI_LEAK_OK=true
            shift
            ;;
        --ctest)
            RUN_CTEST=true
            shift
            ;;
        --check)
            CHECK_DEPENDENCIES=true
            shift
            ;;
        --rebuild)
            REBUILD=true
            shift
            ;;
        --list)
            LIST_ONLY=true
            shift
            ;;
        --run-test=*)
            RUN_SPECIFIC_TEST="${1#*=}"
            shift
            ;;
        --run=*)
            RUN_COUNT="${1#*=}"
            # Validate that RUN_COUNT is a positive integer
            if ! [[ "$RUN_COUNT" =~ ^[1-9][0-9]*$ ]]; then
                echo "Error: --run parameter must be a positive integer."
                exit 1
            fi
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
        --debug-scylladbdb)
            DEBUG_SCYLLADB=ON
            shift
            ;;
        --debug-redis)
            DEBUG_REDIS=ON
            shift
            ;;
        --debug-all)
            DEBUG_CONNECTION_POOL=ON
            DEBUG_TRANSACTION_MANAGER=ON
            DEBUG_SQLITE=ON
            DEBUG_FIREBIRD=ON
            DEBUG_MONGODB=ON
            DEBUG_SCYLLADB=ON
            DEBUG_REDIS=ON
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
        --progress)
            SHOW_PROGRESS=true
            shift
            ;;
        --skip-build)
            SKIP_BUILD=true
            shift
            ;;
        --help)
            echo "Usage: $0 [options]"
            echo "Options:"
            echo "  --mysql, --mysql-on    Enable MySQL support (default)"
            echo "  --mysql-off            Disable MySQL support"
            echo "  --postgres, --postgres-on  Enable PostgreSQL support"
            echo "  --postgres-off         Disable PostgreSQL support"
            echo "  --sqlite, --sqlite-on  Enable SQLite support"
            echo "  --sqlite-off           Disable SQLite support"
            echo "  --firebird, --firebird-on  Enable Firebird support"
            echo "  --firebird-off         Disable Firebird support"
            echo "  --mongodb, --mongodb-on  Enable MongoDB support"
            echo "  --mongodb-off          Disable MongoDB support"
            echo "  --scylladb, --scylladb-on    Enable ScyllaDB support"
            echo "  --scylladb-off           Disable ScyllaDB support"
            echo "  --redis, --redis-on    Enable Redis support"
            echo "  --redis-off            Disable Redis support"
            echo "  --release              Run in Release mode (default: Debug)"
            echo "  --asan                 Enable Address Sanitizer"
            echo "  --valgrind             Run tests with Valgrind"
            echo "  --auto                 Automatically continue to next test set if tests pass"
            echo "  --gssapi-leak-ok       Ignore GSSAPI leaks in PostgreSQL (only with --valgrind)"
            echo "  --ctest                Run tests using CTest"
            echo "  --check                Check shared library dependencies of test executable"
            echo "  --rebuild              Rebuild the test targets before running"
            echo "  --list                 List available tests only (does not run tests)"
            echo "  --run-test=\"filter\"    Run only tests matching the filter. Two formats supported:"
            echo "                           - Wildcard: \"*mysql*\" matches test names containing 'mysql'"
            echo "                           - Tags: \"tag1+tag2\" runs tests with those specific tags"
            echo "  --run=N                Run all test sets N times (default: 1)"
            echo "  --debug-pool           Enable debug output for ConnectionPool"
            echo "  --debug-txmgr          Enable debug output for TransactionManager"
            echo "  --debug-sqlite         Enable debug output for SQLite driver"
            echo "  --debug-firebird       Enable debug output for Firebird driver"
            echo "  --debug-mongodb        Enable debug output for MongoDB driver"
            echo "  --debug-scylladb         Enable debug output for ScyllaDB driver"
            echo "  --debug-redis          Enable debug output for Redis driver"
            echo "  --debug-all            Enable all debug output"
            echo "  --dw-off               Disable libdw support for stack traces"
            echo "  --db-driver-thread-safe-off  Disable thread-safe database driver operations"
            echo "  --progress             Show visual progress bar during test execution"
            echo "  --help                 Show this help message"
            exit 0
            ;;
        *)
            # Unknown option
            echo "Unknown option: $1"
            echo "Usage: $0 [options]"
            exit 1
            ;;
    esac
done

# Check for incompatible options
if [ "$USE_VALGRIND" = true ] && [ "$ENABLE_ASAN" = true ]; then
    echo -e "\nWARNING: Valgrind and AddressSanitizer are not fully compatible."
    echo -e "You should use either Valgrind or AddressSanitizer, but not both."
    echo -e "Consider not using --asan with --valgrind.\n"
    
    # Ask for confirmation
    read -p "Do you want to continue anyway? (y/n) " -n 1 -r
    echo
    if [[ ! $REPLY =~ ^[Yy]$ ]]; then
        echo "Exiting as requested."
        exit 1
    fi
    echo -e "Continuing as requested. Expect potential issues.\n"
fi

# Check if valgrind is installed if --valgrind is used
if [ "$USE_VALGRIND" = true ]; then
    if ! command -v valgrind &> /dev/null; then
        echo "Error: Valgrind is not installed but --valgrind option was specified."
        echo "Please install Valgrind or remove the --valgrind option."
        exit 1
    fi
fi

# Function to check if a command exists
command_exists() {
    command -v "$1" >/dev/null 2>&1
}

# Function to install packages if they don't exist
ensure_package_installed() {
    local package_name="$1"
    local command_name="${2:-$1}"  # Default to package name if command name not provided
    
    if ! command_exists "$command_name"; then
        echo "$command_name is not installed. Attempting to install $package_name..."
        
        # Check if we have sudo access
        if command_exists sudo; then
            sudo apt-get update
            sudo apt-get install -y "$package_name"
        else
            echo "Error: sudo is not available. Please install $package_name manually."
            exit 1
        fi
        
        # Verify installation
        if ! command_exists "$command_name"; then
            echo "Error: Failed to install $package_name. Please install it manually."
            exit 1
        fi
        
        echo "$package_name installed successfully."
    else
        echo "$command_name is already installed."
    fi
}

# Check and install required packages
ensure_package_installed "cmake"
ensure_package_installed "python3-pip" "pip3"

# Check for Conan
if ! command_exists conan; then
    echo "Conan is not installed. Attempting to install via pip..."
    pip3 install conan
    
    # Verify installation
    if ! command_exists conan; then
        echo "Error: Failed to install Conan. Please install it manually."
        exit 1
    fi
    
    echo "Conan installed successfully."
fi

# Check if Conan profile exists and create one if it doesn't
if ! conan profile list | grep -q default; then
    echo "No default Conan profile found. Creating one..."
    conan profile detect
fi

# Progress bar variables for scroll region mode
PROGRESS_SCROLL_SETUP=false
PROGRESS_ROWS=0
PROGRESS_COLS=0
PROGRESS_SCROLL_END=0

# Setup scroll region for sticky progress bar at bottom
setup_progress_scroll_region() {
    if [ "$PROGRESS_SCROLL_SETUP" = false ]; then
        PROGRESS_ROWS=$(tput lines)
        PROGRESS_COLS=$(tput cols)
        PROGRESS_SCROLL_END=$((PROGRESS_ROWS - 2))

        # Clear screen and set scroll region (leave 2 lines at bottom for progress)
        clear
        printf "\033[1;${PROGRESS_SCROLL_END}r"
        printf "\033[1;1H"
        PROGRESS_SCROLL_SETUP=true
    fi
}

# Restore normal scroll region
restore_progress_scroll_region() {
    if [ "$PROGRESS_SCROLL_SETUP" = true ]; then
        printf "\033[1;${PROGRESS_ROWS}r"
        printf "\033[${PROGRESS_ROWS};1H"
        # Clear the last two lines (progress bar area)
        printf "\033[K"
        printf "\033[$((PROGRESS_ROWS - 1));1H"
        printf "\033[K"
        printf "\033[${PROGRESS_ROWS};1H"
        printf "\n"
        PROGRESS_SCROLL_SETUP=false
    fi
}

# Function to display progress bar (only to terminal, not to log)
# Usage: show_progress_bar current_run total_runs current_test total_tests test_name start_time last_test_duration last_test_name
show_progress_bar() {
    local current_run=$1
    local total_runs=$2
    local current_test=$3
    local total_tests=$4
    local test_name=$5
    local start_time=$6
    local last_duration=${7:-0}
    local last_test_name=${8:-""}

    # Calculate total elapsed time
    local current_time=$(date +%s)
    local elapsed=$((current_time - start_time))
    local hours=$((elapsed / 3600))
    local minutes=$(((elapsed % 3600) / 60))
    local seconds=$((elapsed % 60))
    local elapsed_total_str=$(printf "%02d:%02d:%02d" $hours $minutes $seconds)

    # Format last test duration
    local lt_hours=$((last_duration / 3600))
    local lt_minutes=$(((last_duration % 3600) / 60))
    local lt_seconds=$((last_duration % 60))
    local elapsed_last_str=$(printf "%02d:%02d:%02d" $lt_hours $lt_minutes $lt_seconds)

    # Truncate last test name if too long (max 20 chars)
    local display_last_name="$last_test_name"
    if [ ${#display_last_name} -gt 20 ]; then
        display_last_name="${display_last_name:0:17}..."
    fi

    # Calculate percentage
    local total_progress=$(( (current_run - 1) * total_tests + current_test ))
    local total_items=$((total_runs * total_tests))
    local percentage=0
    if [ $total_items -gt 0 ]; then
        percentage=$(( (total_progress * 100) / total_items ))
    fi

    # Build progress bar (20 characters wide)
    local bar_width=20
    local filled=$(( (percentage * bar_width) / 100 ))
    local empty=$((bar_width - filled))

    local bar=""
    for ((i=0; i<filled; i++)); do
        bar="${bar}█"
    done
    for ((i=0; i<empty; i++)); do
        bar="${bar}░"
    done

    # Truncate test name if too long (max 30 chars)
    local display_name="$test_name"
    if [ ${#display_name} -gt 30 ]; then
        display_name="${display_name:0:27}..."
    fi

    # Use scroll region mode with sticky progress bar at bottom
    # The @@PROGRESS@@ marker is for log filtering - we print it then use \r to overwrite it visually
    # Save cursor position
    printf "\033[s"
    # Move to progress line (row ROWS-1) and clear it
    printf "\033[$((PROGRESS_ROWS-1));1H\033[K"
    # Print marker for log filtering, then carriage return to overwrite, then visible separator
    printf "@@PROGRESS@@\r"
    printf "─%.0s" $(seq 1 $PROGRESS_COLS)
    # Move to last row and clear it
    printf "\033[${PROGRESS_ROWS};1H\033[K"
    # Print marker for log filtering, then carriage return to overwrite, then visible progress bar
    printf "@@PROGRESS@@\r"
    # Format the last test info (name + duration) or show "N/A" if no previous test
    local last_info="N/A"
    if [ -n "$display_last_name" ]; then
        last_info="${display_last_name} ${elapsed_last_str}"
    fi
    printf "[Run: %d/%d] [Test: %d/%d] %-30s %s %3d%% [Total: %s] [Last: %-32s]" \
        "$current_run" "$total_runs" \
        "$current_test" "$total_tests" \
        "$display_name" \
        "$bar" \
        "$percentage" \
        "$elapsed_total_str" \
        "$last_info"
    # Restore cursor position
    printf "\033[u"
}

# Set paths
SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )"
CPP_DBC_DIR="${SCRIPT_DIR}"
#TEST_BUILD_DIR="${CPP_DBC_DIR}/build/test_build"
TEST_BUILD_DIR="${CPP_DBC_DIR}/../../build/libs/cpp_dbc/test"
TEST_EXECUTABLE="${TEST_BUILD_DIR}/test/cpp_dbc_tests"

# Check if the tests are built (skip if --skip-build was passed)
if [ "$SKIP_BUILD" = false ] && { [ ! -f "$TEST_EXECUTABLE" ] || [ "$REBUILD" = true ]; }; then
    # Build with appropriate options
    BUILD_CMD="${SCRIPT_DIR}/build_test_cpp_dbc.sh --test"  # Always include --test to ensure tests are built
    if [ "$ENABLE_ASAN" = true ]; then
        BUILD_CMD="$BUILD_CMD --asan"
    fi

    if [ "$USE_CPP_YAML" = "ON" ]; then
        BUILD_CMD="$BUILD_CMD --yaml"
    else
        BUILD_CMD="$BUILD_CMD --yaml-off"
    fi

    if [ "$USE_MYSQL" = "ON" ]; then
        BUILD_CMD="$BUILD_CMD --mysql"
    else
        BUILD_CMD="$BUILD_CMD --mysql-off"
    fi

    if [ "$USE_POSTGRESQL" = "ON" ]; then
        BUILD_CMD="$BUILD_CMD --postgres-on"
    else
        BUILD_CMD="$BUILD_CMD --postgres-off"
    fi
    
    if [ "$USE_SQLITE" = "ON" ]; then
        BUILD_CMD="$BUILD_CMD --sqlite"
    else
        BUILD_CMD="$BUILD_CMD --sqlite-off"
    fi
    
    if [ "$USE_FIREBIRD" = "ON" ]; then
        BUILD_CMD="$BUILD_CMD --firebird"
    else
        BUILD_CMD="$BUILD_CMD --firebird-off"
    fi
    
    if [ "$USE_MONGODB" = "ON" ]; then
        BUILD_CMD="$BUILD_CMD --mongodb"
    else
        BUILD_CMD="$BUILD_CMD --mongodb-off"
    fi

    if [ "$USE_SCYLLADB" = "ON" ]; then
        BUILD_CMD="$BUILD_CMD --scylladb"
    else
        BUILD_CMD="$BUILD_CMD --scylladb-off"
    fi
    
    if [ "$USE_REDIS" = "ON" ]; then
        BUILD_CMD="$BUILD_CMD --redis"
    else
        BUILD_CMD="$BUILD_CMD --redis-off"
    fi
    
    if [ "$BUILD_TYPE" = "Release" ]; then
        BUILD_CMD="$BUILD_CMD --release"
    else
        BUILD_CMD="$BUILD_CMD --debug"
    fi

    if [ "$ENABLE_GCC_ANALYZER" = "ON" ]; then
        BUILD_CMD="$BUILD_CMD --gcc-analyzer"
    fi

    # Add debug options
    if [ "$DEBUG_CONNECTION_POOL" = "ON" ]; then
        BUILD_CMD="$BUILD_CMD --debug-pool"
    fi

    if [ "$DEBUG_TRANSACTION_MANAGER" = "ON" ]; then
        BUILD_CMD="$BUILD_CMD --debug-txmgr"
    fi

    if [ "$DEBUG_SQLITE" = "ON" ]; then
        BUILD_CMD="$BUILD_CMD --debug-sqlite"
    fi

    if [ "$DEBUG_FIREBIRD" = "ON" ]; then
        BUILD_CMD="$BUILD_CMD --debug-firebird"
    fi
    
    if [ "$DEBUG_MONGODB" = "ON" ]; then
        BUILD_CMD="$BUILD_CMD --debug-mongodb"
    fi

    if [ "$DEBUG_SCYLLADB" = "ON" ]; then
        BUILD_CMD="$BUILD_CMD --debug-scylladb"
    fi
    
    if [ "$DEBUG_REDIS" = "ON" ]; then
        BUILD_CMD="$BUILD_CMD --debug-redis"
    fi
    
    # Add dw-off option if specified
    if [ "$BACKWARD_HAS_DW" = "OFF" ]; then
        BUILD_CMD="$BUILD_CMD --dw-off"
    fi

    # Add db-driver-thread-safe-off option if specified
    if [ "$DB_DRIVER_THREAD_SAFE" = "OFF" ]; then
        BUILD_CMD="$BUILD_CMD --db-driver-thread-safe-off"
    fi

    # Execute the build command
    echo "Building tests with command: $BUILD_CMD"
    $BUILD_CMD
else
    echo "Using existing test executable. Use --rebuild to force a rebuild."
fi

# Turn off command echoing if it was enabled by build_test_cpp_dbc.sh
{ set +x; } 2>/dev/null

echo -e "\nRunning tests..."

# Check shared library dependencies if requested
if [ "$CHECK_DEPENDENCIES" = true ]; then
    echo -e "\n========== Checking Shared Library Dependencies ==========\n"
    echo -e "Running ldd on cpp_dbc_tests executable...\n"
    ldd "$TEST_EXECUTABLE"
    echo -e "\n========== End of Dependencies Check ==========\n"
    
    # If checking dependencies, exit here without running tests
    exit 0
fi

echo -e "\n========== Running Tests ==========\n"

# Function to run the test executable with appropriate options
run_test() {
    cd "$TEST_BUILD_DIR"
    
    # Force colored output for Catch2 tests
    # This ensures colors are preserved even when output is piped
    local COLOR_ARGS="--colour-mode=ansi"
    
    if [ "$USE_VALGRIND" = true ]; then
        # Using Valgrind
        # Check if suppression file exists
        #SUPPRESSION_FILE="${SCRIPT_DIR}/valgrind-suppressions.txt"
        #VALGRIND_OPTS="--leak-check=full --show-leak-kinds=all --track-origins=yes --verbose"
        VALGRIND_OPTS="--leak-check=full --show-leak-kinds=definite,indirect,possible --track-origins=yes --verbose"
        
        #if [ -f "$SUPPRESSION_FILE" ]; then
        #    echo "Using Valgrind suppression file: $SUPPRESSION_FILE"
        #    VALGRIND_OPTS="$VALGRIND_OPTS --suppressions=$SUPPRESSION_FILE"
        #fi
        
        if [ -n "$ASAN_OPTIONS" ]; then
            env ASAN_OPTIONS="$ASAN_OPTIONS" valgrind $VALGRIND_OPTS "$TEST_EXECUTABLE" $COLOR_ARGS $@
        else
            valgrind $VALGRIND_OPTS "$TEST_EXECUTABLE" $COLOR_ARGS $@
        fi
    else
        # Not using Valgrind
        if [ -n "$ASAN_OPTIONS" ]; then
            env ASAN_OPTIONS="$ASAN_OPTIONS" "$TEST_EXECUTABLE" $COLOR_ARGS $@
        else
            "$TEST_EXECUTABLE" $COLOR_ARGS $@
        fi
    fi
}

# Handle the --list option
if [ "$LIST_ONLY" = true ]; then
    echo -e "Listing available tests:\n"
    run_test --list-tests
    echo -e "\nListing available tags:\n"
    run_test --list-tags
    exit 0
fi

# Initialize progress tracking variables
PROGRESS_START_TIME=$(date +%s)
CURRENT_TEST_START_TIME=$(date +%s)
LAST_TEST_DURATION=0
LAST_TEST_NAME=""
TOTAL_TESTS=0
TAGS_ARRAY=()

# Setup scroll region for progress bar if enabled
if [ "$SHOW_PROGRESS" = true ]; then
    setup_progress_scroll_region
    # Set trap to restore scroll region on exit
    trap restore_progress_scroll_region EXIT
fi

# Pre-count tests for progress bar if enabled
if [ "$SHOW_PROGRESS" = true ] && [ "$RUN_CTEST" = false ]; then
    echo -e "Counting available tests for progress tracking...\n"
    TAGS=$(run_test --list-tags 2>/dev/null | grep -oP '\[\K[^\]]+' | sort -u)
    TAGS_ARRAY=($TAGS)
    TOTAL_TESTS=${#TAGS_ARRAY[@]}
    echo -e "Found $TOTAL_TESTS test tags to run.\n"
fi

# Run all test sets multiple times
for ((run=1; run<=RUN_COUNT; run++)); do
    echo -e "\n========== Run $run of $RUN_COUNT ==========\n"

    # Run tests with CTest if requested
    if [ "$RUN_CTEST" = true ]; then
        echo -e "Running tests with CTest (Run $run of $RUN_COUNT)...\n"
        cd "$TEST_BUILD_DIR"
        
        if [ -n "$ASAN_OPTIONS" ]; then
            env ASAN_OPTIONS="$ASAN_OPTIONS" ctest -C "$BUILD_TYPE" --output-on-failure
        else
            ctest -C "$BUILD_TYPE" --output-on-failure
        fi
    else
        # Run tests directly
        echo -e "Running tests directly (Run $run of $RUN_COUNT)...\n"
        
        # First list all available tests (only on first run) 
        if [ "$run" -eq 1 ]; then
            echo -e "Available tests:\n"
            run_test --list-tests
        fi
        
        # If a specific test tag was provided, run only that test
        if [ -n "$RUN_SPECIFIC_TEST" ]; then
            echo -e "\nDebug: RUN_SPECIFIC_TEST value: '$RUN_SPECIFIC_TEST'"

            # Check if it's a wildcard pattern (contains *)
            if [[ "$RUN_SPECIFIC_TEST" == *"*"* ]]; then
                # Wildcard pattern - get all available tags and filter matching ones
                echo -e "\nWildcard pattern detected: '$RUN_SPECIFIC_TEST'"
                echo -e "Getting available tags that match the pattern...\n"

                # Get all available tags
                ALL_TAGS=$(run_test --list-tags 2>/dev/null | grep -oP '\[\K[^\]]+' | sort -u)

                # Filter tags that match the wildcard pattern
                MATCHING_TAGS=""
                for TAG in $ALL_TAGS; do
                    # Use bash pattern matching to check if tag matches the wildcard
                    if [[ "$TAG" == $RUN_SPECIFIC_TEST ]]; then
                        if [ -z "$MATCHING_TAGS" ]; then
                            MATCHING_TAGS="$TAG"
                        else
                            MATCHING_TAGS="$MATCHING_TAGS $TAG"
                        fi
                    fi
                done

                if [ -z "$MATCHING_TAGS" ]; then
                    echo -e "No tags found matching pattern '$RUN_SPECIFIC_TEST'"
                else
                    echo -e "Found matching tags: $MATCHING_TAGS\n"
                    # Count matching tags for progress
                    MATCHING_TAGS_ARRAY=($MATCHING_TAGS)
                    TOTAL_TESTS=${#MATCHING_TAGS_ARRAY[@]}
                    CURRENT_TEST=0
                    # Run each matching tag as an exact tag
                    for MATCH_TAG in $MATCHING_TAGS; do
                        CURRENT_TEST=$((CURRENT_TEST + 1))
                        # Show progress bar and track test timing if enabled
                        if [ "$SHOW_PROGRESS" = true ]; then
                            show_progress_bar "$run" "$RUN_COUNT" "$CURRENT_TEST" "$TOTAL_TESTS" "$MATCH_TAG" "$PROGRESS_START_TIME" "$LAST_TEST_DURATION" "$LAST_TEST_NAME"
                        fi
                        # Always track test start time for duration logging
                        CURRENT_TEST_START_TIME=$(date +%s)
                        echo -e "\nRunning tests with tag [$MATCH_TAG] (Run $run of $RUN_COUNT)...\n"
                        run_test -s -r compact "[$MATCH_TAG]"
                        # Calculate and log duration of this test
                        LAST_TEST_DURATION=$(($(date +%s) - CURRENT_TEST_START_TIME))
                        LAST_TEST_NAME="$MATCH_TAG"
                        # Print duration marker for log parsing
                        printf "@@TEST_DURATION:${MATCH_TAG}:${LAST_TEST_DURATION}@@\n"
                    done
                fi
            # Check if there are multiple tags separated by plus sign
            elif [[ "$RUN_SPECIFIC_TEST" == *"+"* ]]; then
                # Multiple tags separated by plus sign
                IFS='+' read -ra TEST_TAGS <<< "$RUN_SPECIFIC_TEST"
                echo -e "Debug: Multiple tags detected: ${#TEST_TAGS[@]}"
                TOTAL_TESTS=${#TEST_TAGS[@]}
                CURRENT_TEST=0

                # Run each tag separately
                for TEST_TAG in "${TEST_TAGS[@]}"; do
                    CURRENT_TEST=$((CURRENT_TEST + 1))
                    # Show progress bar and track test timing if enabled
                    if [ "$SHOW_PROGRESS" = true ]; then
                        show_progress_bar "$run" "$RUN_COUNT" "$CURRENT_TEST" "$TOTAL_TESTS" "$TEST_TAG" "$PROGRESS_START_TIME" "$LAST_TEST_DURATION" "$LAST_TEST_NAME"
                    fi
                    # Always track test start time for duration logging
                    CURRENT_TEST_START_TIME=$(date +%s)
                    echo -e "\nRunning tests with tag [$TEST_TAG] (Run $run of $RUN_COUNT)...\n"
                    run_test -s -r compact "[$TEST_TAG]"
                    # Calculate and log duration of this test
                    LAST_TEST_DURATION=$(($(date +%s) - CURRENT_TEST_START_TIME))
                    LAST_TEST_NAME="$TEST_TAG"
                    # Print duration marker for log parsing
                    printf "@@TEST_DURATION:${TEST_TAG}:${LAST_TEST_DURATION}@@\n"
                done
            else
                # Single tag - add brackets
                TOTAL_TESTS=1
                CURRENT_TEST=1
                # Show progress bar and track test timing if enabled
                if [ "$SHOW_PROGRESS" = true ]; then
                    show_progress_bar "$run" "$RUN_COUNT" "$CURRENT_TEST" "$TOTAL_TESTS" "$RUN_SPECIFIC_TEST" "$PROGRESS_START_TIME" "$LAST_TEST_DURATION" "$LAST_TEST_NAME"
                fi
                # Always track test start time for duration logging
                CURRENT_TEST_START_TIME=$(date +%s)
                echo -e "\nRunning tests with tag [$RUN_SPECIFIC_TEST] (Run $run of $RUN_COUNT)...\n"
                run_test -s -r compact "[$RUN_SPECIFIC_TEST]"
                # Calculate and log duration of this test
                LAST_TEST_DURATION=$(($(date +%s) - CURRENT_TEST_START_TIME))
                LAST_TEST_NAME="$RUN_SPECIFIC_TEST"
                # Print duration marker for log parsing
                printf "@@TEST_DURATION:${RUN_SPECIFIC_TEST}:${LAST_TEST_DURATION}@@\n"
            fi
        else
            # Then run the tests with detailed output
            echo -e "\nRunning tests with detailed output (Run $run of $RUN_COUNT):\n"

            # Get all test tags (only on first run, or if not already obtained for progress)
            if [ "$run" -eq 1 ] && [ ${#TAGS_ARRAY[@]} -eq 0 ]; then
                echo -e "Getting all test tags...\n"
                TAGS=$(run_test --list-tags | grep -oP '\[\K[^\]]+' | sort -u)
                TAGS_ARRAY=($TAGS)
                TOTAL_TESTS=${#TAGS_ARRAY[@]}
            elif [ ${#TAGS_ARRAY[@]} -gt 0 ]; then
                # Use pre-computed tags array
                TAGS="${TAGS_ARRAY[*]}"
            fi

            # Initialize test counter for this run
            CURRENT_TEST=0

            # Run tests by tags to avoid hanging
            for TAG in $TAGS; do
                # Increment test counter
                CURRENT_TEST=$((CURRENT_TEST + 1))

                # Show progress bar and track test timing if enabled (before test output)
                if [ "$SHOW_PROGRESS" = true ]; then
                    show_progress_bar "$run" "$RUN_COUNT" "$CURRENT_TEST" "$TOTAL_TESTS" "$TAG" "$PROGRESS_START_TIME" "$LAST_TEST_DURATION" "$LAST_TEST_NAME"
                fi
                # Always track test start time for duration logging
                CURRENT_TEST_START_TIME=$(date +%s)

                echo -e "\nRunning tests with tag [$TAG] (Run $run of $RUN_COUNT)...\n"

                # Run the test with real-time output while also capturing it for validation
                echo -e "\nRunning test with tag [$TAG] (Run $run of $RUN_COUNT)...\n"
                # Use a temporary file to capture output
                TEST_OUTPUT_FILE=$(mktemp)
                # Force color output
                export CLICOLOR_FORCE=1
                export FORCE_COLOR=1
                # Run the test and tee the output to both the terminal and the temp file
                run_test -s -r compact "[$TAG]" 2>&1 | tee "$TEST_OUTPUT_FILE" || true
                # Store the exit code (this will be the exit code of run_test, not tee)
                TEST_RESULT=${PIPESTATUS[0]}
                # Read the output from the file
                TEST_OUTPUT=$(cat "$TEST_OUTPUT_FILE")
                # Remove the temporary file
                rm -f "$TEST_OUTPUT_FILE"
                
                # Check if this is just a skipped test (not a failure)
                SKIPPED_TEST=false

                # Exit code 4 is used by Catch2 for skipped tests
                if [ $TEST_RESULT -eq 4 ]; then
                    SKIPPED_TEST=true
                fi

                # Also check output patterns as a backup
                if echo "$TEST_OUTPUT" | grep -q "skipped:" && echo "$TEST_OUTPUT" | grep -q "test cases: .* | .* skipped"; then
                    # Also check that there are no failures reported
                    if ! echo "$TEST_OUTPUT" | grep -q "failed"; then
                        SKIPPED_TEST=true
                    fi
                fi

                # Helper function to log test duration (must be called before any continue)
                log_test_duration() {
                    LAST_TEST_DURATION=$(($(date +%s) - CURRENT_TEST_START_TIME))
                    LAST_TEST_NAME="$TAG"
                    # Print duration marker for log parsing
                    printf "@@TEST_DURATION:${TAG}:${LAST_TEST_DURATION}@@\n"
                }

                # Check if we should continue automatically
                if [ "$AUTO_CONTINUE" = true ]; then
                    # Check if this is a skipped test - should continue automatically regardless of exit code
                    if [ "$SKIPPED_TEST" = true ]; then
                        echo -e "\nTest [$TAG] (Run $run of $RUN_COUNT): Test skipped. Continuing automatically..."
                        log_test_duration
                        continue
                    fi

                    # Check if tests passed based on exit code
                    if [ $TEST_RESULT -eq 0 ]; then
                        # If valgrind is enabled, check for memory leaks
                        if [ "$USE_VALGRIND" = true ]; then
                            # Special case for GSSAPI leaks if --gssapi-leak-ok is set
                            if [ "$GSSAPI_LEAK_OK" = true ] && echo "$TEST_OUTPUT" | grep -q "PostgreSQL_GSSAPI_leak"; then
                                # Check if there are no errors despite the GSSAPI leaks
                                if echo "$TEST_OUTPUT" | grep -q "ERROR SUMMARY: 0 errors from 0 contexts"; then
                                    echo -e "\nGSSAPI leaks detected but ignored as requested. Continuing to next test..."
                                    log_test_duration
                                    continue
                                fi
                            fi

                                # Standard case - check for no real memory leaks
                                # Check for either:
                                # 1. All heap blocks were freed (perfect case), OR
                                # 2. No real leaks (definitely/indirectly/possibly lost are all 0)
                                if echo "$TEST_OUTPUT" | grep -q "ERROR SUMMARY: 0 errors from 0 contexts" && \
                                   { echo "$TEST_OUTPUT" | grep -q "All heap blocks were freed -- no leaks are possible" || \
                                     { echo "$TEST_OUTPUT" | grep -q "definitely lost: 0 bytes in 0 blocks" && \
                                       echo "$TEST_OUTPUT" | grep -q "indirectly lost: 0 bytes in 0 blocks" && \
                                       echo "$TEST_OUTPUT" | grep -q "possibly lost: 0 bytes in 0 blocks"; }; }; then
                                    echo -e "\nNo real memory leaks detected. Continuing to next test..."
                                    log_test_duration
                                    continue
                                else
                                echo -e "\nTest [$TAG] (Run $run of $RUN_COUNT): Memory leaks or errors detected. Press Enter to continue or ESC to abort..."
                                read -n 1 key
                                if [[ $key = $'\e' ]]; then
                                    echo -e "\nTest execution aborted by user during test [$TAG] in run $run of $RUN_COUNT."
                                    exit 1
                                fi
                            fi
                        else
                            # No valgrind, just continue if tests passed
                            echo -e "\nAll tests passed. Continuing to next test..."
                            log_test_duration
                            continue
                        fi
                    else
                        # Let's try a different approach to detect skipped tests
                        if echo "$TEST_OUTPUT" | grep -q "skipped:" && ! echo "$TEST_OUTPUT" | grep -q "failed:"; then
                            if [ "$AUTO_CONTINUE" = true ]; then
                                echo -e "\nTest [$TAG] (Run $run of $RUN_COUNT): Test skipped (alt detection). Continuing automatically..."
                                log_test_duration
                                continue
                            fi
                        fi

                        echo -e "\nTest [$TAG] (Run $run of $RUN_COUNT): Tests failed. Press Enter to continue or ESC to abort..."
                        read -n 1 key
                        if [[ $key = $'\e' ]]; then
                            echo -e "\nTest execution aborted by user during test [$TAG] in run $run of $RUN_COUNT."
                            exit 1
                        fi
                    fi
                else
                    # Original behavior - always wait for user input
                    read -p "Press Enter to continue with the next test..."
                fi
                # Log duration for cases that didn't continue early
                log_test_duration
            done
        fi
    fi
done

# Show final progress bar at 100% and restore scroll region if enabled
if [ "$SHOW_PROGRESS" = true ] && [ "$TOTAL_TESTS" -gt 0 ]; then
    show_progress_bar "$RUN_COUNT" "$RUN_COUNT" "$TOTAL_TESTS" "$TOTAL_TESTS" "Completed" "$PROGRESS_START_TIME" "$LAST_TEST_DURATION" "$LAST_TEST_NAME"
    # Small delay to let user see the 100% progress
    sleep 1
    # Restore scroll region (trap will also call this, but calling explicitly is cleaner)
    restore_progress_scroll_region
fi

echo -e "\n========== All Test Runs Completed ($RUN_COUNT total runs) ==========\n"
echo -e "Test execution completed. All $RUN_COUNT runs have been executed. Check the output above for any errors."