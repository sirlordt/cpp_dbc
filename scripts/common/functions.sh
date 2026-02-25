#!/bin/bash
# scripts/common/functions.sh
# Common functions shared between shell scripts in cpp_dbc project
#
# Usage: source this file at the top of your script
#   SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )"
#   source "$SCRIPT_DIR/scripts/common/functions.sh"
#
# Or from subdirectories:
#   source "$(dirname "$0")/../scripts/common/functions.sh"

# Guard against multiple inclusions
if [ -n "$_COMMON_FUNCTIONS_LOADED" ]; then
    return 0
fi
_COMMON_FUNCTIONS_LOADED=1

# ============================================================================
# Color Support Detection and Setup
# ============================================================================

# Check if terminal supports colors
# Returns: 0 if colors supported, 1 otherwise
check_color_support() {
    if [ -t 1 ] && [ -n "$TERM" ] && [ "$TERM" != "dumb" ]; then
        return 0  # Terminal supports colors
    else
        return 1  # Terminal doesn't support colors
    fi
}

# Initialize color variables using tput for portability
# Colors are empty strings if terminal doesn't support colors
_init_colors() {
    if check_color_support && command -v tput &> /dev/null; then
        # Basic colors using tput
        RED=$(tput setaf 1)
        GREEN=$(tput setaf 2)
        YELLOW=$(tput setaf 3)
        BLUE=$(tput setaf 4)
        MAGENTA=$(tput setaf 5)
        CYAN=$(tput setaf 6)
        WHITE=$(tput setaf 7)

        # Text attributes
        BOLD=$(tput bold)
        DIM=$(tput dim)
        REVERSE=$(tput rev)
        UNDERLINE=$(tput smul)
        NC=$(tput sgr0)  # No Color / Reset

        # Cursor movement
        CURSOR_SAVE=$(tput sc)
        CURSOR_RESTORE=$(tput rc)
        CLEAR_LINE=$(tput el)
        CLEAR_SCREEN=$(tput clear)

        # Terminal dimensions
        TERM_COLS=$(tput cols)
        TERM_ROWS=$(tput lines)
    else
        # No color support - use empty strings
        RED=""
        GREEN=""
        YELLOW=""
        BLUE=""
        MAGENTA=""
        CYAN=""
        WHITE=""
        BOLD=""
        DIM=""
        REVERSE=""
        UNDERLINE=""
        NC=""
        CURSOR_SAVE=""
        CURSOR_RESTORE=""
        CLEAR_LINE=""
        CLEAR_SCREEN=""
        TERM_COLS=80
        TERM_ROWS=24
    fi
}

# Initialize colors on load
_init_colors

# Refresh terminal dimensions (call after window resize)
refresh_term_size() {
    if check_color_support && command -v tput &> /dev/null; then
        TERM_COLS=$(tput cols)
        TERM_ROWS=$(tput lines)
    fi
}

# Move cursor to specific position
# Arguments: $1=row, $2=column (1-based)
cursor_to() {
    if check_color_support && command -v tput &> /dev/null; then
        tput cup "$1" "$2"
    fi
}

# Hide cursor
cursor_hide() {
    if check_color_support && command -v tput &> /dev/null; then
        tput civis
    fi
}

# Show cursor
cursor_show() {
    if check_color_support && command -v tput &> /dev/null; then
        tput cnorm
    fi
}

# ============================================================================
# Time/Duration Functions
# ============================================================================

# Format seconds as HH:MM:SS
# Arguments:
#   $1 - seconds (integer)
# Output: Formatted string like "01:23:45"
# Usage:
#   duration_str=$(format_duration 3665)  # Returns "01:01:05"
format_duration() {
    local seconds=$1
    local hours
    local minutes
    local secs
    hours=$((seconds / 3600))
    minutes=$(((seconds % 3600) / 60))
    secs=$((seconds % 60))
    printf "%02d:%02d:%02d" "$hours" "$minutes" "$secs"
}

# Format seconds as human-readable elapsed time
# Arguments:
#   $1 - seconds (integer)
# Output: Formatted string like "1h 23m 45s" or "5m 30s" or "45s"
# Usage:
#   elapsed_str=$(format_elapsed_time 3665)  # Returns "1h 1m 5s"
format_elapsed_time() {
    local total_seconds=$1
    local hours
    local minutes
    local seconds
    hours=$((total_seconds / 3600))
    minutes=$(((total_seconds % 3600) / 60))
    seconds=$((total_seconds % 60))

    if [ "$hours" -gt 0 ]; then
        printf "%dh %dm %ds" "$hours" "$minutes" "$seconds"
    elif [ "$minutes" -gt 0 ]; then
        printf "%dm %ds" "$minutes" "$seconds"
    else
        printf "%ds" "$seconds"
    fi
}

# ============================================================================
# Validation Functions
# ============================================================================

# Validate that a value is a non-negative integer
# Arguments:
#   $1 - value to validate
#   $2 - parameter name (for error message)
# Returns: 0 if valid, exits with error if invalid
# Usage:
#   validate_numeric "$count" "--count"
validate_numeric() {
    local value="$1"
    local param_name="$2"

    if ! [[ "$value" =~ ^[0-9]+$ ]]; then
        echo "${RED}Error:${NC} $param_name must be a non-negative integer, got: '$value'" >&2
        exit 1
    fi
}

# ============================================================================
# Log Management Functions
# ============================================================================

# Cleanup old log files, keeping only the most recent ones
# Arguments:
#   $1 - log directory path
#   $2 - maximum number of logs to keep (default: 4)
# Usage:
#   cleanup_old_logs "/path/to/logs" 10
cleanup_old_logs() {
    local log_path="$1"
    local max_logs="${2:-4}"

    if [ ! -d "$log_path" ]; then
        return 0
    fi

    # Get list of log files sorted by modification time (newest first)
    local logs
    mapfile -t logs < <(find "$log_path" -maxdepth 1 -type f -name "*.log" -printf '%T@ %p\n' | sort -rn | cut -d' ' -f2-)

    local log_count=${#logs[@]}
    if [ "$log_count" -gt "$max_logs" ]; then
        echo "Cleaning up old logs in ${log_path}, keeping ${max_logs} most recent logs..."
        printf '%s\n' "${logs[@]:$max_logs}" | xargs rm -f --
    fi
}

# ============================================================================
# Valgrind Error Checking Functions
# ============================================================================

# Check a log file for Valgrind errors
# Arguments:
#   $1 - log file path
# Returns: 0 if no errors, 1 if errors found
# Sets: VALGRIND_ERROR_MSG with error description if errors found
# Usage:
#   if ! check_valgrind_errors "$log_file"; then
#       echo "Valgrind error: $VALGRIND_ERROR_MSG"
#   fi
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

# Verbose version of check_valgrind_errors with output
# Arguments:
#   $1 - log file path
# Returns: 0 if no errors, 1 if errors found
# Usage:
#   check_valgrind_errors_verbose "$log_file"
check_valgrind_errors_verbose() {
    local log_file="$1"
    local found_issues=0

    echo "Checking for valgrind errors..."

    # Extract error counts from ERROR SUMMARY lines
    local error_lines
    error_lines=$(grep -a -n "ERROR SUMMARY:" "$log_file" 2>/dev/null)

    if [ -n "$error_lines" ]; then
        local found_error=0
        while IFS= read -r line; do
            local line_num
            local error_text
            line_num=$(echo "$line" | cut -d':' -f1)
            error_text=$(echo "$line" | cut -d':' -f2-)

            # Extract error and context counts
            if [[ "$error_text" =~ ([0-9]+)[[:space:]]+errors[[:space:]]+from[[:space:]]+([0-9]+)[[:space:]]+contexts ]]; then
                local errors="${BASH_REMATCH[1]}"
                local contexts="${BASH_REMATCH[2]}"

                # Only report if errors or contexts are greater than 0
                if [ "$errors" -gt 0 ] || [ "$contexts" -gt 0 ]; then
                    echo "${RED}[ERROR SUMMARY: $errors errors from $contexts contexts]${NC} => $log_file:$line_num"
                    found_issues=1
                    found_error=1
                fi
            fi
        done <<< "$error_lines"

        if [ $found_error -eq 0 ]; then
            echo "${GREEN}ERROR SUMMARY found but no errors reported.${NC}"
        fi
    else
        echo "${YELLOW}No valgrind error summaries found.${NC}"
    fi

    return $found_issues
}

# ============================================================================
# Wildcard Pattern Matching Functions
# ============================================================================

# Check if a string contains wildcard characters (* or ?)
# Arguments:
#   $1 - string to check
# Returns: 0 if contains wildcards, 1 otherwise
# Usage:
#   if contains_wildcard "23_*"; then
#       echo "Pattern contains wildcards"
#   fi
contains_wildcard() {
    local pattern="$1"
    [[ "$pattern" == *"*"* || "$pattern" == *"?"* ]]
}

# Check if a string matches a wildcard pattern
# Arguments:
#   $1 - string to check
#   $2 - pattern (can contain * and ?)
# Returns: 0 if matches, 1 otherwise
# Usage:
#   if matches_wildcard_pattern "23_001_example_firebird_basic" "23_*"; then
#       echo "Matches!"
#   fi
matches_wildcard_pattern() {
    local string="$1"
    local pattern="$2"
    # shellcheck disable=SC2053
    [[ "$string" == $pattern ]]
}

# ============================================================================
# Print Utility Functions
# ============================================================================

# Print a colored message
# Arguments:
#   $1 - color variable (e.g., RED, GREEN)
#   $2 - message
print_color() {
    local color="$1"
    local message="$2"
    echo -e "${color}${message}${NC}"
}

# Print success message
print_success() {
    echo -e "${GREEN}$1${NC}"
}

# Print error message
print_error() {
    echo -e "${RED}Error: $1${NC}" >&2
}

# Print warning message
print_warning() {
    echo -e "${YELLOW}Warning: $1${NC}"
}

# Print info message
print_info() {
    echo -e "${CYAN}$1${NC}"
}

# Print a horizontal line
# Arguments:
#   $1 - character to use (default: -)
#   $2 - width (default: terminal width)
print_line() {
    local char="${1:--}"
    local width="${2:-$TERM_COLS}"
    printf '%*s\n' "$width" '' | tr ' ' "$char"
}

# Print a header with a title
# Arguments:
#   $1 - title
print_header() {
    local title="$1"
    print_line "="
    echo -e "${BOLD}${title}${NC}"
    print_line "="
}

# ============================================================================
# Docker DB Container Management Functions
# ============================================================================

# Get Docker detection info for a DB driver
# Arguments:
#   $1 - driver name (mysql, postgres, mongodb, redis, scylladb, firebird)
# Output: "IMAGE_KEYWORD:STANDARD_PORT" or empty string if not supported
# Usage:
#   info=$(get_driver_docker_info "mysql")  # Returns "mysql:3306"
get_driver_docker_info() {
    local driver="$1"
    case "$driver" in
        mysql)      echo "mysql:3306"    ;;
        postgres)   echo "postgres:5432" ;;
        mongodb)    echo "mongo:27017"   ;;
        redis)      echo "redis:6379"    ;;
        scylladb)   echo "scylla:9042"   ;;
        firebird)   echo "firebird:3050" ;;
        *)          echo "" ;;
    esac
}

# Find a Docker container for a given DB driver.
# Matching strategy:
#   1. Primary: match by port mapping "127.0.0.1:PORT->" (most reliable,
#      handles custom image names like ubuntu-22-04-redis)
#   2. Fallback: match by image name keyword (for non-standard port setups)
# Arguments:
#   $1 - driver name (mysql, postgres, mongodb, redis, scylladb, firebird)
# Output: container name (stdout) if found
# Returns: 0 if found, 1 if not found
# Usage:
#   container=$(find_db_container "mysql") && echo "Found: $container"
find_db_container() {
    local driver="$1"
    local driver_info
    driver_info=$(get_driver_docker_info "$driver")
    if [ -z "$driver_info" ]; then
        return 1
    fi

    local image_keyword="${driver_info%%:*}"
    local port="${driver_info##*:}"

    # Primary: match by port mapping (works even with custom image names)
    local container
    container=$(docker ps -a --format "{{.Names}}\t{{.Ports}}" 2>/dev/null \
        | awk -F'\t' -v p="$port" '$2 ~ "127\\.0\\.0\\.1:"p"->" {print $1}' \
        | head -1)

    if [ -n "$container" ]; then
        echo "$container"
        return 0
    fi

    # Fallback: match by image name keyword
    container=$(docker ps -a --format "{{.Names}}\t{{.Image}}" 2>/dev/null \
        | awk -F'\t' -v img="$image_keyword" 'tolower($2) ~ img {print $1}' \
        | head -1)

    if [ -n "$container" ]; then
        echo "$container"
        return 0
    fi

    return 1
}

# Wait for a Docker container to be running and healthy.
# Polls docker inspect every second without hardcoded sleep times.
# For containers with a healthcheck: waits for health == "healthy".
# For containers without a healthcheck: "running" state is sufficient.
# Prints one dot per second while waiting (inline, no newline).
# Sets global WAIT_ELAPSED_SECONDS to the actual wait time on return.
# Arguments:
#   $1 - container name
#   $2 - max wait in seconds (default: 120)
# Returns: 0 if ready, 1 if timed out
# Usage:
#   printf "Waiting... "
#   if wait_for_container "my_container" 120; then
#       echo " ready (${WAIT_ELAPSED_SECONDS}s)"
#   fi
WAIT_ELAPSED_SECONDS=0
wait_for_container() {
    local container="$1"
    local max_wait="${2:-120}"
    WAIT_ELAPSED_SECONDS=0

    while [ "$WAIT_ELAPSED_SECONDS" -lt "$max_wait" ]; do
        local status health
        status=$(docker inspect --format='{{.State.Status}}' "$container" 2>/dev/null)
        health=$(docker inspect \
            --format='{{if .State.Health}}{{.State.Health.Status}}{{else}}none{{end}}' \
            "$container" 2>/dev/null)

        if [ "$status" = "running" ]; then
            if [ "$health" = "none" ] || [ "$health" = "healthy" ]; then
                return 0
            fi
        fi

        printf "."
        sleep 1
        ((WAIT_ELAPSED_SECONDS++))
    done

    return 1
}

# Restart Docker containers for the enabled DB drivers before running tests.
# - Checks Docker availability and user access privileges.
# - For each driver, finds the matching container by port then by image name.
# - If no container found for a driver: prints a warning and continues.
# - Restarts all found containers, then waits for each to be ready.
# - Intended to run BEFORE the TUI or test execution starts.
# Arguments:
#   $@ - enabled driver names (e.g., mysql postgres mongodb redis scylladb firebird)
#        SQLite is automatically skipped (no container needed).
# Usage:
#   restart_db_containers_for_test mysql postgres mongodb
restart_db_containers_for_test() {
    local -a drivers=("$@")
    if [ ${#drivers[@]} -eq 0 ]; then
        return 0
    fi

    # Check Docker availability
    if ! command -v docker &>/dev/null; then
        print_warning "Docker not found — skipping DB container restart."
        return 0
    fi

    # Check Docker access privileges (non-fatal)
    if ! docker ps &>/dev/null 2>&1; then
        print_warning "Cannot access Docker (no permissions?) — skipping DB container restart."
        return 0
    fi

    print_header "DB Container Restart"

    local -a containers_to_restart=()
    local -a driver_labels=()

    for driver in "${drivers[@]}"; do
        [ "$driver" = "sqlite" ] && continue  # No container needed for SQLite

        local container
        container=$(find_db_container "$driver")
        if [ -n "$container" ]; then
            print_info "  [${driver}] Found container: ${container}"
            containers_to_restart+=("$container")
            driver_labels+=("$driver")
        else
            print_warning "  [${driver}] No local Docker container found — skipping (tests may fail to connect)"
        fi
    done

    if [ ${#containers_to_restart[@]} -eq 0 ]; then
        echo ""
        print_warning "No DB containers found to restart. Continuing with tests."
        echo ""
        return 0
    fi

    echo ""
    print_info "Restarting ${#containers_to_restart[@]} container(s)..."
    echo ""

    for i in "${!containers_to_restart[@]}"; do
        local container="${containers_to_restart[$i]}"
        local label="${driver_labels[$i]}"
        printf "  %-12s %-52s " "[${label}]" "${container}"
        if docker restart "$container" >/dev/null 2>&1; then
            echo -e "${GREEN}restarted${NC}"
        else
            echo -e "${RED}FAILED to restart${NC}"
        fi
    done

    echo ""
    print_info "Waiting for containers to be ready (polling state, no fixed delays)..."
    echo ""

    local all_ready=true
    for i in "${!containers_to_restart[@]}"; do
        local container="${containers_to_restart[$i]}"
        local label="${driver_labels[$i]}"
        printf "  %-12s %-52s " "[${label}]" "${container}"
        if wait_for_container "$container" 120; then
            echo -e " ${GREEN}ready${NC} (${WAIT_ELAPSED_SECONDS}s)"
        else
            echo -e " ${YELLOW}TIMEOUT${NC} (${WAIT_ELAPSED_SECONDS}s — proceeding anyway)"
            all_ready=false
        fi
    done

    echo ""
    if [ "$all_ready" = true ]; then
        print_success "All DB containers are ready."
    else
        print_warning "Some containers may not be fully ready. Proceeding with tests anyway."
    fi
    print_line "-"
    echo ""
}

# ============================================================================
# Test Completion Notification (Gotify / curl-based)
# ============================================================================

# Send a Gotify notification after tests complete.
#
# Reads NOTIFY_COMMAND from <project_root>/.env.secrets.
# If the file is absent or NOTIFY_COMMAND is empty, returns silently.
#
# NOTIFY_COMMAND format (placeholders replaced before execution):
#   curl "URL?token=TOKEN" -F "title=@__TITLE__@" -F "message=@__MESSAGE__@" -F "priority=N"
#
# Message format built from final_report.log (parallel runs):
#   [PASSED]      10_ - 5 runs
#   [PASSED]      20_ - 3 runs
#   [FAILED]      20_ - run 4
#   [INTERRUPTED] 21_ - run 3
#
# For non-parallel runs (no report file): single-line pass/fail summary.
#
# Arguments:
#   $1 - project root directory (where .env.secrets lives)
#   $2 - path to final_report.log (empty string for non-parallel runs)
#   $3 - overall exit code (0 = success, non-zero = failure)
# Usage:
#   send_test_notification "$project_root" "$report_file" "$exit_code"
send_test_notification() {
    local project_root="$1"
    local report_file="${2:-}"
    local exit_code="${3:-0}"

    # Load NOTIFY_COMMAND from .env.secrets
    local secrets_file="$project_root/.env.secrets"
    if [ ! -f "$secrets_file" ]; then
        return 0
    fi

    # Extract NOTIFY_COMMAND value from file (supports unquoted values with spaces)
    local NOTIFY_COMMAND
    NOTIFY_COMMAND=$(grep '^NOTIFY_COMMAND=' "$secrets_file" | head -1 | sed 's/^NOTIFY_COMMAND=//')

    if [ -z "$NOTIFY_COMMAND" ]; then
        return 0
    fi

    # Check curl is available
    if ! command -v curl &>/dev/null; then
        print_warning "send_test_notification: curl not found, cannot send notification."
        return 0
    fi

    # ── Build compact message from final_report.log (parallel mode) ──────────
    local message=""
    local has_failures=false
    local has_interrupted=false

    if [ -n "$report_file" ] && [ -f "$report_file" ]; then
        while IFS= read -r line; do
            # Parse compact summary lines (written at top of final_report.log)
            # PASSED: [PASSED] 10_ - 5 run(s) or [PASSED] 10_ - 5 run(s) (with warnings)
            if [[ "$line" =~ ^\[PASSED\]\ ([0-9]+_)\ -\ ([0-9]+)\ run ]]; then
                local prefix="${BASH_REMATCH[1]}"
                local runs="${BASH_REMATCH[2]}"
                message+="[PASSED] ${prefix} - ${runs} run(s)"$'\n'

            # FAILED: [FAILED] 20_ - Failed at run 4
            elif [[ "$line" =~ ^\[FAILED\]\ ([0-9]+_)\ -\ Failed\ at\ run\ ([0-9]+) ]]; then
                local prefix="${BASH_REMATCH[1]}"
                local run_num="${BASH_REMATCH[2]}"
                message+="[FAILED] ${prefix} - run ${run_num}"$'\n'
                has_failures=true

            # INTERRUPTED: [INTERRUPTED] 21_ - Stopped at run 3
            elif [[ "$line" =~ ^\[INTERRUPTED\]\ ([0-9]+_)\ -\ Stopped\ at\ run\ ([0-9]+) ]]; then
                local prefix="${BASH_REMATCH[1]}"
                local run_num="${BASH_REMATCH[2]}"
                message+="[INTERRUPTED] ${prefix} - run ${run_num}"$'\n'
                has_interrupted=true

            # Summary line: Summary: 7 passed, 1 failed (...)
            elif [[ "$line" =~ ^Summary:\ [0-9] ]]; then
                message+="$line"$'\n'

            # Total time: 00:44:45
            elif [[ "$line" =~ ^Total\ time:\ ([0-9]{2}:[0-9]{2}:[0-9]{2}) ]]; then
                message+="Total time: ${BASH_REMATCH[1]}"$'\n'
            fi
        done < "$report_file"
    fi

    # Fallback: no report file (non-parallel run) or empty parse result
    if [ -z "$message" ]; then
        if [ "$exit_code" -eq 0 ]; then
            message="Tests completed successfully."
        else
            message="Tests completed with failures (exit code: ${exit_code})."
            has_failures=true
        fi
    fi

    # ── Build title ───────────────────────────────────────────────────────────
    local title
    if [ "$has_failures" = true ]; then
        title="Tests finished: FAILED"
    elif [ "$has_interrupted" = true ]; then
        title="Tests finished: interrupted"
    else
        title="Tests finished: all passed"
    fi

    # ── Substitute placeholders ───────────────────────────────────────────────
    # Replace @__TITLE__@ inline (title has no newlines, safe for direct sub).
    # Replace @__MESSAGE__@ with a shell variable reference so multiline message
    # is preserved correctly through eval.
    # NOTE: intermediate variable prevents bash from misinterpreting the inner '}'
    # as closing the outer '${...//}' parameter expansion.
    local _msg_ref='${_NOTIFY_MSG}'
    local cmd="${NOTIFY_COMMAND//@__TITLE__@/${title}}"
    cmd="${cmd//@__MESSAGE__@/$_msg_ref}"

    # ── Execute synchronously and show result ─────────────────────────────────
    print_line "-"
    print_info "Sending notification: ${title}"
    local _notify_exit=0
    (
        export _NOTIFY_MSG="$message"
        eval "$cmd" >/dev/null 2>&1
    ) || _notify_exit=$?

    if [ "$_notify_exit" -eq 0 ]; then
        print_success "Notification sent successfully."
    else
        print_warning "Notification failed (curl exit code: ${_notify_exit}). Check NOTIFY_COMMAND in .env.secrets."
    fi
    print_line "-"
    echo ""
}
