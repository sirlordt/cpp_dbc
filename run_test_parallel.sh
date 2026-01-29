#!/bin/bash
# run_test_parallel.sh
# Parallel test runner for cpp_dbc
# Executes test prefixes (10_, 20_, 21_, etc.) in parallel batches
# This script reuses run_test.sh for actual test execution

set -e

# Script directory (project root)
SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )"

# Default values
PARALLEL_COUNT=1
PARALLEL_ORDER=""  # Comma-separated list of prefixes to run first (e.g., "23_,24_")
RUN_COUNT=1
TIMESTAMP=$(date '+%Y-%m-%d-%H-%M-%S')
BUILD_DONE=false
SHOW_TUI=false
GLOBAL_START_TIME=0
SUMMARIZE_MODE=false
SUMMARIZE_FOLDER=""

# Arguments to pass to run_test.sh (everything except --parallel)
# Using array to avoid word-splitting and glob expansion issues
declare -a PASS_THROUGH_ARGS=()

# Log directory for this run
LOG_DIR="$SCRIPT_DIR/logs/test/$TIMESTAMP"

# Associative arrays for tracking
declare -A PREFIX_PIDS          # PID for each running prefix
declare -A PREFIX_CURRENT_RUN   # Current run number for each prefix
declare -A PREFIX_STATUS        # Status: pending, running, completed, failed
declare -A PREFIX_LOG_FILE      # Log file path for current run
declare -A PREFIX_START_TIME    # Start time (epoch seconds) for current run
declare -A PREFIX_FAIL_REASON   # Failure reason (Valgrind error message, etc.)
declare -A PREFIX_TOTAL_TESTS   # Total test count per prefix (known after Run 1)
declare -a ALL_PREFIXES         # Array of all test prefixes

# TUI variables
SELECTED_INDEX=0
TERM_ROWS=0
TERM_COLS=0
LEFT_PANEL_WIDTH=30
RUNNING_PREFIXES_COUNT=0
ORIGINAL_STTY_SETTINGS=""

# Color codes
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
CYAN='\033[0;36m'
WHITE='\033[1;37m'
DIM='\033[2m'
REVERSE='\033[7m'
NC='\033[0m' # No Color

# Validate that a value is a non-negative integer
# Usage: validate_numeric "value" "parameter_name"
# Returns 0 if valid, exits with error if invalid
validate_numeric() {
    local value="$1"
    local param_name="$2"

    if ! [[ "$value" =~ ^[0-9]+$ ]]; then
        echo "Error: $param_name must be a non-negative integer, got: '$value'"
        exit 1
    fi
}

# Parse command line arguments
# Extract --parallel and --run, pass everything else through
parse_arguments() {
    while [[ $# -gt 0 ]]; do
        case $1 in
            --parallel=*)
                PARALLEL_COUNT="${1#*=}"
                validate_numeric "$PARALLEL_COUNT" "--parallel"
                shift
                ;;
            --parallel-order=*|--paralel-order=*)
                PARALLEL_ORDER="${1#*=}"
                shift
                ;;
            --run=*)
                RUN_COUNT="${1#*=}"
                validate_numeric "$RUN_COUNT" "--run"
                # Also pass --run to the test script for its internal use
                PASS_THROUGH_ARGS+=("$1")
                shift
                ;;
            --help)
                echo "Usage: $0 --parallel=N [run_test.sh options]"
                echo ""
                echo "Parallel options:"
                echo "  --parallel=N    Run N test prefixes in parallel (required)"
                echo "  --parallel-order=P1,P2,...  Specify prefixes to run first"
                echo "                              (e.g., --parallel-order=23_,24_)"
                echo "  --progress      Show interactive TUI with split panel view"
                echo "  --summarize     Show summary of the latest test run"
                echo "  --summarize=FOLDER  Show summary of a specific test run folder"
                echo "                      (e.g., --summarize=2026-01-28-14-35-42)"
                echo ""
                echo "All other options are passed directly to run_test.sh"
                echo ""
                echo "Example:"
                echo "  $0 --parallel=3 --sqlite --postgres --mysql --valgrind --auto --run=2"
                echo "  $0 --parallel=3 --progress --sqlite --mysql --auto"
                echo "  $0 --parallel=5 --parallel-order=23_,24_ --sqlite --mysql --auto"
                echo "  $0 --summarize"
                echo "  $0 --summarize=2026-01-28-14-35-42"
                exit 0
                ;;
            --summarize)
                SUMMARIZE_MODE=true
                shift
                ;;
            --summarize=*)
                SUMMARIZE_MODE=true
                SUMMARIZE_FOLDER="${1#*=}"
                shift
                ;;
            --progress)
                SHOW_TUI=true
                shift
                ;;
            *)
                # Pass through to run_test.sh
                PASS_THROUGH_ARGS+=("$1")
                shift
                ;;
        esac
    done
}

# Discover all test prefixes from test files
discover_prefixes() {
    local test_dir="$SCRIPT_DIR/libs/cpp_dbc/test"
    local prefixes=()

    if [ ! -d "$test_dir" ]; then
        echo "Error: Test directory not found: $test_dir"
        exit 1
    fi

    # Find all unique prefixes (e.g., 10_, 20_, 21_, etc.)
    for file in "$test_dir"/*_*_test_*.cpp; do
        if [ -f "$file" ]; then
            local basename=$(basename "$file")
            # Extract prefix (first part before second underscore)
            local prefix=$(echo "$basename" | grep -oP '^\d+_' | head -1)
            if [ -n "$prefix" ]; then
                # Remove trailing underscore for comparison
                local prefix_num="${prefix%_}"
                if [[ ! " ${prefixes[*]} " =~ " ${prefix_num} " ]]; then
                    prefixes+=("$prefix_num")
                fi
            fi
        fi
    done

    # Sort prefixes numerically
    IFS=$'\n' sorted=($(sort -n <<<"${prefixes[*]}")); unset IFS
    ALL_PREFIXES=("${sorted[@]}")

    if [ "$SHOW_TUI" = false ]; then
        echo "Discovered ${#ALL_PREFIXES[@]} test prefixes: ${ALL_PREFIXES[*]}"
    fi
}

# Apply parallel order to prioritize specified prefixes
# Reorders ALL_PREFIXES so that prefixes in PARALLEL_ORDER come first
apply_parallel_order() {
    if [ -z "$PARALLEL_ORDER" ]; then
        return
    fi

    # Parse PARALLEL_ORDER (comma-separated list like "23_,24_" or "23,24")
    local -a priority_prefixes=()
    IFS=',' read -ra order_parts <<< "$PARALLEL_ORDER"
    for part in "${order_parts[@]}"; do
        # Remove trailing underscore if present, and any whitespace
        local prefix=$(echo "$part" | sed 's/_$//' | tr -d ' ')
        if [ -n "$prefix" ]; then
            priority_prefixes+=("$prefix")
        fi
    done

    if [ ${#priority_prefixes[@]} -eq 0 ]; then
        return
    fi

    # Build new ordered array: priority prefixes first, then remaining
    local -a new_order=()
    local -a remaining=()

    # First, add priority prefixes in the order specified (if they exist in ALL_PREFIXES)
    for prio in "${priority_prefixes[@]}"; do
        for prefix in "${ALL_PREFIXES[@]}"; do
            if [ "$prefix" = "$prio" ]; then
                new_order+=("$prefix")
                break
            fi
        done
    done

    # Then, add remaining prefixes in their original order
    for prefix in "${ALL_PREFIXES[@]}"; do
        local found=false
        for prio in "${priority_prefixes[@]}"; do
            if [ "$prefix" = "$prio" ]; then
                found=true
                break
            fi
        done
        if [ "$found" = false ]; then
            remaining+=("$prefix")
        fi
    done

    # Combine: priority prefixes first, then remaining
    ALL_PREFIXES=("${new_order[@]}" "${remaining[@]}")

    if [ "$SHOW_TUI" = false ]; then
        echo "Applied parallel order: ${priority_prefixes[*]} (first)"
        echo "Reordered prefixes: ${ALL_PREFIXES[*]}"
    fi
}

# Validate and adjust parallel count
validate_parallel_count() {
    # Ensure PARALLEL_COUNT is numeric before arithmetic comparisons
    validate_numeric "$PARALLEL_COUNT" "PARALLEL_COUNT"

    local max_parallel=${#ALL_PREFIXES[@]}

    if [ "$PARALLEL_COUNT" -gt "$max_parallel" ]; then
        if [ "$SHOW_TUI" = false ]; then
            echo "Note: parallel=$PARALLEL_COUNT exceeds available prefixes ($max_parallel). Adjusting to $max_parallel."
        fi
        PARALLEL_COUNT=$max_parallel
    fi

    if [ "$PARALLEL_COUNT" -lt 1 ]; then
        PARALLEL_COUNT=1
    fi
}

# Initialize prefix status
initialize_prefixes() {
    for prefix in "${ALL_PREFIXES[@]}"; do
        PREFIX_STATUS[$prefix]="pending"
        PREFIX_CURRENT_RUN[$prefix]=0
    done
}

# Perform initial build before running tests in parallel
do_initial_build() {
    if [ "$BUILD_DONE" = true ]; then
        return 0
    fi

    if [ "$SHOW_TUI" = false ]; then
        echo ""
        echo -e "${CYAN}========================================"
        echo "  Building tests (once before parallel execution)"
        echo "========================================${NC}"
        echo ""
    fi

    # Build command: use --list to just build without running tests
    # Call run_test.sh (which handles option conversion to run_test_cpp_dbc.sh)
    # Using array to avoid word-splitting and glob expansion
    local -a build_cmd=("$SCRIPT_DIR/run_test.sh" "${PASS_THROUGH_ARGS[@]}" --rebuild --list)

    if [ "$SHOW_TUI" = false ]; then
        echo "Build command: ${build_cmd[*]}"
        echo ""
    fi

    # Execute build
    cd "$SCRIPT_DIR"
    if [ "$SHOW_TUI" = true ]; then
        "${build_cmd[@]}" > /dev/null 2>&1
    else
        "${build_cmd[@]}"
    fi

    BUILD_DONE=true

    if [ "$SHOW_TUI" = false ]; then
        echo ""
        echo -e "${GREEN}Build completed successfully${NC}"
        echo ""
    fi
}

# Create log file path for a prefix and run
get_log_file_path() {
    local prefix=$1
    local run_num=$2

    mkdir -p "$LOG_DIR"
    printf "%s/%s_RUN%02d.log" "$LOG_DIR" "$prefix" "$run_num"
}

# Start a test for a prefix
start_test() {
    local prefix=$1
    local run_num=$((PREFIX_CURRENT_RUN[$prefix] + 1))

    PREFIX_CURRENT_RUN[$prefix]=$run_num
    PREFIX_STATUS[$prefix]="running"
    PREFIX_START_TIME[$prefix]=$(date +%s)

    local log_file=$(get_log_file_path "$prefix" "$run_num")
    PREFIX_LOG_FILE[$prefix]="$log_file"

    local filter="${prefix}_*"

    # Build full command as array to avoid word-splitting and glob expansion
    # run_test.sh handles option conversion to run_test_cpp_dbc.sh
    local -a cmd=(
        "$SCRIPT_DIR/run_test.sh"
        "${PASS_THROUGH_ARGS[@]}"
        --skip-build              # Skip build (already done)
        --auto                    # Non-interactive mode
        --run=1                   # Single run (we manage runs)
        "--run-test=$filter"      # Filter for this prefix
    )

    # For logging purposes, create a display string
    local cmd_display="${cmd[*]}"

    # Run the test in background
    (
        cd "$SCRIPT_DIR"

        # Force colored output for terminal programs
        export FORCE_COLOR=1
        export CLICOLOR_FORCE=1

        {
            echo "=== Test Prefix: ${prefix}_ ==="
            echo "=== Run: $run_num of $RUN_COUNT ==="
            echo "=== Started: $(date) ==="
            echo "=== Command: $cmd_display ==="
            echo ""

            # Execute the test command (preserve ANSI colors for TUI display)
            # Strip only carriage returns to clean up progress output
            "${cmd[@]}" 2>&1 | sed -r "s/\r//g"
            exit_code=${PIPESTATUS[0]}

            echo ""
            echo "=== Finished: $(date) ==="
            echo "=== Exit Code: $exit_code ==="

            exit $exit_code
        } > "$log_file" 2>&1

    ) &

    PREFIX_PIDS[$prefix]=$!
}

# Check if a test has completed
check_test_completion() {
    local prefix=$1
    local pid=${PREFIX_PIDS[$prefix]}

    if [ -z "$pid" ]; then
        return 1
    fi

    # Check if process is still running
    if kill -0 "$pid" 2>/dev/null; then
        return 1  # Still running
    fi

    return 0  # Completed
}

# Get exit code of completed test
get_test_exit_code() {
    local prefix=$1
    local pid=${PREFIX_PIDS[$prefix]}
    local rc=0

    # Use safe capture pattern to avoid set -e aborting on non-zero exit
    wait "$pid" 2>/dev/null || rc=$?
    echo "${rc}"
}

# Count running tests
count_running_tests() {
    local count=0
    for prefix in "${ALL_PREFIXES[@]}"; do
        if [ "${PREFIX_STATUS[$prefix]}" = "running" ]; then
            ((count++)) || true
        fi
    done
    echo $count
}

# Get next pending prefix
get_next_pending() {
    # First priority: prefixes that have already started but need more runs
    # This ensures all runs of a prefix complete before starting new prefixes
    for prefix in "${ALL_PREFIXES[@]}"; do
        if [ "${PREFIX_STATUS[$prefix]}" = "pending" ] && [ "${PREFIX_CURRENT_RUN[$prefix]:-0}" -gt 0 ]; then
            echo "$prefix"
            return
        fi
    done

    # Second priority: prefixes that haven't started yet
    for prefix in "${ALL_PREFIXES[@]}"; do
        if [ "${PREFIX_STATUS[$prefix]}" = "pending" ] && [ "${PREFIX_CURRENT_RUN[$prefix]:-0}" -eq 0 ]; then
            echo "$prefix"
            return
        fi
    done

    echo ""
}

# Check if all tests are done
all_tests_done() {
    for prefix in "${ALL_PREFIXES[@]}"; do
        local status=${PREFIX_STATUS[$prefix]}
        if [ "$status" != "completed" ] && [ "$status" != "failed" ]; then
            return 1
        fi
    done
    return 0
}

# Rename log file to indicate failure
# Changes PREFIX_RUNXX.log to PREFIX_RUNXX_fail.log
mark_log_as_failed() {
    local prefix=$1
    local log_file="${PREFIX_LOG_FILE[$prefix]}"

    if [ -f "$log_file" ]; then
        # Replace .log with _fail.log
        local fail_log_file="${log_file%.log}_fail.log"
        mv "$log_file" "$fail_log_file"
        PREFIX_LOG_FILE[$prefix]="$fail_log_file"
    fi
}

# Check for Valgrind errors in log file
# Returns 0 if no Valgrind errors found, 1 if errors found
# Sets VALGRIND_ERROR_MSG with the error description if found
VALGRIND_ERROR_MSG=""
check_valgrind_errors() {
    local log_file=$1
    VALGRIND_ERROR_MSG=""

    if [ ! -f "$log_file" ]; then
        return 0
    fi

    # Check for ERROR SUMMARY with non-zero errors
    local error_summary
    error_summary=$(grep -E "ERROR SUMMARY: [1-9][0-9]* errors" "$log_file" 2>/dev/null | tail -1)
    if [ -n "$error_summary" ]; then
        VALGRIND_ERROR_MSG="Valgrind: $error_summary"
        return 1
    fi

    # Check for memory leaks (definitely lost, indirectly lost, possibly lost)
    local definitely_lost
    definitely_lost=$(grep -E "definitely lost: [1-9][0-9,]* bytes" "$log_file" 2>/dev/null | tail -1)
    if [ -n "$definitely_lost" ]; then
        VALGRIND_ERROR_MSG="Valgrind memory leak: $definitely_lost"
        return 1
    fi

    local indirectly_lost
    indirectly_lost=$(grep -E "indirectly lost: [1-9][0-9,]* bytes" "$log_file" 2>/dev/null | tail -1)
    if [ -n "$indirectly_lost" ]; then
        VALGRIND_ERROR_MSG="Valgrind memory leak: $indirectly_lost"
        return 1
    fi

    local possibly_lost
    possibly_lost=$(grep -E "possibly lost: [1-9][0-9,]* bytes" "$log_file" 2>/dev/null | tail -1)
    if [ -n "$possibly_lost" ]; then
        VALGRIND_ERROR_MSG="Valgrind memory leak: $possibly_lost"
        return 1
    fi

    # Check for invalid read/write operations
    if grep -qE "Invalid read of size" "$log_file" 2>/dev/null; then
        VALGRIND_ERROR_MSG="Valgrind: Invalid read detected"
        return 1
    fi

    if grep -qE "Invalid write of size" "$log_file" 2>/dev/null; then
        VALGRIND_ERROR_MSG="Valgrind: Invalid write detected"
        return 1
    fi

    # Check for uninitialised value usage
    if grep -qE "Conditional jump or move depends on uninitialised value" "$log_file" 2>/dev/null; then
        VALGRIND_ERROR_MSG="Valgrind: Use of uninitialised value"
        return 1
    fi

    if grep -qE "Use of uninitialised value" "$log_file" 2>/dev/null; then
        VALGRIND_ERROR_MSG="Valgrind: Use of uninitialised value"
        return 1
    fi

    # Check for invalid free
    if grep -qE "Invalid free\(\)" "$log_file" 2>/dev/null; then
        VALGRIND_ERROR_MSG="Valgrind: Invalid free() detected"
        return 1
    fi

    # Check for mismatched free/delete
    if grep -qE "Mismatched free\(\) / delete" "$log_file" 2>/dev/null; then
        VALGRIND_ERROR_MSG="Valgrind: Mismatched free/delete detected"
        return 1
    fi

    return 0
}

# Find all failure lines in a log file with line numbers
# Outputs: line_number|error_type|error_message for each error found
find_failure_lines() {
    local log_file=$1

    if [ ! -f "$log_file" ]; then
        return
    fi

    # Use -n flag to get line numbers, -a for binary-safe

    # Check for ERROR SUMMARY with non-zero errors
    grep -an "ERROR SUMMARY: [1-9][0-9]* errors" "$log_file" 2>/dev/null | while IFS=: read -r line_num content; do
        echo "${line_num}|Valgrind|${content}"
    done

    # Check for memory leaks (definitely lost)
    grep -an "definitely lost: [1-9][0-9,]* bytes" "$log_file" 2>/dev/null | while IFS=: read -r line_num content; do
        echo "${line_num}|Memory leak|${content}"
    done

    # Check for memory leaks (indirectly lost)
    grep -an "indirectly lost: [1-9][0-9,]* bytes" "$log_file" 2>/dev/null | while IFS=: read -r line_num content; do
        echo "${line_num}|Memory leak|${content}"
    done

    # Check for memory leaks (possibly lost)
    grep -an "possibly lost: [1-9][0-9,]* bytes" "$log_file" 2>/dev/null | while IFS=: read -r line_num content; do
        echo "${line_num}|Memory leak|${content}"
    done

    # Check for invalid read
    grep -an "Invalid read of size" "$log_file" 2>/dev/null | while IFS=: read -r line_num content; do
        echo "${line_num}|Valgrind|${content}"
    done

    # Check for invalid write
    grep -an "Invalid write of size" "$log_file" 2>/dev/null | while IFS=: read -r line_num content; do
        echo "${line_num}|Valgrind|${content}"
    done

    # Check for uninitialised value usage
    grep -an "Conditional jump or move depends on uninitialised value" "$log_file" 2>/dev/null | while IFS=: read -r line_num content; do
        echo "${line_num}|Valgrind|${content}"
    done

    grep -an "Use of uninitialised value" "$log_file" 2>/dev/null | while IFS=: read -r line_num content; do
        echo "${line_num}|Valgrind|${content}"
    done

    # Check for invalid free
    grep -an "Invalid free()" "$log_file" 2>/dev/null | while IFS=: read -r line_num content; do
        echo "${line_num}|Valgrind|${content}"
    done

    # Check for mismatched free/delete
    grep -an "Mismatched free() / delete" "$log_file" 2>/dev/null | while IFS=: read -r line_num content; do
        echo "${line_num}|Valgrind|${content}"
    done

    # Check for Catch2 test failures (format: /path/file.cpp:123: FAILED:)
    grep -an ": FAILED:" "$log_file" 2>/dev/null | while IFS=: read -r line_num content; do
        # content contains the rest after line_num, which is the cpp path and failure info
        # Extract the source file path and line from the content (format: /path/file.cpp:123: FAILED:)
        local source_info=$(echo "$content" | grep -oE '[^[:space:]]+\.cpp:[0-9]+')
        if [ -n "$source_info" ]; then
            echo "${line_num}|Test FAILED|${source_info}"
        else
            echo "${line_num}|Test FAILED|${content}"
        fi
    done

    # Check for Catch2 test failures with lowercase "failed" (may include ANSI codes)
    # Format: /path/file.cpp:123: [ANSI]failed[ANSI]: assertion for: expected == actual
    grep -an "failed" "$log_file" 2>/dev/null | grep "\.cpp:" | grep -v "test case" | while IFS= read -r full_line; do
        # Extract line number (everything before first colon)
        local line_num=$(echo "$full_line" | cut -d: -f1)
        # Get the content after line number
        local content=$(echo "$full_line" | cut -d: -f2-)
        # Strip ANSI codes for extraction
        local clean_content=$(echo "$content" | sed 's/\x1b\[[0-9;]*m//g')
        # Extract the source file path and line
        local source_info=$(echo "$clean_content" | grep -oE '[^[:space:]]+\.cpp:[0-9]+')
        if [ -n "$source_info" ]; then
            # Also extract the assertion info after "failed:"
            local assertion_info=$(echo "$clean_content" | sed 's/.*failed: //' | head -c 80)
            echo "${line_num}|Test FAILED|${source_info}: ${assertion_info}"
        fi
    done
}

# Count total tests completed in a log file
# Returns the count of completed test cases
count_total_tests_in_log() {
    local log_file=$1

    if [ ! -f "$log_file" ]; then
        echo "0"
        return
    fi

    # Count "All tests passed" or "test case(s) failed" result lines
    local count
    count=$(grep -cE "All tests passed|[0-9]+ test case(s)? failed" "$log_file" 2>/dev/null || echo "0")
    echo "$count"
}

# ============================================================================
# Detailed Summary Functions
# ============================================================================

# Format duration in seconds to HH:MM:SS
format_duration() {
    local seconds=$1
    local hours=$((seconds / 3600))
    local minutes=$(((seconds % 3600) / 60))
    local secs=$((seconds % 60))
    printf "%02d:%02d:%02d" $hours $minutes $secs
}

# Extract test durations from log file
# Returns: tag|duration_seconds for each test
extract_test_durations_from_log() {
    local log_file=$1

    if [ ! -f "$log_file" ]; then
        return
    fi

    # Look for @@TEST_DURATION:tag:seconds@@ pattern
    grep -a "@@TEST_DURATION:" "$log_file" 2>/dev/null | while IFS= read -r line; do
        local tag=$(echo "$line" | sed 's/.*@@TEST_DURATION:\([^:]*\):.*/\1/')
        local duration=$(echo "$line" | sed 's/.*@@TEST_DURATION:[^:]*:\([0-9]*\)@@.*/\1/')
        echo "${tag}|${duration}"
    done
}

# Display detailed summary for all log files in LOG_DIR
# Groups by prefix and shows test execution details
display_detailed_summary() {
    echo ""
    echo "========================================"
    echo "  Detailed Test Execution Summary"
    echo "========================================"
    echo "Log directory: $LOG_DIR/"
    echo ""

    # Process each prefix
    for prefix in "${ALL_PREFIXES[@]}"; do
        local prefix_status="${PREFIX_STATUS[$prefix]}"
        local total_runs=${PREFIX_CURRENT_RUN[$prefix]}

        # Skip if no runs were done
        if [ "$total_runs" -eq 0 ]; then
            continue
        fi

        # Header for prefix (remove leading zeros from run count)
        local runs_display=$((10#$total_runs))
        local failed_run_display=$((10#${PREFIX_FAILED_RUN[$prefix]:-$total_runs}))
        echo ""
        if [ "$prefix_status" = "completed" ]; then
            echo -e "${GREEN}=== Prefix: ${prefix}_ (PASSED - $runs_display runs) ===${NC}"
        elif [ "$prefix_status" = "interrupted" ]; then
            echo -e "${YELLOW}=== Prefix: ${prefix}_ (INTERRUPTED at run $runs_display) ===${NC}"
        else
            echo -e "${RED}=== Prefix: ${prefix}_ (FAILED at run $failed_run_display) ===${NC}"
        fi

        # Collect all tests and their durations for this prefix
        declare -A all_tests
        declare -A run_durations
        declare -a log_files_used=()

        # Process each run's log file
        for ((run=1; run<=total_runs; run++)); do
            local log_file=$(get_log_file_path "$prefix" "$run")

            # Check for failed log file naming
            if [ ! -f "$log_file" ]; then
                local fail_log="${log_file%.log}_fail.log"
                if [ -f "$fail_log" ]; then
                    log_file="$fail_log"
                fi
            fi

            if [ ! -f "$log_file" ]; then
                continue
            fi

            # Track this log file
            log_files_used+=("$log_file")

            # Extract test durations
            while IFS='|' read -r tag duration; do
                [ -z "$tag" ] && continue
                all_tests[$tag]=1
                run_durations["${tag}|${run}"]=$duration
            done < <(extract_test_durations_from_log "$log_file")
        done

        # Sort tests by tag
        local sorted_tags=$(printf '%s\n' "${!all_tests[@]}" | sort)

        if [ -z "$sorted_tags" ]; then
            echo "  No test duration data found in logs."
            continue
        fi

        # Find max tag length for formatting
        local max_tag_len=0
        for tag in $sorted_tags; do
            local len=${#tag}
            [ $len -gt $max_tag_len ] && max_tag_len=$len
        done

        # Build header
        local header="  "
        header+=$(printf "%-${max_tag_len}s" "Test Tag")
        for ((run=1; run<=total_runs; run++)); do
            header+="  $(printf '%8s' "Run $run")"
        done
        if [ "$total_runs" -gt 1 ]; then
            header+="  $(printf '%8s' "Total")"
        fi

        echo "$header"
        echo "  $(printf '%*s' $((max_tag_len + total_runs * 10 + (total_runs > 1 ? 10 : 0))) '' | tr ' ' '-')"

        # Track totals per run
        declare -A run_totals
        local grand_total=0

        # Display each test
        for tag in $sorted_tags; do
            local line="  "
            line+=$(printf "%-${max_tag_len}s" "$tag")

            local test_total=0
            for ((run=1; run<=total_runs; run++)); do
                local dur="${run_durations["${tag}|${run}"]:-0}"
                local dur_str=$(format_duration "$dur")
                line+="  $(printf '%8s' "$dur_str")"
                test_total=$((test_total + dur))
                run_totals[$run]=$((${run_totals[$run]:-0} + dur))
            done

            if [ "$total_runs" -gt 1 ]; then
                local total_str=$(format_duration "$test_total")
                line+="  $(printf '%8s' "$total_str")"
            fi
            grand_total=$((grand_total + test_total))

            echo "$line"
        done

        # Show totals row
        echo "  $(printf '%*s' $((max_tag_len + total_runs * 10 + (total_runs > 1 ? 10 : 0))) '' | tr ' ' '-')"
        local totals_line="  "
        totals_line+=$(printf "%-${max_tag_len}s" "TOTAL")
        for ((run=1; run<=total_runs; run++)); do
            local run_total_str=$(format_duration "${run_totals[$run]:-0}")
            totals_line+="  $(printf '%8s' "$run_total_str")"
        done
        if [ "$total_runs" -gt 1 ]; then
            local grand_str=$(format_duration "$grand_total")
            totals_line+="  $(printf '%8s' "$grand_str")"
        fi
        echo "$totals_line"

        # Show log file paths
        if [ ${#log_files_used[@]} -gt 0 ]; then
            echo ""
            echo "  Log files:"
            for log_path in "${log_files_used[@]}"; do
                echo "    - $log_path"
            done
        fi

        # Clean up associative arrays
        unset all_tests
        unset log_files_used
        unset run_durations
        unset run_totals
    done

    echo ""
}

# ============================================================================
# Summarize Mode Functions
# ============================================================================

# Find the latest log folder in logs/test/
find_latest_log_folder() {
    local log_base="$SCRIPT_DIR/logs/test"

    if [ ! -d "$log_base" ]; then
        echo ""
        return
    fi

    # Find directories matching the timestamp pattern, sort by name (newest last)
    local latest
    latest=$(find "$log_base" -maxdepth 1 -type d -name "20*" | sort | tail -1)

    if [ -n "$latest" ]; then
        basename "$latest"
    fi
}

# Discover prefixes and runs from log files in a folder
# Populates ALL_PREFIXES, PREFIX_STATUS, PREFIX_CURRENT_RUN from log filenames
discover_prefixes_from_logs() {
    local log_folder=$1

    # Reset arrays
    ALL_PREFIXES=()
    declare -gA PREFIX_STATUS
    declare -gA PREFIX_CURRENT_RUN
    declare -gA PREFIX_LOG_FILE
    declare -gA PREFIX_FAILED_RUN

    # Find all log files and extract prefix/run info
    # Log files are named: PREFIX_RUNXX.log or PREFIX_RUNXX_fail.log
    local prefixes_found=()

    for log_file in "$log_folder"/*.log; do
        [ -f "$log_file" ] || continue

        local basename=$(basename "$log_file")
        # Extract prefix (everything before _RUN)
        local prefix=$(echo "$basename" | sed 's/_RUN.*//')
        # Extract run number (portable sed instead of grep -oP)
        local run_num=$(echo "$basename" | sed -n 's/.*_RUN\([0-9][0-9]*\).*/\1/p')
        run_num="${run_num:-0}"
        # Check if it's a failed log
        local is_failed=false
        [[ "$basename" == *"_fail.log" ]] && is_failed=true

        # Add to prefixes if not already there
        if [[ ! " ${prefixes_found[*]} " =~ " ${prefix} " ]]; then
            prefixes_found+=("$prefix")
        fi

        # Track the highest run number for this prefix
        local current_max=${PREFIX_CURRENT_RUN[$prefix]:-0}
        if [ "$run_num" -gt "$current_max" ]; then
            PREFIX_CURRENT_RUN[$prefix]=$run_num
        fi

        # Track status - if any log is a fail log, mark as failed
        if [ "$is_failed" = true ]; then
            PREFIX_STATUS[$prefix]="failed"
            PREFIX_LOG_FILE[$prefix]="$log_file"
            PREFIX_FAILED_RUN[$prefix]=$run_num
        elif [ "${PREFIX_STATUS[$prefix]}" != "failed" ]; then
            PREFIX_STATUS[$prefix]="completed"
        fi
    done

    # Sort prefixes numerically
    IFS=$'\n' sorted=($(sort -n <<<"${prefixes_found[*]}")); unset IFS
    ALL_PREFIXES=("${sorted[@]}")
}

# Run summarize mode - display summary from existing log folder
run_summarize_mode() {
    local log_folder_name=""

    if [ -n "$SUMMARIZE_FOLDER" ]; then
        # Use specified folder
        log_folder_name="$SUMMARIZE_FOLDER"
    else
        # Find latest folder
        log_folder_name=$(find_latest_log_folder)
        if [ -z "$log_folder_name" ]; then
            echo -e "${RED}Error: No log folders found in logs/test/${NC}"
            exit 1
        fi
    fi

    # Set LOG_DIR to the folder
    LOG_DIR="$SCRIPT_DIR/logs/test/$log_folder_name"

    if [ ! -d "$LOG_DIR" ]; then
        echo -e "${RED}Error: Log folder not found: $LOG_DIR${NC}"
        exit 1
    fi

    echo ""
    echo "========================================"
    echo "  Test Run Summary"
    echo "========================================"
    echo "Log folder: $log_folder_name"
    echo "Full path: $LOG_DIR/"

    # Count log files
    local log_count=$(find "$LOG_DIR" -name "*.log" -type f | wc -l)
    echo "Log files: $log_count"

    # Discover prefixes from logs
    discover_prefixes_from_logs "$LOG_DIR"

    if [ ${#ALL_PREFIXES[@]} -eq 0 ]; then
        echo -e "${YELLOW}No test log files found in this folder.${NC}"
        exit 0
    fi

    echo "Prefixes found: ${ALL_PREFIXES[*]}"

    # Determine RUN_COUNT from logs (max run number found)
    local max_run=0
    for prefix in "${ALL_PREFIXES[@]}"; do
        local runs=${PREFIX_CURRENT_RUN[$prefix]:-0}
        [ "$runs" -gt "$max_run" ] && max_run=$runs
    done
    RUN_COUNT=$max_run

    # Display the detailed summary
    display_detailed_summary

    # Print final status
    echo ""
    echo "========================================"
    echo "  Status Summary"
    echo "========================================"
    echo ""

    local completed=0
    local failed=0

    for prefix in "${ALL_PREFIXES[@]}"; do
        local status="${PREFIX_STATUS[$prefix]}"
        local runs=$((10#${PREFIX_CURRENT_RUN[$prefix]}))

        if [ "$status" = "completed" ]; then
            ((completed++)) || true
            echo -e "${GREEN}[PASSED]${NC} ${prefix}_ - $runs run(s)"
        else
            ((failed++)) || true
            local failed_run=$((10#${PREFIX_FAILED_RUN[$prefix]:-$runs}))
            echo -e "${RED}[FAILED]${NC} ${prefix}_ - Failed at run $failed_run"
            # Find the fail log
            local fail_log=$(find "$LOG_DIR" -name "${prefix}_RUN*_fail.log" -type f | head -1)
            if [ -n "$fail_log" ]; then
                # Find all failure lines with line numbers
                local failure_lines
                failure_lines=$(find_failure_lines "$fail_log")

                if [ -n "$failure_lines" ]; then
                    # Display each failure with its line number
                    echo "$failure_lines" | while IFS='|' read -r line_num error_type error_content; do
                        [ -z "$line_num" ] && continue
                        # Trim whitespace from error_content
                        error_content=$(echo "$error_content" | sed 's/^[[:space:]]*//' | sed 's/[[:space:]]*$//')
                        echo "         Log: ${fail_log}:${line_num}"
                        echo "         Reason: [${error_type}] ${error_content}"
                    done
                else
                    # No specific errors found, just show the log file
                    echo "         Log: $fail_log"
                fi
            fi
        fi
    done

    echo ""
    echo "Summary: $completed passed, $failed failed"
    echo ""
}

# ============================================================================
# TUI Functions
# ============================================================================

# Initialize TUI
tui_init() {
    # Get terminal size
    TERM_ROWS=$(tput lines)
    TERM_COLS=$(tput cols)

    # Save original terminal settings and disable echo
    # This prevents escape sequences from being echoed when arrow keys are pressed
    ORIGINAL_STTY_SETTINGS=$(stty -g)
    stty -echo

    # Hide cursor
    tput civis

    # Clear screen
    clear

    # Set up alternate screen buffer
    tput smcup
}

# Cleanup TUI
tui_cleanup() {
    # Restore original terminal settings (re-enables echo)
    if [ -n "$ORIGINAL_STTY_SETTINGS" ]; then
        stty "$ORIGINAL_STTY_SETTINGS"
    else
        stty echo
    fi

    # Show cursor
    tput cnorm

    # Return to main screen buffer
    tput rmcup

    # Clear screen
    clear
}

# Draw a box
tui_draw_box() {
    local x=$1
    local y=$2
    local width=$3
    local height=$4
    local title=$5

    # Top border
    tput cup $y $x
    printf "┌"
    for ((i=1; i<width-1; i++)); do printf "─"; done
    printf "┐"

    # Title if provided
    if [ -n "$title" ]; then
        local title_pos=$((x + 2))
        tput cup $y $title_pos
        printf " %s " "$title"
    fi

    # Sides
    for ((row=1; row<height-1; row++)); do
        tput cup $((y + row)) $x
        printf "│"
        tput cup $((y + row)) $((x + width - 1))
        printf "│"
    done

    # Bottom border
    tput cup $((y + height - 1)) $x
    printf "└"
    for ((i=1; i<width-1; i++)); do printf "─"; done
    printf "┘"
}

# Draw left panel with task list
# Get array of running task prefixes
get_running_prefixes() {
    local -a running=()
    for prefix in "${ALL_PREFIXES[@]}"; do
        if [ "${PREFIX_STATUS[$prefix]}" = "running" ]; then
            running+=("$prefix")
        fi
    done
    echo "${running[@]}"
}

tui_draw_left_panel() {
    local panel_height=$((TERM_ROWS - 4))

    # Get running tasks count
    local running_count=$(count_running_tests)

    # Calculate total elapsed time
    local current_time=$(date +%s)
    local total_elapsed=$((current_time - GLOBAL_START_TIME))
    local total_elapsed_str=$(format_elapsed_time $total_elapsed)

    # Draw box with running count and elapsed time in title
    tui_draw_box 0 0 $LEFT_PANEL_WIDTH $panel_height "Running ($running_count) ($total_elapsed_str)"

    # Available width for content (panel width minus borders)
    local content_width=$((LEFT_PANEL_WIDTH - 2))

    # Build array of running prefixes
    local -a running_prefixes=()
    for prefix in "${ALL_PREFIXES[@]}"; do
        if [ "${PREFIX_STATUS[$prefix]}" = "running" ]; then
            running_prefixes+=("$prefix")
        fi
    done

    # Get new count
    local new_count=${#running_prefixes[@]}

    # Only clear extra rows if list shrank (to remove stale entries)
    # Each row is already padded with spaces, so existing content will be overwritten naturally
    if [ $new_count -lt $RUNNING_PREFIXES_COUNT ]; then
        for ((clear_row=new_count+1; clear_row<=RUNNING_PREFIXES_COUNT && clear_row<panel_height-1; clear_row++)); do
            tput cup $clear_row 1
            printf "%${content_width}s" ""
        done
    fi

    # Draw only running tasks
    local row=1
    local idx=0
    for prefix in "${running_prefixes[@]}"; do
        if [ $row -ge $((panel_height - 1)) ]; then
            break
        fi

        local run_num="${PREFIX_CURRENT_RUN[$prefix]}"
        local total_tests="${PREFIX_TOTAL_TESTS[$prefix]}"
        local log_file="${PREFIX_LOG_FILE[$prefix]}"
        local test_start_time="${PREFIX_START_TIME[$prefix]}"

        # Calculate elapsed time for this specific test
        local test_elapsed=0
        if [ -n "$test_start_time" ] && [ "$test_start_time" -gt 0 ]; then
            local now=$(date +%s)
            test_elapsed=$((now - test_start_time))
        fi
        local test_elapsed_str=$(format_elapsed_time $test_elapsed)

        # Get progress info
        local progress_info=$(get_test_progress_info "$log_file")
        local completed_tests
        IFS='|' read -r _ completed_tests <<< "$progress_info"
        completed_tests="${completed_tests:-0}"

        # Format progress string
        local progress_str
        if [ -n "$total_tests" ] && [ "$total_tests" -gt 0 ]; then
            progress_str="$completed_tests/$total_tests"
        else
            progress_str="$completed_tests"
        fi

        tput cup $row 1

        # Build the label text with elapsed time
        local label=$(printf "%s_ R%d (%s) %s" "$prefix" "$run_num" "$progress_str" "$test_elapsed_str")

        # Truncate label to fit content width
        label="${label:0:$content_width}"

        # Pad with spaces to fill the content width
        local padded_label=$(printf "%-${content_width}s" "$label")

        # Print with colors and highlight
        if [ $idx -eq $SELECTED_INDEX ]; then
            printf "${REVERSE}${YELLOW}%s${NC}" "$padded_label"
        else
            printf "${YELLOW}%s${NC}" "$padded_label"
        fi

        ((row++)) || true
        ((idx++)) || true
    done

    # Store running prefixes count for navigation bounds
    RUNNING_PREFIXES_COUNT=${#running_prefixes[@]}
}

# Draw right panel with log content
tui_draw_right_panel() {
    local panel_x=$LEFT_PANEL_WIDTH
    local panel_width=$((TERM_COLS - LEFT_PANEL_WIDTH))
    local panel_height=$((TERM_ROWS - 4))

    # Build array of running prefixes
    local -a running_prefixes=()
    for prefix in "${ALL_PREFIXES[@]}"; do
        if [ "${PREFIX_STATUS[$prefix]}" = "running" ]; then
            running_prefixes+=("$prefix")
        fi
    done

    # Get selected prefix from running tasks
    local selected_prefix=""
    local log_file=""
    local title="Output"

    if [ ${#running_prefixes[@]} -gt 0 ] && [ $SELECTED_INDEX -lt ${#running_prefixes[@]} ]; then
        selected_prefix="${running_prefixes[$SELECTED_INDEX]}"
        log_file="${PREFIX_LOG_FILE[$selected_prefix]}"
        title="Output: ${selected_prefix}_"
    fi

    # Draw box
    tui_draw_box $panel_x 0 $panel_width $panel_height "$title"

    # Draw log content
    local content_width=$((panel_width - 2))
    local content_height=$((panel_height - 2))

    if [ -f "$log_file" ]; then
        # Get last N lines of log file
        local lines
        mapfile -t lines < <(tail -n $content_height "$log_file" 2>/dev/null)

        local row=1
        for line in "${lines[@]}"; do
            if [ $row -ge $content_height ]; then
                break
            fi

            tput cup $row $((panel_x + 1))

            # Calculate visible length (without ANSI codes) for truncation
            local visible_line
            visible_line=$(printf "%s" "$line" | sed 's/\x1b\[[0-9;]*m//g')
            local visible_len=${#visible_line}

            if [ $visible_len -le $content_width ]; then
                # Line fits - print with colors preserved, then pad with spaces
                printf "%s" "$line"
                local padding=$((content_width - visible_len))
                if [ $padding -gt 0 ]; then
                    printf "%${padding}s" ""
                fi
            else
                # Line too long - truncate visible content while preserving colors
                # This is a simplified approach: print and let terminal handle overflow
                # Then clear to our boundary
                local truncated_visible="${visible_line:0:$((content_width - 3))}..."
                # For colored output, we need smarter truncation
                # Use a character-by-character approach to preserve ANSI sequences
                local char_count=0
                local i=0
                local result=""
                local in_escape=false
                local line_len=${#line}

                while [ $i -lt $line_len ] && [ $char_count -lt $((content_width - 3)) ]; do
                    local char="${line:$i:1}"
                    if [ "$char" = $'\x1b' ]; then
                        # Start of ANSI escape sequence
                        in_escape=true
                        result+="$char"
                    elif [ "$in_escape" = true ]; then
                        result+="$char"
                        # End of escape sequence is 'm'
                        if [ "$char" = "m" ]; then
                            in_escape=false
                        fi
                    else
                        result+="$char"
                        ((char_count++)) || true
                    fi
                    ((i++)) || true
                done

                # Print truncated content with colors, add ellipsis, reset color, pad
                printf "%s${NC}..." "$result"
                local remaining=$((content_width - char_count - 3))
                if [ $remaining -gt 0 ]; then
                    printf "%${remaining}s" ""
                fi
            fi

            ((row++)) || true
        done

        # Clear remaining rows
        while [ $row -lt $content_height ]; do
            tput cup $row $((panel_x + 1))
            printf "%${content_width}s" ""
            ((row++)) || true
        done
    else
        tput cup 2 $((panel_x + 2))
        printf "${DIM}(waiting for log...)${NC}"
    fi
}

# Draw status bar
tui_draw_status_bar() {
    local y=$((TERM_ROWS - 3))

    tput cup $y 0
    printf "${REVERSE}"

    local running=$(count_running_tests)
    local completed=0
    local failed=0

    for prefix in "${ALL_PREFIXES[@]}"; do
        case "${PREFIX_STATUS[$prefix]}" in
            completed) ((completed++)) || true ;;
            failed)    ((failed++)) || true ;;
        esac
    done

    # Calculate total elapsed time
    local current_time=$(date +%s)
    local total_elapsed=$((current_time - GLOBAL_START_TIME))
    local total_elapsed_str=$(format_elapsed_time $total_elapsed)

    local status_text=" Running: $running | Completed: $completed | Failed: $failed | Time: $total_elapsed_str "
    printf "%-${TERM_COLS}s" "$status_text"
    printf "${NC}"

    # Progress line for selected task
    tput cup $((y + 1)) 0

    # Build array of running prefixes
    local -a running_prefixes=()
    for prefix in "${ALL_PREFIXES[@]}"; do
        if [ "${PREFIX_STATUS[$prefix]}" = "running" ]; then
            running_prefixes+=("$prefix")
        fi
    done

    local info_text=""
    if [ ${#running_prefixes[@]} -gt 0 ] && [ $SELECTED_INDEX -lt ${#running_prefixes[@]} ]; then
        local selected_prefix="${running_prefixes[$SELECTED_INDEX]}"
        local log_file="${PREFIX_LOG_FILE[$selected_prefix]}"
        local total_tests="${PREFIX_TOTAL_TESTS[$selected_prefix]}"
        local progress_info=$(get_test_progress_info "$log_file")
        local test_name completed_tests
        IFS='|' read -r test_name completed_tests <<< "$progress_info"
        test_name="${test_name:-waiting...}"
        completed_tests="${completed_tests:-0}"

        local progress_str
        if [ -n "$total_tests" ] && [ "$total_tests" -gt 0 ]; then
            progress_str="$completed_tests of $total_tests"
        else
            progress_str="$completed_tests completed"
        fi

        info_text=" ${selected_prefix}_ (${progress_str}) ${test_name} | arrows/jk=nav q=quit "
    else
        info_text=" Waiting for tasks... | q=quit "
    fi

    # Clear the entire line by padding to terminal width
    printf "${DIM}%-${TERM_COLS}s${NC}" "$info_text"
}

# Main TUI draw function
tui_draw() {
    TERM_ROWS=$(tput lines)
    TERM_COLS=$(tput cols)

    tui_draw_left_panel
    tui_draw_right_panel
    tui_draw_status_bar
}

# Handle key input (non-blocking)
tui_handle_input() {
    local key
    read -s -t 0.1 -n 1 key 2>/dev/null || true

    # Skip if no input
    [ -z "$key" ] && return 0

    case "$key" in
        q|Q)
            return 1  # Signal to quit
            ;;
        $'\x1b')
            # Escape sequence - read next characters for arrow keys
            # Use longer timeout (0.1s) to ensure we capture the full sequence
            read -s -t 0.1 -n 2 key 2>/dev/null || true
            case "$key" in
                '[A')  # Up arrow
                    if [ $SELECTED_INDEX -gt 0 ]; then
                        ((SELECTED_INDEX--)) || true
                    fi
                    ;;
                '[B')  # Down arrow
                    if [ $SELECTED_INDEX -lt $((RUNNING_PREFIXES_COUNT - 1)) ]; then
                        ((SELECTED_INDEX++)) || true
                    fi
                    ;;
            esac
            # Flush any remaining input to prevent leakage
            while read -s -t 0.001 -n 1 2>/dev/null; do :; done
            ;;
        k|K)  # vim-style up
            if [ $SELECTED_INDEX -gt 0 ]; then
                ((SELECTED_INDEX--)) || true
            fi
            ;;
        j|J)  # vim-style down
            if [ $SELECTED_INDEX -lt $((RUNNING_PREFIXES_COUNT - 1)) ]; then
                ((SELECTED_INDEX++)) || true
            fi
            ;;
        '[')
            # Stray bracket from incomplete escape sequence - flush remaining
            while read -s -t 0.001 -n 1 2>/dev/null; do :; done
            ;;
    esac

    return 0
}

# Run TUI main loop
run_parallel_tests_tui() {
    # Initialize TUI
    tui_init

    # Create log directory
    mkdir -p "$LOG_DIR"

    # Do initial build (silently for TUI)
    do_initial_build

    # Set global start time for total elapsed tracking
    GLOBAL_START_TIME=$(date +%s)

    # Main loop
    while ! all_tests_done; do
        # Check for completed tests
        for prefix in "${ALL_PREFIXES[@]}"; do
            if [ "${PREFIX_STATUS[$prefix]}" = "running" ]; then
                if check_test_completion "$prefix"; then
                    local exit_code=$(get_test_exit_code "$prefix")
                    local current_run=${PREFIX_CURRENT_RUN[$prefix]}
                    local log_file="${PREFIX_LOG_FILE[$prefix]}"

                    if [ $exit_code -eq 0 ]; then
                        # Check for Valgrind errors even if exit code is 0
                        if ! check_valgrind_errors "$log_file"; then
                            # Valgrind errors found - treat as failure
                            PREFIX_STATUS[$prefix]="failed"
                            PREFIX_FAIL_REASON[$prefix]="$VALGRIND_ERROR_MSG"
                            mark_log_as_failed "$prefix"
                        else
                            # Run passed - store total test count for future runs
                            if [ -z "${PREFIX_TOTAL_TESTS[$prefix]}" ]; then
                                PREFIX_TOTAL_TESTS[$prefix]=$(count_total_tests_in_log "$log_file")
                            fi

                            if [ $current_run -lt $RUN_COUNT ]; then
                                PREFIX_STATUS[$prefix]="pending"
                            else
                                PREFIX_STATUS[$prefix]="completed"
                            fi
                        fi
                    else
                        PREFIX_STATUS[$prefix]="failed"
                        PREFIX_FAIL_REASON[$prefix]="Exit code: $exit_code"
                        mark_log_as_failed "$prefix"
                    fi

                    unset "PREFIX_PIDS[$prefix]"
                fi
            fi
        done

        # Start new tests if we have capacity
        local running_count=$(count_running_tests)
        while [ $running_count -lt $PARALLEL_COUNT ]; do
            local next_prefix=$(get_next_pending)
            if [ -z "$next_prefix" ]; then
                break
            fi

            start_test "$next_prefix"
            ((running_count++)) || true
        done

        # Adjust SELECTED_INDEX if it's out of bounds for running tasks
        running_count=$(count_running_tests)
        if [ $running_count -gt 0 ]; then
            if [ $SELECTED_INDEX -ge $running_count ]; then
                SELECTED_INDEX=$((running_count - 1))
            fi
        else
            SELECTED_INDEX=0
        fi

        # Draw TUI
        tui_draw

        # Handle input
        if ! tui_handle_input; then
            break  # User pressed q
        fi

        # Small delay
        sleep 0.5
    done

    # Final draw
    tui_draw
    sleep 1

    # Cleanup TUI
    tui_cleanup

    # Mark any still-running tests as interrupted
    for prefix in "${ALL_PREFIXES[@]}"; do
        if [ "${PREFIX_STATUS[$prefix]}" = "running" ]; then
            PREFIX_STATUS[$prefix]="interrupted"
            # Kill the process if still running
            local pid="${PREFIX_PIDS[$prefix]}"
            if [ -n "$pid" ] && kill -0 "$pid" 2>/dev/null; then
                kill "$pid" 2>/dev/null || true
            fi
        fi
    done

    # Calculate total elapsed time
    local end_time=$(date +%s)
    local total_elapsed=$((end_time - GLOBAL_START_TIME))
    local total_elapsed_str=$(format_elapsed_time $total_elapsed)

    # Display detailed summary first
    display_detailed_summary

    # Print summary
    echo ""
    echo "========================================"
    echo "  Parallel Test Run Complete"
    echo "========================================"
    echo ""

    local completed=0
    local failed=0
    local interrupted=0

    for prefix in "${ALL_PREFIXES[@]}"; do
        if [ "${PREFIX_STATUS[$prefix]}" = "completed" ]; then
            ((completed++)) || true
            echo -e "${GREEN}[PASSED]${NC} ${prefix}_ - $RUN_COUNT runs"
        elif [ "${PREFIX_STATUS[$prefix]}" = "failed" ]; then
            ((failed++)) || true
            local run_num=${PREFIX_CURRENT_RUN[$prefix]}
            echo -e "${RED}[FAILED]${NC} ${prefix}_ - Failed at run $run_num"
            # Find the fail log
            local fail_log="${PREFIX_LOG_FILE[$prefix]}"
            if [ -n "$fail_log" ] && [ -f "$fail_log" ]; then
                # Find all failure lines with line numbers
                local failure_lines
                failure_lines=$(find_failure_lines "$fail_log")

                if [ -n "$failure_lines" ]; then
                    # Display each failure with its line number
                    echo "$failure_lines" | while IFS='|' read -r line_num error_type error_content; do
                        [ -z "$line_num" ] && continue
                        # Trim whitespace from error_content
                        error_content=$(echo "$error_content" | sed 's/^[[:space:]]*//' | sed 's/[[:space:]]*$//')
                        echo "         Log: ${fail_log}:${line_num}"
                        echo "         Reason: [${error_type}] ${error_content}"
                    done
                else
                    # No specific errors found, just show the log file
                    echo "         Log: $fail_log"
                fi
            fi
        elif [ "${PREFIX_STATUS[$prefix]}" = "interrupted" ]; then
            ((interrupted++)) || true
            local run_num=${PREFIX_CURRENT_RUN[$prefix]}
            echo -e "${YELLOW}[INTERRUPTED]${NC} ${prefix}_ - Stopped at run $run_num"
            local log_file="${PREFIX_LOG_FILE[$prefix]}"
            if [ -n "$log_file" ] && [ -f "$log_file" ]; then
                echo "         Log: $log_file"
            fi
        fi
    done

    echo ""
    if [ $interrupted -gt 0 ]; then
        echo "Summary: $completed passed, $failed failed, $interrupted interrupted"
    else
        echo "Summary: $completed passed, $failed failed"
    fi
    echo "Total time: $total_elapsed_str"
    echo "Logs saved in: $LOG_DIR/"
    echo ""

    if [ $failed -gt 0 ]; then
        exit 1
    fi
}

# ============================================================================
# Simple (non-TUI) Functions
# ============================================================================

# Format elapsed time from seconds to HH:MM:SS
format_elapsed_time() {
    local seconds=$1
    local hours=$((seconds / 3600))
    local minutes=$(((seconds % 3600) / 60))
    local secs=$((seconds % 60))
    printf "%02d:%02d:%02d" $hours $minutes $secs
}

# Get current test info from log file
# Returns: test_name|completed_tests
get_test_progress_info() {
    local log_file=$1
    local test_name=""
    local completed_tests="0"

    if [ ! -f "$log_file" ]; then
        printf "waiting...|0"
        return
    fi

    # Get current running test from "Running tests with tag [TAG]" pattern
    local running_line
    running_line=$(grep -E "Running tests with tag \[" "$log_file" 2>/dev/null | tail -1 | tr -d '\n\r')
    if [ -n "$running_line" ]; then
        # Extract the tag name between brackets
        test_name=$(echo "$running_line" | grep -oP '\[\K[^\]]+' | head -1 | tr -d '\n\r')
    fi

    # Count completed tests by counting "All tests passed" or "test case(s) failed" result lines
    completed_tests=$(grep -cE "All tests passed|[0-9]+ test case(s)? failed" "$log_file" 2>/dev/null || echo "0")
    completed_tests=$(echo "$completed_tests" | tr -d '\n\r')
    completed_tests="${completed_tests:-0}"

    # If no test name found, check if still building or waiting
    if [ -z "$test_name" ]; then
        if grep -q "Building" "$log_file" 2>/dev/null; then
            test_name="building..."
        elif grep -q "Running tests" "$log_file" 2>/dev/null; then
            test_name="starting..."
        else
            test_name="waiting..."
        fi
    fi

    # Truncate test name if too long (keep tag format readable)
    if [ ${#test_name} -gt 35 ]; then
        test_name="${test_name:0:32}..."
    fi

    printf "%s|%s" "$test_name" "$completed_tests"
}

# Display current status (simple mode)
display_status() {
    local running_count=$(count_running_tests)

    # Calculate total elapsed time
    local current_time=$(date +%s)
    local total_elapsed=$((current_time - GLOBAL_START_TIME))
    local total_elapsed_str=$(format_elapsed_time $total_elapsed)

    echo ""
    echo -e "${BLUE}Running tests ($running_count task) (Total Elapsed: $total_elapsed_str):${NC}"
    echo ""

    for prefix in "${ALL_PREFIXES[@]}"; do
        if [ "${PREFIX_STATUS[$prefix]}" = "running" ]; then
            local run_num=${PREFIX_CURRENT_RUN[$prefix]}
            local log_file="${PREFIX_LOG_FILE[$prefix]}"
            local start_time="${PREFIX_START_TIME[$prefix]}"
            local total_tests="${PREFIX_TOTAL_TESTS[$prefix]}"

            # Calculate elapsed time
            local current_time=$(date +%s)
            local elapsed=$((current_time - start_time))
            local elapsed_str=$(format_elapsed_time $elapsed)

            # Get test progress info (returns: test_name|completed_tests)
            local progress_info
            progress_info=$(get_test_progress_info "$log_file")

            # Parse using IFS to avoid newline issues
            local test_name completed_tests
            IFS='|' read -r test_name completed_tests <<< "$progress_info"

            # Ensure values are clean
            test_name="${test_name:-waiting...}"
            completed_tests="${completed_tests:-0}"

            # Format progress: "X of Y" if total known, "X completed" otherwise
            local progress_str
            if [ -n "$total_tests" ] && [ "$total_tests" -gt 0 ]; then
                progress_str="$completed_tests of $total_tests"
            else
                progress_str="$completed_tests completed"
            fi

            printf "  ${YELLOW}%s_%s${NC} (Run %d of %d) (%s) (Elapsed: %s)\n" \
                "$prefix" "$test_name" "$run_num" "$RUN_COUNT" "$progress_str" "$elapsed_str"
        fi
    done

    # Add empty lines if less than PARALLEL_COUNT running
    local displayed=$running_count
    while [ $displayed -lt $PARALLEL_COUNT ]; do
        echo ""
        ((displayed++)) || true
    done

    echo ""
}

# Main execution loop (simple mode)
run_parallel_tests_simple() {
    echo ""
    echo "========================================"
    echo "  Parallel Test Runner"
    echo "========================================"
    echo "Parallel count: $PARALLEL_COUNT"
    echo "Run count per prefix: $RUN_COUNT"
    echo "Timestamp: $TIMESTAMP"
    echo "Log directory: $LOG_DIR/"
    echo "Pass-through args: ${PASS_THROUGH_ARGS[*]}"
    echo "========================================"

    # Create log directory
    mkdir -p "$LOG_DIR"

    # Do initial build
    do_initial_build

    echo ""
    echo -e "${CYAN}========================================"
    echo "  Starting Parallel Test Execution"
    echo "========================================${NC}"

    # Set global start time for total elapsed tracking
    GLOBAL_START_TIME=$(date +%s)

    # Initial status display
    display_status

    while ! all_tests_done; do
        # Check for completed tests
        for prefix in "${ALL_PREFIXES[@]}"; do
            if [ "${PREFIX_STATUS[$prefix]}" = "running" ]; then
                if check_test_completion "$prefix"; then
                    local exit_code=$(get_test_exit_code "$prefix")
                    local current_run=${PREFIX_CURRENT_RUN[$prefix]}
                    local log_file="${PREFIX_LOG_FILE[$prefix]}"

                    if [ $exit_code -eq 0 ]; then
                        # Check for Valgrind errors even if exit code is 0
                        if ! check_valgrind_errors "$log_file"; then
                            # Valgrind errors found - treat as failure
                            PREFIX_STATUS[$prefix]="failed"
                            PREFIX_FAIL_REASON[$prefix]="$VALGRIND_ERROR_MSG"
                            mark_log_as_failed "$prefix"
                            echo -e "${RED}[FAILED]${NC} ${prefix}_ - Run $current_run failed (Valgrind error)"
                            echo "  $VALGRIND_ERROR_MSG"
                            echo "  Log: ${PREFIX_LOG_FILE[$prefix]}"
                        else
                            # Run passed - store total test count for future runs
                            if [ -z "${PREFIX_TOTAL_TESTS[$prefix]}" ]; then
                                PREFIX_TOTAL_TESTS[$prefix]=$(count_total_tests_in_log "$log_file")
                            fi

                            if [ $current_run -lt $RUN_COUNT ]; then
                                # More runs needed, set back to pending
                                PREFIX_STATUS[$prefix]="pending"
                                echo -e "${GREEN}[PASSED]${NC} ${prefix}_ Run $current_run - continuing to next run"
                            else
                                # All runs completed successfully
                                PREFIX_STATUS[$prefix]="completed"
                                echo -e "${GREEN}[COMPLETED]${NC} ${prefix}_ - All $RUN_COUNT runs passed"
                            fi
                        fi
                    else
                        # Test failed - stop this prefix
                        PREFIX_STATUS[$prefix]="failed"
                        PREFIX_FAIL_REASON[$prefix]="Exit code: $exit_code"
                        mark_log_as_failed "$prefix"
                        echo -e "${RED}[FAILED]${NC} ${prefix}_ - Run $current_run failed (exit code: $exit_code)"
                        echo "  Log: ${PREFIX_LOG_FILE[$prefix]}"
                    fi

                    # Clear PID
                    unset "PREFIX_PIDS[$prefix]"
                fi
            fi
        done

        # Start new tests if we have capacity
        local running_count=$(count_running_tests)
        while [ $running_count -lt $PARALLEL_COUNT ]; do
            local next_prefix=$(get_next_pending)
            if [ -z "$next_prefix" ]; then
                break  # No more pending tests
            fi

            start_test "$next_prefix"
            ((running_count++)) || true
        done

        # Display current status
        display_status

        # Small delay to prevent busy waiting
        sleep 2
    done

    # Calculate total elapsed time
    local end_time=$(date +%s)
    local total_elapsed=$((end_time - GLOBAL_START_TIME))
    local total_elapsed_str=$(format_elapsed_time $total_elapsed)

    # Display detailed summary first
    display_detailed_summary

    echo ""
    echo "========================================"
    echo "  Parallel Test Run Complete"
    echo "========================================"
    echo ""

    # Summary
    local completed=0
    local failed=0

    for prefix in "${ALL_PREFIXES[@]}"; do
        if [ "${PREFIX_STATUS[$prefix]}" = "completed" ]; then
            ((completed++)) || true
            echo -e "${GREEN}[PASSED]${NC} ${prefix}_ - $RUN_COUNT runs"
        elif [ "${PREFIX_STATUS[$prefix]}" = "failed" ]; then
            ((failed++)) || true
            local run_num=${PREFIX_CURRENT_RUN[$prefix]}
            echo -e "${RED}[FAILED]${NC} ${prefix}_ - Failed at run $run_num"
            # Find the fail log
            local fail_log="${PREFIX_LOG_FILE[$prefix]}"
            if [ -n "$fail_log" ] && [ -f "$fail_log" ]; then
                # Find all failure lines with line numbers
                local failure_lines
                failure_lines=$(find_failure_lines "$fail_log")

                if [ -n "$failure_lines" ]; then
                    # Display each failure with its line number
                    echo "$failure_lines" | while IFS='|' read -r line_num error_type error_content; do
                        [ -z "$line_num" ] && continue
                        # Trim whitespace from error_content
                        error_content=$(echo "$error_content" | sed 's/^[[:space:]]*//' | sed 's/[[:space:]]*$//')
                        echo "         Log: ${fail_log}:${line_num}"
                        echo "         Reason: [${error_type}] ${error_content}"
                    done
                else
                    # No specific errors found, just show the log file
                    echo "         Log: $fail_log"
                fi
            fi
        fi
    done

    echo ""
    echo "Summary: $completed passed, $failed failed"
    echo "Total time: $total_elapsed_str"
    echo "Logs saved in: $LOG_DIR/"
    echo ""

    if [ $failed -gt 0 ]; then
        exit 1
    fi
}

# ============================================================================
# Cleanup and Main
# ============================================================================

# Cleanup function for Ctrl+C
cleanup() {
    # Restore TUI if active
    if [ "$SHOW_TUI" = true ]; then
        tui_cleanup
    fi

    echo ""
    echo -e "${YELLOW}Interrupted! Stopping all running tests...${NC}"

    for prefix in "${ALL_PREFIXES[@]}"; do
        if [ "${PREFIX_STATUS[$prefix]}" = "running" ]; then
            local pid=${PREFIX_PIDS[$prefix]}
            if [ -n "$pid" ]; then
                kill -TERM "$pid" 2>/dev/null || true
                # Also kill child processes
                pkill -P "$pid" 2>/dev/null || true
            fi
        fi
    done

    echo "All tests stopped."
    exit 130
}

# Set up trap for cleanup
trap cleanup INT TERM

# Main
parse_arguments "$@"

# Check if summarize mode is requested
if [ "$SUMMARIZE_MODE" = true ]; then
    run_summarize_mode
    exit 0
fi

# Normal test execution mode
discover_prefixes
apply_parallel_order
validate_parallel_count
initialize_prefixes

if [ "$SHOW_TUI" = true ]; then
    run_parallel_tests_tui
else
    run_parallel_tests_simple
fi
