#!/bin/bash
# Generate unique DBException error codes
# Format: 12-character uppercase alphanumeric (A-Z, 0-9) with at least 5 letters
#         and no more than 4 consecutive repeated characters

PROJECT_ROOT="${1:-$(pwd)}"
COUNT="${2:-1}"

generate_code() {
    while true; do
        # Generate random 12-char code with A-Z and 0-9
        CODE=$(cat /dev/urandom | tr -dc 'A-Z0-9' | fold -w 12 | head -n 1)

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

for ((i=1; i<=COUNT; i++)); do
    generate_code
done
