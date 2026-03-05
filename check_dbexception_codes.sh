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
# collect_marks
# Outputs lines: FILE:LINENO:MARK
#
# Handles ALL patterns where DBException("CODE", ...) appears:
#
#   Pattern 1 — code on the same line as DBException(:
#     throw DBException("CODE", "msg", callstack);
#     return unexpected(DBException("CODE", "msg", callstack));
#     m_initError = DBException("CODE", "msg", callstack);
#
#   Pattern 2 — code on the line immediately after DBException(:
#     return unexpected<DBException>(DBException(
#         "CODE",
#         "msg",
#         callstack));
#
# Skips full-line comments (// ...).
# Skips DBException( after an inline // comment on the same line.
#
# Line numbers are per-file (reset to 1 at each new file).
# Uses: perl -e with explicit while + continue { close ARGV if eof }
#       to correctly reset $. between files.
# ------------------------------------------------------------------------------
collect_marks() {
    local files
    mapfile -t files < <(
        find "${SCAN_DIRS[@]}" \( -name "*.cpp" -o -name "*.hpp" \) 2>/dev/null | sort
    )
    [ ${#files[@]} -eq 0 ] && return

    perl -e '
        while (<>) {
            # Skip full-line comments
            next if /^\s*\/\//;

            # Skip lines without DBException(
            next unless /DBException\s*\(/;

            # Verify that DBException( is not after an inline // on this line
            my $before_dbe = substr($_, 0, index($_, "DBException"));
            next if $before_dbe =~ m{//};

            # -------------------------------------------------------------------
            # Pattern 1: "CODE" on the same line as DBException(
            # -------------------------------------------------------------------
            if (/DBException\s*\(\s*"([^"]*)"/) {
                printf "%s:%d:%s\n", $ARGV, $., $1;
                next;
            }

            # -------------------------------------------------------------------
            # Pattern 2: "CODE" on the next line
            # The line ends after DBException( with no quoted string.
            # -------------------------------------------------------------------
            my $cur_file = $ARGV;
            my $lnum     = $.;
            my $next     = <>;

            # EOF of all files
            last unless defined $next;

            # Do not cross into the next file
            next unless $ARGV eq $cur_file;

            # Skip if the next line is a comment
            next if $next =~ /^\s*\/\//;

            # The next line must be only whitespace + a quoted string
            if ($next =~ /^\s*"([^"]*)"/) {
                printf "%s:%d:%s\n", $cur_file, $lnum, $1;
            }

        } continue {
            # Reset $. to 0 at end of each file so the next file starts at 1.
            # This gives correct per-file line numbers instead of cumulative ones.
            close ARGV if eof;
        }
    ' "${files[@]}" 2>/dev/null
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
        [ -z "$mark" ] && mark="<EMPTY>"
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
            delete seen
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

# ------------------------------------------------------------------------------
# replace_code_at_line FILE LINENO OLD_CODE NEW_CODE
#
# Replaces OLD_CODE with NEW_CODE at the exact location reported by
# collect_marks. Handles both patterns:
#
#   Pattern 1 — code on LINENO (same line as DBException():
#     sed replaces "OLD_CODE" → "NEW_CODE" on that exact line.
#
#   Pattern 2 — code on LINENO+1 (DBException( is on LINENO, code is next):
#     sed replaces "OLD_CODE" → "NEW_CODE" on LINENO+1.
#
# Line-specific replacement (not global) correctly handles same-file duplicates:
# only the target occurrence is replaced, not all occurrences in the file.
# ------------------------------------------------------------------------------
replace_code_at_line() {
    local file="$1"
    local lineno="$2"
    local old_code="$3"
    local new_code="$4"

    # Check which line the quoted code string actually lives on
    local line_content
    line_content=$(sed -n "${lineno}p" "$file")

    if echo "$line_content" | grep -qF "\"${old_code}\""; then
        # Pattern 1: code is on lineno
        sed -i "${lineno}s|\"${old_code}\"|\"${new_code}\"|" "$file"
    else
        # Pattern 2: code is on the next line
        local next_lineno=$(( lineno + 1 ))
        sed -i "${next_lineno}s|\"${old_code}\"|\"${new_code}\"|" "$file"
    fi
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
    echo "Detected patterns:"
    echo "  Pattern 1 (single-line): DBException(\"CODE\", \"msg\", callstack)"
    echo "  Pattern 2 (multi-line):  DBException("
    echo "                               \"CODE\","
    echo "                               \"msg\", callstack)"
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
            printf "  %-14s  len=%-3s  %s:%s\n" "$mark" "$len" "$file" "$lineno"
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
                printf "    %s:%s\n" "${file#$PROJECT_ROOT/}" "$lineno"
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
        [ -z "$mark" ] && mark="<EMPTY>"
        local len=${#mark}
        [ "$mark" = "<EMPTY>" ] && len=0
        local status
        if is_valid_mark "$mark"; then
            status="OK"
        else
            status="INVALID"
        fi
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
                echo "  ERROR: Failed to generate code for '${mark}' at $file:$lineno"
                continue
            fi
            replace_code_at_line "$file" "$lineno" "$mark" "$new_code"
            echo "  ${mark}  →  ${new_code}  (${file#$PROJECT_ROOT/}:$lineno)"
        done
    else
        echo "  OK: No invalid marks."
    fi

    echo ""

    # 2. Fix cross-file duplicates
    # Strategy: keep the first occurrence (by sorted file path), replace all others.
    # Each replacement generates a fresh unique code via generate_dbexception_code.sh.
    echo "--- Fixing cross-file duplicates ---"
    local duplicates
    duplicates=$(find_duplicate_marks)
    if [ -n "$duplicates" ]; then
        echo "$duplicates" | while read -r old_code; do
            local locations
            locations=$(collect_marks | grep ":${old_code}$")

            local first_file first_lineno
            first_file=$(echo "$locations" | head -1 | cut -d: -f1)
            first_lineno=$(echo "$locations" | head -1 | cut -d: -f2)
            echo "  Keeping ${old_code} in: ${first_file#$PROJECT_ROOT/}:${first_lineno}"

            echo "$locations" | tail -n +2 | while IFS=: read -r file lineno mark; do
                local new_code
                new_code=$("$GENERATE_SCRIPT" "$PROJECT_ROOT" 1 2>/dev/null)
                if [ -z "$new_code" ]; then
                    echo "    ERROR: Failed to generate code at ${file#$PROJECT_ROOT/}:$lineno"
                    continue
                fi
                replace_code_at_line "$file" "$lineno" "$old_code" "$new_code"
                echo "    ${old_code}  →  ${new_code}  (${file#$PROJECT_ROOT/}:$lineno)"
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
