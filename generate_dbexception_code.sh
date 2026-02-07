#!/bin/bash
# Generate unique DBException error codes
# Format: 12-character uppercase alphanumeric (A-Z, 0-9) with at least 5 letters
#         and no more than 4 consecutive repeated characters

show_usage() {
    echo "Usage: $0 [PROJECT_ROOT] [COUNT] [--help|-h]"
    echo ""
    echo "Generate unique DBException error codes for the cpp_dbc project."
    echo ""
    echo "Arguments:"
    echo "  PROJECT_ROOT  Path to project root directory (default: current directory)"
    echo "  COUNT         Number of codes to generate (default: 1)"
    echo ""
    echo "Options:"
    echo "  --help, -h    Show this help message"
    echo ""
    echo "Examples:"
    echo "  $0                    # Generate 1 code using current directory"
    echo "  $0 /path/to/project   # Generate 1 code for specified project"
    echo "  $0 /path/to/project 5 # Generate 5 codes for specified project"
}

# Handle --help argument
if [[ "$1" == "--help" ]] || [[ "$1" == "-h" ]]; then
    show_usage
    exit 0
fi

PROJECT_ROOT="${1:-$(pwd)}"
COUNT="${2:-1}"

# Validate COUNT is a positive integer
if ! [[ "$COUNT" =~ ^[1-9][0-9]*$ ]]; then
    echo "ERROR: COUNT must be a positive integer: $COUNT" >&2
    exit 1
fi

if [[ ! -d "$PROJECT_ROOT" ]]; then
    echo "ERROR: Project root directory does not exist: $PROJECT_ROOT" >&2
    exit 1
fi

generate_code() {
    local MAX_ITERATIONS=1000
    local iteration=0

    while true; do
        iteration=$((iteration + 1))
        if [[ $iteration -gt $MAX_ITERATIONS ]]; then
            echo "ERROR: Failed to generate unique code after $MAX_ITERATIONS attempts" >&2
            return 1
        fi

        # Generate random 12-char code with A-Z and 0-9
        CODE=$(tr -dc 'A-Z0-9' < /dev/urandom | fold -w 12 | head -n 1)

        # Count letters in the code
        LETTER_COUNT=$(echo "$CODE" | tr -cd 'A-Z' | wc -c)

        # Check for 5+ consecutive repeated characters
        HAS_REPEAT=$(echo "$CODE" | grep -E '(.)\1{4,}')

        # Verify: at least 5 letters and no 5+ consecutive repeats
        if [[ "$LETTER_COUNT" -ge 5 ]] && [[ -z "$HAS_REPEAT" ]]; then
            # Check if code already exists in the project
            if ! grep -rq "\"$CODE\"" "$PROJECT_ROOT/libs" 2>/dev/null; then
                echo "$CODE"
                return 0
            fi
        fi
    done
}

exit_code=0
for ((i=1; i<=COUNT; i++)); do
    if ! generate_code; then
        exit_code=1
    fi
done
exit $exit_code
