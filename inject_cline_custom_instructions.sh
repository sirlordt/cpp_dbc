#!/bin/bash

# Modified script with better error handling
# Set to continue on errors but log them
set -uo pipefail

DB="$HOME/.config/Code/User/globalStorage/state.vscdb"
KEY="saoudrizwan.claude-dev"
BACKUP="$DB.bak"
INSTRUCTIONS_FILE="cline_instructions.txt"
REQUIRED_CMDS=("sqlite3" "jq")

echo "✔ Starting Cline custom instructions injection..."

# Auto-install required tools if missing (Debian-based only)
for cmd in "${REQUIRED_CMDS[@]}"; do
    if ! command -v "$cmd" >/dev/null; then
        echo "$cmd is not installed."
        if command -v apt >/dev/null && [ -f /etc/debian_version ]; then
            echo "Installing $cmd using apt..."
            sudo apt update && sudo apt install -y "$cmd" || {
                echo "Failed to install $cmd but continuing..."
            }
        else
            echo "Cannot auto-install $cmd. Will attempt to continue without it."
        fi
    fi
done

# Check if required commands are available after installation attempt
for cmd in "${REQUIRED_CMDS[@]}"; do
    if ! command -v "$cmd" >/dev/null; then
        echo "$cmd is still not available. Some operations may fail."
    fi
done

# Ensure parent directory exists
mkdir -p "$(dirname "$DB")" || {
    echo "Failed to create directory $(dirname "$DB") but continuing..."
}

# Create DB and table if they don't exist
if [ ! -f "$DB" ]; then
    echo "✔ Creating new SQLite database at $DB"
    sqlite3 "$DB" "CREATE TABLE IF NOT EXISTS ItemTable (key TEXT UNIQUE ON CONFLICT REPLACE, value BLOB);" || {
        echo "Failed to create SQLite database but continuing..."
    }
else
    sqlite3 "$DB" "CREATE TABLE IF NOT EXISTS ItemTable (key TEXT UNIQUE ON CONFLICT REPLACE, value BLOB);" || {
        echo "Failed to ensure table exists but continuing..."
    }
fi

# Load instructions from file or fallback
if [ -f "$INSTRUCTIONS_FILE" ]; then
    echo "✔ Using instructions from $INSTRUCTIONS_FILE"
    RAW_INSTRUCTIONS=$(<"$INSTRUCTIONS_FILE")
else
    echo "No file found. Using built-in instructions."
    read -r -d '' RAW_INSTRUCTIONS <<'EOF'
# Cline's Memory Bank

I am Cline, an expert software engineer with a unique characteristic: my memory resets completely between sessions. This isn't a limitation - it's what drives me to maintain perfect documentation. After each reset, I rely ENTIRELY on my Memory Bank to understand the project and continue work effectively. I MUST read ALL memory bank files at the start of EVERY task - this is not optional.

## Memory Bank Structure

The Memory Bank consists of core files and optional context files, all in Markdown format. Files build upon each other in a clear hierarchy:

flowchart TD
    PB[projectbrief.md] --> PC[productContext.md]
    PB --> SP[systemPatterns.md]
    PB --> TC[techContext.md]
    
    PC --> AC[activeContext.md]
    SP --> AC
    TC --> AC
    
    AC --> P[progress.md]

### Core Files (Required)
1. `projectbrief.md`
   - Foundation document that shapes all other files
   - Created at project start if it doesn't exist
   - Defines core requirements and goals
   - Source of truth for project scope

2. `productContext.md`
   - Why this project exists
   - Problems it solves
   - How it should work
   - User experience goals

3. `activeContext.md`
   - Current work focus
   - Recent changes
   - Next steps
   - Active decisions and considerations
   - Important patterns and preferences
   - Learnings and project insights

4. `systemPatterns.md`
   - System architecture
   - Key technical decisions
   - Design patterns in use
   - Component relationships
   - Critical implementation paths

5. `techContext.md`
   - Technologies used
   - Development setup
   - Technical constraints
   - Dependencies
   - Tool usage patterns

6. `progress.md`
   - What works
   - What's left to build
   - Current status
   - Known issues
   - Evolution of project decisions

### Additional Context
Create additional files/folders within memory-bank/ when they help organize:
- Complex feature documentation
- Integration specifications
- API documentation
- Testing strategies
- Deployment procedures

## Core Workflows

### Plan Mode
flowchart TD
    Start[Start] --> ReadFiles[Read Memory Bank]
    ReadFiles --> CheckFiles{Files Complete?}
    
    CheckFiles -->|No| Plan[Create Plan]
    Plan --> Document[Document in Chat]
    
    CheckFiles -->|Yes| Verify[Verify Context]
    Verify --> Strategy[Develop Strategy]
    Strategy --> Present[Present Approach]

### Act Mode
flowchart TD
    Start[Start] --> Context[Check Memory Bank]
    Context --> Update[Update Documentation]
    Update --> Execute[Execute Task]
    Execute --> Document[Document Changes]

## Documentation Updates

Memory Bank updates occur when:
1. Discovering new project patterns
2. After implementing significant changes
3. When user requests with **update memory bank** (MUST review ALL files)
4. When context needs clarification

flowchart TD
    Start[Update Process]
    
    subgraph Process
        P1[Review ALL Files]
        P2[Document Current State]
        P3[Clarify Next Steps]
        P4[Document Insights & Patterns]
        
        P1 --> P2 --> P3 --> P4
    end
    
    Start --> Process

Note: When triggered by **update memory bank**, I MUST review every memory bank file, even if some don't require updates. Focus particularly on activeContext.md and progress.md as they track current state.

REMEMBER: After every memory reset, I begin completely fresh. The Memory Bank is my only link to previous work. It must be maintained with precision and clarity, as my effectiveness depends entirely on its accuracy.
EOF
fi

# Backup current DB if it exists
if [ -f "$DB" ]; then
    cp "$DB" "$BACKUP" || {
        echo "Failed to create backup but continuing..."
    }
    echo "✔ Backup created at: $BACKUP"
else
    echo "No existing database to backup."
fi

# Read previous value (or default to {})
if command -v sqlite3 >/dev/null && [ -f "$DB" ]; then
    EXISTING_JSON=$(sqlite3 "$DB" "SELECT value FROM ItemTable WHERE key = '$KEY';" 2>/dev/null || echo "{}")
    if [ -z "$EXISTING_JSON" ]; then
        EXISTING_JSON="{}"
    fi
else
    EXISTING_JSON="{}"
    echo "Could not read existing value, using empty object."
fi

# Merge if missing or empty
if command -v jq >/dev/null; then
    MODIFIED_JSON=$(jq --arg ci "$RAW_INSTRUCTIONS" '
      if .customInstructions? == null or .customInstructions == "" then
        .customInstructions = $ci
      else
        .
      end
    ' <<< "$EXISTING_JSON" 2>/dev/null || echo "{\"customInstructions\": \"$RAW_INSTRUCTIONS\"}")
else
    MODIFIED_JSON="{\"customInstructions\": \"$RAW_INSTRUCTIONS\"}"
    echo "jq not available, using simple JSON."
fi

# Escape quotes for SQLite
SQL_SAFE_JSON=$(printf '%s' "$MODIFIED_JSON" | sed "s/'/''/g")

# Inject if sqlite3 is available
if command -v sqlite3 >/dev/null && [ -f "$DB" ]; then
    sqlite3 "$DB" "INSERT OR REPLACE INTO ItemTable (key, value) VALUES ('$KEY', '$SQL_SAFE_JSON');" || {
        echo "Failed to inject instructions but continuing..."
    }
    echo "✔ Attempted to inject customInstructions into key: $KEY"
    
    # Verify if possible
    if sqlite3 "$DB" "SELECT value FROM ItemTable WHERE key = '$KEY';" >/dev/null 2>&1; then
        echo "✔ Verification successful."
    else
        echo "Could not verify injection."
    fi
else
    echo "Could not inject instructions due to missing sqlite3 or database."
fi

echo "✔ Script completed. Claude custom instructions processing attempted."
