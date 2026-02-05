#!/bin/bash
# lib/common_functions.sh
# Common functions shared between shell scripts in cpp_dbc project
#
# Usage: source this file at the top of your script
#   SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )"
#   source "$SCRIPT_DIR/lib/common_functions.sh"
#
# Or from subdirectories:
#   source "$(dirname "$0")/../lib/common_functions.sh"

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
