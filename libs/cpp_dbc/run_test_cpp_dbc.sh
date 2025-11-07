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
#   --release              Run in Release mode (default: Debug)
#   --asan                 Enable Address Sanitizer
#   --valgrind             Run tests with Valgrind
#   --auto                 Automatically continue to next test set if tests pass
#   --gssapi-leak-ok       Ignore GSSAPI leaks in PostgreSQL (only with --valgrind)
#   --ctest                Run tests using CTest
#   --check                Check shared library dependencies of test executable
#   --rebuild              Rebuild the test targets before running
#   --list                 List available tests only (does not run tests)
#   --run-test="tag"       Run only tests with the specified tag
#   --run=N                Run all test sets N times (default: 1)
#   --debug-pool           Enable debug output for ConnectionPool
#   --debug-txmgr          Enable debug output for TransactionManager
#   --debug-all            Enable all debug output
#   --help                 Show this help message

# Default values for options
USE_MYSQL=ON
USE_CPP_YAML=OFF
USE_POSTGRESQL=OFF
USE_SQLITE=OFF
BUILD_TYPE=Debug
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
RUN_COUNT=1

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
        --release)
            BUILD_TYPE=Release
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
        --debug-all)
            DEBUG_CONNECTION_POOL=ON
            DEBUG_TRANSACTION_MANAGER=ON
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
            echo "  --release              Run in Release mode (default: Debug)"
            echo "  --asan                 Enable Address Sanitizer"
            echo "  --valgrind             Run tests with Valgrind"
            echo "  --auto                 Automatically continue to next test set if tests pass"
            echo "  --gssapi-leak-ok       Ignore GSSAPI leaks in PostgreSQL (only with --valgrind)"
            echo "  --ctest                Run tests using CTest"
            echo "  --check                Check shared library dependencies of test executable"
            echo "  --rebuild              Rebuild the test targets before running"
            echo "  --list                 List available tests only (does not run tests)"
            echo "  --run-test=\"tag\"       Run only tests with the specified tag"
            echo "  --run=N                Run all test sets N times (default: 1)"
            echo "  --debug-pool           Enable debug output for ConnectionPool"
            echo "  --debug-txmgr          Enable debug output for TransactionManager"
            echo "  --debug-all            Enable all debug output"
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

# Set paths
SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )"
CPP_DBC_DIR="${SCRIPT_DIR}"
#TEST_BUILD_DIR="${CPP_DBC_DIR}/build/test_build"
TEST_BUILD_DIR="${CPP_DBC_DIR}/../../build/libs/cpp_dbc/test"
TEST_EXECUTABLE="${TEST_BUILD_DIR}/test/cpp_dbc_tests"

# Check if the tests are built
if [ ! -f "$TEST_EXECUTABLE" ] || [ "$REBUILD" = true ]; then
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
    
    if [ "$BUILD_TYPE" = "Release" ]; then
        BUILD_CMD="$BUILD_CMD --release"
    else
        BUILD_CMD="$BUILD_CMD --debug"
    fi

    # Add debug options
    if [ "$DEBUG_CONNECTION_POOL" = "ON" ]; then
        BUILD_CMD="$BUILD_CMD --debug-pool"
    fi

    if [ "$DEBUG_TRANSACTION_MANAGER" = "ON" ]; then
        BUILD_CMD="$BUILD_CMD --debug-txmgr"
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
            echo -e "\nRunning tests with tag [$RUN_SPECIFIC_TEST] (Run $run of $RUN_COUNT)...\n"
            run_test -s -r compact "[$RUN_SPECIFIC_TEST]"
        else
            # Then run the tests with detailed output
            echo -e "\nRunning tests with detailed output (Run $run of $RUN_COUNT):\n"
            
            # Get all test tags (only on first run)
            if [ "$run" -eq 1 ]; then
                echo -e "Getting all test tags...\n"
                TAGS=$(run_test --list-tags | grep -oP '\[\K[^\]]+' | sort -u)
            fi
            
            # Run tests by tags to avoid hanging
            for TAG in $TAGS; do
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
                
                # Check if we should continue automatically
                if [ "$AUTO_CONTINUE" = true ]; then
                    # Check if tests passed based on exit code
                    if [ $TEST_RESULT -eq 0 ]; then
                        # If valgrind is enabled, check for memory leaks
                        if [ "$USE_VALGRIND" = true ]; then
                            # Special case for GSSAPI leaks if --gssapi-leak-ok is set
                            if [ "$GSSAPI_LEAK_OK" = true ] && echo "$TEST_OUTPUT" | grep -q "PostgreSQL_GSSAPI_leak"; then
                                # Check if there are no errors despite the GSSAPI leaks
                                if echo "$TEST_OUTPUT" | grep -q "ERROR SUMMARY: 0 errors from 0 contexts"; then
                                    echo -e "\nGSSAPI leaks detected but ignored as requested. Continuing to next test..."
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
                            continue
                        fi
                    else
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
            done
        fi
    fi
done

echo -e "\n========== All Test Runs Completed ($RUN_COUNT total runs) ==========\n"
echo -e "Test execution completed. All $RUN_COUNT runs have been executed. Check the output above for any errors."