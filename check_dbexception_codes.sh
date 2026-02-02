#!/bin/bash
# Check, list, and fix DBException error codes in the project
# Usage:
#   ./check_dbexception_codes.sh --check   # Check for duplicates (exit 1 if found)
#   ./check_dbexception_codes.sh --list    # List all codes with their locations
#   ./check_dbexception_codes.sh --fix     # Auto-fix problematic duplicates

PROJECT_ROOT="${PROJECT_ROOT:-$(cd "$(dirname "$0")" && pwd -P)}"
LIBS_DIR="$PROJECT_ROOT/libs"
GENERATE_SCRIPT="$PROJECT_ROOT/generate_dbexception_code.sh"

# Find duplicates that are in DIFFERENT files (problematic duplicates)
find_problematic_duplicates() {
    grep -rnP --include="*.cpp" --include="*.hpp" \
        'DBException\s*\(\s*"[A-Z0-9]+"\s*,' "$LIBS_DIR" 2>/dev/null | \
        grep -oP '"[A-Z0-9]+"' | tr -d '"' | sort | uniq -d | while read code; do
        # Get files where this code appears
        files=$(grep -rl --include="*.cpp" --include="*.hpp" "DBException.*\"$code\"" "$LIBS_DIR" 2>/dev/null | sort -u)
        if [ -z "$files" ]; then
            file_count=0
        else
            file_count=$(echo "$files" | wc -l)
        fi

        # If code appears in more than one file, it's problematic
        if [ "$file_count" -gt 1 ]; then
            echo "$code"
        fi
    done
}

# Get all locations for a specific code
get_code_locations() {
    local code=$1
    grep -rn --include="*.cpp" --include="*.hpp" "DBException.*\"$code\"" "$LIBS_DIR" 2>/dev/null
}

case "$1" in
    --check)
        echo "Checking for problematic duplicate DBException codes..."
        duplicates=$(find_problematic_duplicates)

        if [ -n "$duplicates" ]; then
            echo "ERROR: Found duplicate codes in different files:"
            echo "$duplicates" | while read code; do
                echo ""
                echo "=== $code ==="
                get_code_locations "$code"
            done
            exit 1
        else
            echo "OK: No problematic duplicates found."
            exit 0
        fi
        ;;

    --list)
        echo "All DBException codes in the project:"
        echo ""
        grep -rnP --include="*.cpp" --include="*.hpp" \
            'DBException\s*\(\s*"[A-Z0-9]+"\s*,' "$LIBS_DIR" 2>/dev/null | \
            sed -E 's|.*/libs/||' | \
            grep -oP '"[A-Z0-9]+".*' | \
            sed -E 's/"([A-Z0-9]+)".*/\1/' | \
            sort | uniq -c | sort -rn
        ;;

    --fix)
        echo "Checking for problematic duplicate DBException codes..."
        duplicates=$(find_problematic_duplicates)

        if [ -z "$duplicates" ]; then
            echo "OK: No problematic duplicates found. Nothing to fix."
            exit 0
        fi

        if [ ! -x "$GENERATE_SCRIPT" ]; then
            echo "ERROR: generate_dbexception_code.sh not found or not executable at $GENERATE_SCRIPT"
            exit 1
        fi

        echo "Found duplicates. Fixing..."
        echo ""

        # Process each duplicate code
        echo "$duplicates" | while read old_code; do
            echo "=== Fixing $old_code ==="

            # Get all occurrences
            locations=$(get_code_locations "$old_code")

            # Get unique files (we keep the first occurrence, replace others)
            first_file=$(echo "$locations" | head -1 | cut -d: -f1)

            echo "  Keeping in: $first_file"

            # Replace in all other files
            echo "$locations" | tail -n +2 | while read location; do
                file=$(echo "$location" | cut -d: -f1)
                line=$(echo "$location" | cut -d: -f2)

                # Generate new unique code
                new_code=$("$GENERATE_SCRIPT" "$PROJECT_ROOT" 1)

                echo "  Replacing in $file:$line with $new_code"

                # Use sed to replace the code in the specific file (portable for Linux and macOS)
                sed "s/DBException(\"$old_code\"/DBException(\"$new_code\"/g" "$file" > "$file.tmp" && mv "$file.tmp" "$file"
            done

            echo ""
        done

        echo "Fix complete. Running verification..."
        echo ""

        # Verify fix
        remaining=$(find_problematic_duplicates)
        if [ -n "$remaining" ]; then
            echo "WARNING: Some duplicates may still exist:"
            echo "$remaining"
            exit 1
        else
            echo "OK: All problematic duplicates fixed."
            exit 0
        fi
        ;;

    *)
        echo "Usage: $0 [--check|--list|--fix]"
        echo ""
        echo "  --check   Check for duplicate codes in different files (exit 1 if found)"
        echo "  --list    List all codes with occurrence count"
        echo "  --fix     Auto-fix problematic duplicates by generating new unique codes"
        exit 1
        ;;
esac
