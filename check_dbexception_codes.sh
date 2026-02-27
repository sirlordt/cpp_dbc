#!/bin/bash
# Check, list, and fix DBException error codes in the project
# Usage:
#   ./check_dbexception_codes.sh --check   # Check for all problems (exit 1 if found)
#   ./check_dbexception_codes.sh --list    # List all codes with locations and validity
#   ./check_dbexception_codes.sh --fix     # Auto-fix all problems (invalid length + duplicates)

PROJECT_ROOT="${PROJECT_ROOT:-$(cd "$(dirname "$0")" && pwd -P)}"
GENERATE_SCRIPT="$PROJECT_ROOT/generate_dbexception_code.sh"

# All source directories to scan (libs + project root src)
SCAN_DIRS=()
[ -d "$PROJECT_ROOT/libs" ] && SCAN_DIRS+=("$PROJECT_ROOT/libs")
[ -d "$PROJECT_ROOT/src"  ] && SCAN_DIRS+=("$PROJECT_ROOT/src")

VALID_MARK_REGEX='^[A-Z0-9]{12}$'
VALID_LETTER_MIN=5

# ------------------------------------------------------------------------------
# collect_all_marks
# Outputs lines: FILE:LINE:MARK
# Excludes commented-out lines.
# Captures ANY string in the first argument position of DBException(...),
# including empty string, lowercase, wrong length — everything.
# ------------------------------------------------------------------------------
collect_all_marks() {
    grep -rnP --include="*.cpp" --include="*.hpp" \
        'DBException\s*\(\s*"[^"]*"\s*,' "${SCAN_DIRS[@]}" 2>/dev/null \
    | grep -vP ':\s*//' \
    | grep -vP '^\s*//' \
    | grep -oP '^[^:]+:\d+:.*DBException\s*\(\s*"\K[^"]*' \
    | while IFS= read -r line; do
        # line is "FILE:LINE:MARK" but grep -oP consumed up to the mark
        # We need to reconstruct — use a two-pass approach below
        echo "$line"
    done
}

# Better two-pass: extract FILE:LINE and MARK separately then combine
collect_marks() {
    grep -rnP --include="*.cpp" --include="*.hpp" \
        'DBException\s*\(\s*"[^"]*"\s*,' "${SCAN_DIRS[@]}" 2>/dev/null \
    | grep -vP ':\s*//' \
    | grep -vPv '^\S' \
    | while IFS=: read -r file line rest; do
        mark=$(echo "$rest" | grep -oP 'DBException\s*\(\s*"\K[^"]*')
        if [ -n "$mark" ] || echo "$rest" | grep -qP 'DBException\s*\(\s*""'; then
            [ -z "$mark" ] && mark="<EMPTY>"
            echo "$file:$line:$mark"
        fi
    done
}

# Reliable extraction via awk-style sed
collect_marks() {
    grep -rnP --include="*.cpp" --include="*.hpp" \
        'DBException\s*\(\s*"[^"]*"\s*,' "${SCAN_DIRS[@]}" 2>/dev/null \
    | grep -vP '(^\s*//)|(:\s*//.*DBException)' \
    | while IFS= read -r fullline; do
        # fullline = /path/file.cpp:42:    throw DBException("MARK", ...
        file=$(echo "$fullline" | cut -d: -f1)
        lineno=$(echo "$fullline" | cut -d: -f2)
        mark=$(echo "$fullline" | grep -oP 'DBException\s*\(\s*"\K[^"]*')
        # Handle empty mark case: DBException("", ...)
        if echo "$fullline" | grep -qP 'DBException\s*\(\s*""'; then
            mark="<EMPTY>"
        fi
        echo "${file}:${lineno}:${mark}"
    done
}

# ------------------------------------------------------------------------------
# is_valid_mark  — returns 0 if valid, 1 otherwise
# Valid: exactly 12 chars, only A-Z and 0-9, at least 5 letters,
#        no more than 4 consecutive repeated chars
# ------------------------------------------------------------------------------
is_valid_mark() {
    local mark="$1"
    [ "$mark" = "<EMPTY>" ] && return 1
    [ ${#mark} -ne 12 ] && return 1
    echo "$mark" | grep -qP '^[A-Z0-9]{12}$' || return 1
    local letters
    letters=$(echo "$mark" | tr -cd 'A-Z' | wc -c)
    [ "$letters" -lt "$VALID_LETTER_MIN" ] && return 1
    echo "$mark" | grep -qP '(.)\1{4,}' && return 1
    return 0
}

# ------------------------------------------------------------------------------
# find_invalid_marks
# Outputs lines: FILE:LINE:MARK  for every mark that fails is_valid_mark
# ------------------------------------------------------------------------------
find_invalid_marks() {
    collect_marks | while IFS=: read -r file lineno mark; do
        if ! is_valid_mark "$mark"; then
            echo "${file}:${lineno}:${mark}"
        fi
    done
}

# ------------------------------------------------------------------------------
# find_duplicate_marks
# Outputs lines: MARK  for every valid mark that appears in more than one file
# ------------------------------------------------------------------------------
find_duplicate_marks() {
    collect_marks | while IFS=: read -r file lineno mark; do
        is_valid_mark "$mark" && echo "$file $mark"
    done \
    | sort -k2 \
    | awk '{marks[$2]=marks[$2] " " $1} END {
        for (m in marks) {
            n = split(marks[m], files, " ")
            seen[""] = 0
            unique = 0
            for (i=1; i<=n; i++) {
                if (files[i] != "" && !(files[i] in seen)) {
                    seen[files[i]] = 1
                    unique++
                }
            }
            if (unique > 1) print m
        }
    }'
}

show_usage() {
    echo "Usage: $0 [--check|--list|--fix|--help]"
    echo ""
    echo "Check, list, and fix DBException error codes in the project."
    echo ""
    echo "Validates:"
    echo "  - Exactly 12 characters (A-Z, 0-9)"
    echo "  - At least 5 letters"
    echo "  - No more than 4 consecutive repeated characters"
    echo "  - No empty marks"
    echo "  - No duplicate codes across different files"
    echo ""
    echo "Options:"
    echo "  --check   Report all invalid marks and cross-file duplicates (exit 1 if found)"
    echo "  --list    List all codes with file, line, length, and validity status"
    echo "  --fix     Auto-fix invalid marks and cross-file duplicates"
    echo "  --help    Show this help message"
}

# ------------------------------------------------------------------------------
# --check
# ------------------------------------------------------------------------------
check_mode() {
    local found_problems=0

    echo "=== Checking DBException marks ==="
    echo ""

    # 1. Invalid marks
    echo "--- Invalid marks (wrong length, format, or empty) ---"
    local invalids
    invalids=$(find_invalid_marks)
    if [ -n "$invalids" ]; then
        echo "$invalids" | while IFS=: read -r file lineno mark; do
            local len=${#mark}
            [ "$mark" = "<EMPTY>" ] && len=0
            printf "  %-6s  len=%-3s  %s:%s\n" "$mark" "$len" "$file" "$lineno"
        done
        found_problems=1
    else
        echo "  OK: No invalid marks found."
    fi

    echo ""

    # 2. Duplicate marks across files
    echo "--- Duplicate marks across different files ---"
    local duplicates
    duplicates=$(find_duplicate_marks)
    if [ -n "$duplicates" ]; then
        echo "$duplicates" | while read -r code; do
            echo "  === $code ==="
            collect_marks | grep ":${code}$" | while IFS=: read -r file lineno mark; do
                echo "    $file:$lineno"
            done
        done
        found_problems=1
    else
        echo "  OK: No cross-file duplicates found."
    fi

    echo ""

    if [ "$found_problems" -eq 1 ]; then
        echo "RESULT: Problems found. Run --fix to auto-correct."
        exit 1
    else
        echo "RESULT: All marks are valid and unique."
        exit 0
    fi
}

# ------------------------------------------------------------------------------
# --list
# ------------------------------------------------------------------------------
list_mode() {
    echo "=== All DBException marks ==="
    echo ""
    printf "%-14s  %-4s  %-7s  %s\n" "MARK" "LEN" "STATUS" "LOCATION"
    printf "%-14s  %-4s  %-7s  %s\n" "--------------" "----" "-------" "--------"

    collect_marks | sort -t: -k3 | while IFS=: read -r file lineno mark; do
        local len=${#mark}
        [ "$mark" = "<EMPTY>" ] && len=0
        local status
        if is_valid_mark "$mark"; then
            status="OK"
        else
            status="INVALID"
        fi
        # Shorten path to relative from PROJECT_ROOT
        local shortfile="${file#$PROJECT_ROOT/}"
        printf "%-14s  %-4s  %-7s  %s:%s\n" "$mark" "$len" "$status" "$shortfile" "$lineno"
    done
}

# ------------------------------------------------------------------------------
# --fix
# ------------------------------------------------------------------------------
fix_mode() {
    if [ ! -x "$GENERATE_SCRIPT" ]; then
        echo "ERROR: generate_dbexception_code.sh not found or not executable at $GENERATE_SCRIPT"
        exit 1
    fi

    local fixed=0

    echo "=== Fixing DBException marks ==="
    echo ""

    # 1. Fix invalid marks first
    echo "--- Fixing invalid marks ---"
    local invalids
    invalids=$(find_invalid_marks)
    if [ -n "$invalids" ]; then
        echo "$invalids" | while IFS=: read -r file lineno mark; do
            local new_code
            new_code=$("$GENERATE_SCRIPT" "$PROJECT_ROOT" 1 2>/dev/null)
            if [ -z "$new_code" ]; then
                echo "  ERROR: Failed to generate code for $mark at $file:$lineno"
                continue
            fi
            if [ "$mark" = "<EMPTY>" ]; then
                # Replace DBException("", with DBException("NEW_CODE",
                sed -i "s|DBException(\"\",|DBException(\"${new_code}\",|g" "$file"
            else
                sed -i "s|DBException(\"${mark}\"|DBException(\"${new_code}\"|g" "$file"
            fi
            echo "  $mark  →  $new_code  ($file:$lineno)"
            fixed=1
        done
    else
        echo "  OK: No invalid marks."
    fi

    echo ""

    # 2. Fix cross-file duplicates (keep first occurrence, replace rest)
    echo "--- Fixing cross-file duplicates ---"
    local duplicates
    duplicates=$(find_duplicate_marks)
    if [ -n "$duplicates" ]; then
        echo "$duplicates" | while read -r old_code; do
            local locations
            locations=$(collect_marks | grep ":${old_code}$")
            local first_file
            first_file=$(echo "$locations" | head -1 | cut -d: -f1)
            echo "  Keeping $old_code in: ${first_file#$PROJECT_ROOT/}"

            echo "$locations" | tail -n +2 | while IFS=: read -r file lineno mark; do
                local new_code
                new_code=$("$GENERATE_SCRIPT" "$PROJECT_ROOT" 1 2>/dev/null)
                if [ -z "$new_code" ]; then
                    echo "    ERROR: Failed to generate code for $old_code at $file:$lineno"
                    continue
                fi
                sed -i "s|DBException(\"${old_code}\"|DBException(\"${new_code}\"|g" "$file"
                echo "    $old_code  →  $new_code  (${file#$PROJECT_ROOT/}:$lineno)"
                fixed=1
            done
        done
    else
        echo "  OK: No cross-file duplicates."
    fi

    echo ""

    # Final verification
    echo "--- Verification ---"
    local remaining_invalid
    remaining_invalid=$(find_invalid_marks)
    local remaining_dupes
    remaining_dupes=$(find_duplicate_marks)

    if [ -n "$remaining_invalid" ] || [ -n "$remaining_dupes" ]; then
        echo "WARNING: Some problems may still exist — re-run --check for details."
        exit 1
    else
        echo "OK: All marks are valid and unique."
        exit 0
    fi
}

# ------------------------------------------------------------------------------
# Main
# ------------------------------------------------------------------------------
case "$1" in
    --help|-h)
        show_usage
        exit 0
        ;;
    --check)
        check_mode
        ;;
    --list)
        list_mode
        ;;
    --fix)
        fix_mode
        ;;
    *)
        echo "Unknown option: $1"
        echo ""
        show_usage
        exit 1
        ;;
esac
