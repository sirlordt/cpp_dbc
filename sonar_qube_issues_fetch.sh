#!/bin/bash

set -euo pipefail

# =============================================================================
# SonarCloud Issues Fetcher
# Fetches issues for a specific file from SonarCloud API
# Supports both file UUID and filename (auto-resolves UUID)
# =============================================================================

readonly SCRIPT_NAME=$(basename "$0")
readonly SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
readonly DEFAULT_OUTPUT_DIR="./issues"
readonly DEFAULT_API_URL="https://sonarcloud.io/api"
readonly PROPERTIES_FILE="${SCRIPT_DIR}/.sonarcloud.properties"

# Colors for output
readonly RED='\033[0;31m'
readonly GREEN='\033[0;32m'
readonly YELLOW='\033[1;33m'
readonly BLUE='\033[0;34m'
readonly CYAN='\033[0;36m'
readonly NC='\033[0m' # No Color

# -----------------------------------------------------------------------------
# Logging functions
# -----------------------------------------------------------------------------
log_info() {
    echo -e "${BLUE}[INFO]${NC} $1"
}

log_success() {
    echo -e "${GREEN}[SUCCESS]${NC} $1"
}

log_warning() {
    echo -e "${YELLOW}[WARNING]${NC} $1"
}

log_error() {
    echo -e "${RED}[ERROR]${NC} $1" >&2
}

log_debug() {
    if [[ "${DEBUG:-false}" == "true" ]]; then
        echo -e "${CYAN}[DEBUG]${NC} $1"
    fi
}

# -----------------------------------------------------------------------------
# Load project key from .sonarcloud.properties file
# -----------------------------------------------------------------------------
load_project_from_properties() {
    if [[ ! -f "$PROPERTIES_FILE" ]]; then
        log_error "Properties file not found: ${PROPERTIES_FILE}"
        log_error "Please create a .sonarcloud.properties file in the script directory with:"
        log_error "  sonar.projectKey=your_project_key"
        exit 1
    fi
    
    local project_key
    project_key=$(grep -E "^sonar\.projectKey=" "$PROPERTIES_FILE" 2>/dev/null | cut -d'=' -f2- | tr -d '[:space:]')
    
    if [[ -z "$project_key" ]]; then
        log_error "sonar.projectKey not found or empty in: ${PROPERTIES_FILE}"
        log_error "Please add the following line to your .sonarcloud.properties file:"
        log_error "  sonar.projectKey=your_project_key"
        exit 1
    fi
    
    echo "$project_key"
}

# -----------------------------------------------------------------------------
# Show usage information
# -----------------------------------------------------------------------------
show_usage() {
    cat << EOF
Usage: ${SCRIPT_NAME} [--file=<filename|component_key>] [OPTIONS]

Fetch SonarCloud issues and save them as JSON.
If --file is not specified, fetches ALL issues from ALL files in the project.

Options:
  --file=NAME|KEY       Filename (e.g., driver_mysql.cpp) or component key
                        If a filename is provided, the script will resolve
                        it to a component key automatically
                        If omitted, fetches issues from all files
  --token=TOKEN         SonarCloud API token (optional)
                        Falls back to SONAR_QUBE_TOKEN env variable
                        If neither available, attempts unauthenticated request
  --project=KEY         Project key (overrides .sonarcloud.properties)
  --branch=NAME         Branch name (default: main/master branch)
  --pr=ID|auto          Pull request ID/number, or 'auto' to detect from
                        current git branch
  --url=URL             API base URL (default: ${DEFAULT_API_URL})
  --out=DIR             Output directory (default: ${DEFAULT_OUTPUT_DIR})
  --statuses=LIST       Issue statuses, comma-separated (default: OPEN,CONFIRMED)
  --qualities=LIST      Software qualities filter (default: MAINTAINABILITY)
  --list-files          List all files in the project with their component keys
  --list-branches       List all branches in the project
  --list-prs            List all pull requests in the project
  --debug               Enable debug output
  -h, --help            Show this help message

Configuration:
  The script reads the project key from .sonarcloud.properties file located
  in the same directory as the script. The file should contain:
    sonar.projectKey=your_project_key

Examples:
  # Fetch ALL issues from ALL files (organized by file in timestamped folder)
  ${SCRIPT_NAME}
  ${SCRIPT_NAME} --branch=develop
  ${SCRIPT_NAME} --pr=42

  # Using filename (uses main/master branch by default)
  ${SCRIPT_NAME} --file=driver_mysql.cpp

  # Get issues from a specific branch
  ${SCRIPT_NAME} --file=driver_mysql.cpp --branch=develop
  ${SCRIPT_NAME} --file=driver_mysql.cpp --branch=feature/new-driver

  # Get issues from a pull request (by ID)
  ${SCRIPT_NAME} --file=driver_mysql.cpp --pr=42

  # Auto-detect PR from current git branch
  ${SCRIPT_NAME} --file=driver_mysql.cpp --pr=auto

  # List available branches and PRs
  ${SCRIPT_NAME} --list-branches
  ${SCRIPT_NAME} --list-prs

  # List all files in project
  ${SCRIPT_NAME} --list-files

  # With custom API URL (e.g., self-hosted SonarQube)
  ${SCRIPT_NAME} --file=driver.cpp --url=https://sonar.mycompany.com/api

Environment Variables:
  SONAR_QUBE_TOKEN      Default API token if --token not provided
  DEBUG=true            Enable debug output

EOF
}

# -----------------------------------------------------------------------------
# Check and install dependencies
# -----------------------------------------------------------------------------
check_dependencies() {
    local missing_deps=()
    
    if ! command -v curl &> /dev/null; then
        missing_deps+=("curl")
    fi
    
    if ! command -v jq &> /dev/null; then
        missing_deps+=("jq")
    fi
    
    if ! command -v git &> /dev/null; then
        missing_deps+=("git")
    fi
    
    if [[ ${#missing_deps[@]} -eq 0 ]]; then
        log_info "All dependencies are installed"
        return 0
    fi
    
    log_warning "Missing dependencies: ${missing_deps[*]}"
    
    # Helper function to run command with or without sudo
    run_pkg_cmd() {
        if [[ $EUID -eq 0 ]]; then
            "$@"
        elif command -v sudo &> /dev/null; then
            sudo "$@"
        else
            "$@"
        fi
    }
    
    # Detect package manager and install
    if command -v apt-get &> /dev/null; then
        log_info "Detected apt package manager, installing dependencies..."
        run_pkg_cmd apt-get update -qq 2>/dev/null || true
        run_pkg_cmd apt-get install -y -qq "${missing_deps[@]}"
    elif command -v dnf &> /dev/null; then
        log_info "Detected dnf package manager, installing dependencies..."
        run_pkg_cmd dnf install -y -q "${missing_deps[@]}"
    elif command -v yum &> /dev/null; then
        log_info "Detected yum package manager, installing dependencies..."
        run_pkg_cmd yum install -y -q "${missing_deps[@]}"
    elif command -v pacman &> /dev/null; then
        log_info "Detected pacman package manager, installing dependencies..."
        run_pkg_cmd pacman -S --noconfirm "${missing_deps[@]}"
    elif command -v brew &> /dev/null; then
        log_info "Detected Homebrew, installing dependencies..."
        brew install "${missing_deps[@]}"
    elif command -v apk &> /dev/null; then
        log_info "Detected apk package manager, installing dependencies..."
        run_pkg_cmd apk add --no-cache "${missing_deps[@]}"
    elif command -v zypper &> /dev/null; then
        log_info "Detected zypper package manager, installing dependencies..."
        run_pkg_cmd zypper install -y "${missing_deps[@]}"
    else
        log_error "Could not detect package manager. Please install manually: ${missing_deps[*]}"
        exit 1
    fi
    
    log_success "Dependencies installed successfully"
}

# -----------------------------------------------------------------------------
# Parse command line arguments
# -----------------------------------------------------------------------------
parse_arguments() {
    FILE_INPUT=""
    TOKEN=""
    PROJECT=""
    BRANCH=""
    PULL_REQUEST=""
    API_URL="${DEFAULT_API_URL}"
    OUTPUT_DIR="${DEFAULT_OUTPUT_DIR}"
    STATUSES="OPEN,CONFIRMED"
    QUALITIES="MAINTAINABILITY"
    LIST_FILES=false
    LIST_BRANCHES=false
    LIST_PRS=false
    DEBUG="${DEBUG:-false}"
    
    for arg in "$@"; do
        case $arg in
            --file=*)
                FILE_INPUT="${arg#*=}"
                ;;
            --token=*)
                TOKEN="${arg#*=}"
                ;;
            --project=*)
                PROJECT="${arg#*=}"
                ;;
            --branch=*)
                BRANCH="${arg#*=}"
                ;;
            --pr=*)
                PULL_REQUEST="${arg#*=}"
                ;;
            --url=*)
                API_URL="${arg#*=}"
                # Remove trailing slash if present
                API_URL="${API_URL%/}"
                ;;
            --out=*)
                OUTPUT_DIR="${arg#*=}"
                ;;
            --statuses=*)
                STATUSES="${arg#*=}"
                ;;
            --qualities=*)
                QUALITIES="${arg#*=}"
                ;;
            --list-files)
                LIST_FILES=true
                ;;
            --list-branches)
                LIST_BRANCHES=true
                ;;
            --list-prs)
                LIST_PRS=true
                ;;
            --debug)
                DEBUG=true
                ;;
            -h|--help)
                show_usage
                exit 0
                ;;
            *)
                log_error "Unknown argument: $arg"
                show_usage
                exit 1
                ;;
        esac
    done
    
    # Check for conflicting options
    if [[ -n "$BRANCH" && -n "$PULL_REQUEST" ]]; then
        log_error "Cannot specify both --branch and --pr at the same time"
        exit 1
    fi
    
    # Load project from properties file if not provided via command line
    if [[ -z "$PROJECT" ]]; then
        PROJECT=$(load_project_from_properties)
        log_info "Loaded project from ${PROPERTIES_FILE}: ${PROJECT}"
    fi
    
    # Handle --pr=auto: detect current branch and find associated PR
    if [[ "$PULL_REQUEST" == "auto" ]]; then
        log_info "Auto-detecting PR from current git branch..."
        local current_branch
        current_branch=$(get_current_branch)
        PULL_REQUEST=$(resolve_pr_from_branch "$current_branch")
    fi
    
    # Note: --file is now optional. If not provided, all issues will be fetched
    
    # Token fallback to environment variable
    if [[ -z "$TOKEN" ]]; then
        if [[ -n "${SONAR_QUBE_TOKEN:-}" ]]; then
            TOKEN="$SONAR_QUBE_TOKEN"
            log_info "Using token from SONAR_QUBE_TOKEN environment variable"
        else
            log_warning "No token provided, attempting unauthenticated request"
        fi
    fi
}

# -----------------------------------------------------------------------------
# Make API request with optional authentication
# -----------------------------------------------------------------------------
api_request() {
    local url="$1"
    local response
    local http_code
    
    log_debug "API Request: ${url}" >&2
    
    if [[ -n "$TOKEN" ]]; then
        response=$(curl -s -w "\n%{http_code}" -H "Authorization: Bearer ${TOKEN}" "${url}" 2>&1) || true
    else
        response=$(curl -s -w "\n%{http_code}" "${url}" 2>&1) || true
    fi
    
    # Extract HTTP code (last line) and body (everything else)
    http_code=$(echo "$response" | tail -n1)
    response=$(echo "$response" | sed '$d')
    
    log_debug "HTTP Code: ${http_code}" >&2
    
    # Check for errors
    if [[ "$http_code" == "000" ]]; then
        log_error "Network error: Could not connect to SonarCloud API"
        log_error "Please check your internet connection and firewall settings"
        exit 1
    fi
    
    if [[ "$http_code" != "200" ]]; then
        log_error "API request failed with HTTP code: ${http_code}"
        if echo "$response" | jq -e '.errors' &>/dev/null; then
            local error_msg
            error_msg=$(echo "$response" | jq -r '.errors[0].msg // "Unknown error"')
            log_error "API Error: ${error_msg}"
        fi
        exit 1
    fi
    
    echo "$response"
}

# -----------------------------------------------------------------------------
# Get current git branch name
# -----------------------------------------------------------------------------
get_current_branch() {
    local branch=""
    
    # Try git branch --show-current (Git 2.22+)
    branch=$(git branch --show-current 2>/dev/null)
    
    # Fallback for older git versions
    if [[ -z "$branch" ]]; then
        branch=$(git rev-parse --abbrev-ref HEAD 2>/dev/null)
    fi
    
    if [[ -z "$branch" || "$branch" == "HEAD" ]]; then
        log_error "Could not detect current git branch"
        log_error "Make sure you are in a git repository and not in detached HEAD state"
        exit 1
    fi
    
    echo "$branch"
}

# -----------------------------------------------------------------------------
# Resolve PR ID from branch name
# -----------------------------------------------------------------------------
resolve_pr_from_branch() {
    local branch_name="$1"
    
    log_info "Detecting PR for branch: ${branch_name}" >&2
    
    local url="${API_URL}/project_pull_requests/list"
    url="${url}?project=${PROJECT}"
    
    local response
    response=$(api_request "$url")
    
    local pr_count
    pr_count=$(echo "$response" | jq '.pullRequests | length')
    
    if [[ "$pr_count" -eq 0 ]]; then
        log_error "No pull requests found for project: ${PROJECT}"
        exit 1
    fi
    
    # Find PR matching the branch name
    local pr_id
    pr_id=$(echo "$response" | jq -r --arg branch "$branch_name" '
        .pullRequests[] | select(.branch == $branch) | .key
    ' | head -n1)
    
    if [[ -z "$pr_id" || "$pr_id" == "null" ]]; then
        log_error "No pull request found for branch: ${branch_name}"
        log_info "Available pull requests:" >&2
        echo "" >&2
        echo "$response" | jq -r '.pullRequests[] | "  PR #\(.key): \(.branch) -> \(.base // "main")"' >&2
        echo "" >&2
        exit 1
    fi
    
    log_success "Found PR #${pr_id} for branch: ${branch_name}" >&2
    echo "$pr_id"
}

# -----------------------------------------------------------------------------
# Check if input looks like a component key (contains project prefix with colon)
# Component keys look like: project_key:path/to/file.cpp
# -----------------------------------------------------------------------------
is_component_key() {
    local input="$1"
    # Component keys contain a colon followed by a path
    if [[ "$input" == *":"* ]]; then
        return 0
    fi
    return 1
}

# -----------------------------------------------------------------------------
# List all files in the project
# -----------------------------------------------------------------------------
list_project_files() {
    log_info "Fetching file list for project: ${PROJECT}"
    if [[ -n "$PULL_REQUEST" ]]; then
        log_info "Pull Request: ${PULL_REQUEST}"
    elif [[ -n "$BRANCH" ]]; then
        log_info "Branch: ${BRANCH}"
    fi
    
    local page=1
    local page_size=500
    local total=0
    local fetched=0
    
    echo ""
    printf "${CYAN}%-70s %s${NC}\n" "FILE PATH" "COMPONENT KEY"
    printf "%s\n" "$(printf '=%.0s' {1..120})"
    
    while true; do
        local url="${API_URL}/components/tree"
        url="${url}?component=${PROJECT}"
        url="${url}&qualifiers=FIL"
        url="${url}&strategy=leaves"
        url="${url}&ps=${page_size}"
        url="${url}&p=${page}"
        
        # Add branch or pull request filter
        if [[ -n "$PULL_REQUEST" ]]; then
            url="${url}&pullRequest=${PULL_REQUEST}"
        elif [[ -n "$BRANCH" ]]; then
            url="${url}&branch=${BRANCH}"
        fi
        
        local response
        response=$(api_request "$url")
        
        if [[ $page -eq 1 ]]; then
            total=$(echo "$response" | jq '.paging.total')
            log_info "Total files in project: ${total}"
            echo ""
        fi
        
        local count
        count=$(echo "$response" | jq '.components | length')
        
        if [[ $count -eq 0 ]]; then
            break
        fi
        
        # Print files with their component keys
        echo "$response" | jq -r '.components[] | "\(.path // .name)\t\(.key // "N/A")"' | \
        while IFS=$'\t' read -r path key; do
            printf "%-70s %s\n" "$path" "$key"
        done
        
        fetched=$((fetched + count))
        
        if [[ $fetched -ge $total ]]; then
            break
        fi
        
        page=$((page + 1))
    done
    
    echo ""
    log_success "Listed ${fetched} files"
}

# -----------------------------------------------------------------------------
# List all branches in the project
# -----------------------------------------------------------------------------
list_project_branches() {
    log_info "Fetching branch list for project: ${PROJECT}"
    
    local url="${API_URL}/project_branches/list"
    url="${url}?project=${PROJECT}"
    
    local response
    response=$(api_request "$url")
    
    local count
    count=$(echo "$response" | jq '.branches | length')
    
    echo ""
    printf "${CYAN}%-40s %-15s %-20s %s${NC}\n" "BRANCH NAME" "TYPE" "STATUS" "LAST ANALYSIS"
    printf "%s\n" "$(printf '=%.0s' {1..100})"
    
    echo "$response" | jq -r '.branches[] | "\(.name)\t\(.type // "N/A")\t\(.status.qualityGateStatus // "N/A")\t\(.analysisDate // "N/A")"' | \
    while IFS=$'\t' read -r name type status date; do
        # Format date if present
        if [[ "$date" != "N/A" ]]; then
            date=$(echo "$date" | cut -d'T' -f1)
        fi
        printf "%-40s %-15s %-20s %s\n" "$name" "$type" "$status" "$date"
    done
    
    echo ""
    log_success "Listed ${count} branches"
}

# -----------------------------------------------------------------------------
# List all pull requests in the project
# -----------------------------------------------------------------------------
list_project_prs() {
    log_info "Fetching pull request list for project: ${PROJECT}"
    
    local url="${API_URL}/project_pull_requests/list"
    url="${url}?project=${PROJECT}"
    
    local response
    response=$(api_request "$url")
    
    local count
    count=$(echo "$response" | jq '.pullRequests | length')
    
    if [[ "$count" -eq 0 ]]; then
        log_warning "No pull requests found for project: ${PROJECT}"
        return
    fi
    
    echo ""
    printf "${CYAN}%-10s %-50s %-20s %s${NC}\n" "PR ID" "TITLE" "BRANCH" "STATUS"
    printf "%s\n" "$(printf '=%.0s' {1..110})"
    
    echo "$response" | jq -r '.pullRequests[] | "\(.key)\t\(.title // "N/A")\t\(.branch // "N/A")\t\(.status.qualityGateStatus // "N/A")"' | \
    while IFS=$'\t' read -r key title branch status; do
        # Truncate title if too long
        if [[ ${#title} -gt 47 ]]; then
            title="${title:0:47}..."
        fi
        printf "%-10s %-50s %-20s %s\n" "$key" "$title" "$branch" "$status"
    done
    
    echo ""
    log_success "Listed ${count} pull requests"
}

# -----------------------------------------------------------------------------
# Resolve filename to UUID
# -----------------------------------------------------------------------------
resolve_file_uuid() {
    local filename="$1"
    
    log_info "Resolving filename to component key: ${filename}"
    
    # Extract just the basename for search query
    local search_name
    search_name=$(basename "$filename")
    
    # Use components/tree API to search for the file
    local url="${API_URL}/components/tree"
    url="${url}?component=${PROJECT}"
    url="${url}&qualifiers=FIL"
    url="${url}&strategy=leaves"
    url="${url}&q=${search_name}"
    url="${url}&ps=100"
    
    # Add branch or pull request filter
    if [[ -n "$PULL_REQUEST" ]]; then
        url="${url}&pullRequest=${PULL_REQUEST}"
    elif [[ -n "$BRANCH" ]]; then
        url="${url}&branch=${BRANCH}"
    fi
    
    local response
    response=$(api_request "$url")
    
    log_debug "API Response: ${response}" >&2
    
    local total
    total=$(echo "$response" | jq '.paging.total // 0')
    
    log_debug "Total results: ${total}" >&2
    
    if [[ "$total" -eq 0 ]]; then
        log_error "File not found in project: ${filename}"
        log_info "Use --list-files to see all available files"
        exit 1
    fi
    
    # Initialize variables
    local component_key=""
    local matched_path=""
    
    # Debug: show what fields are available
    log_debug "Available fields in first component:" >&2
    log_debug "$(echo "$response" | jq '.components[0] | keys' 2>/dev/null)" >&2
    log_debug "First component data:" >&2
    log_debug "$(echo "$response" | jq '.components[0]' 2>/dev/null)" >&2
    
    # If the user provided a path, try exact match first
    if [[ "$filename" == *"/"* ]]; then
        component_key=$(echo "$response" | jq -r --arg path "$filename" '
            .components[] | select(.path == $path) | .key // empty
        ' | head -n1)
        
        if [[ -n "$component_key" ]]; then
            matched_path="$filename"
        else
            # Path provided but no exact match - show available options
            log_error "Exact path not found: ${filename}"
            log_info "Files matching '${search_name}':"
            echo ""
            echo "$response" | jq -r '.components[] | "  \(.path)"'
            echo ""
            log_info "Please use one of the paths above"
            exit 1
        fi
    fi
    
    # If no path provided, check how many matches we have
    if [[ -z "$component_key" ]]; then
        local match_count
        match_count=$(echo "$response" | jq '.components | length')
        
        log_debug "Match count: ${match_count}" >&2
        
        if [[ "$match_count" -gt 1 ]]; then
            # Multiple matches - require user to specify the path
            log_warning "Multiple files match '${filename}':"
            echo ""
            echo "$response" | jq -r '.components[] | "  \(.path)"'
            echo ""
            log_error "Ambiguous filename. Please specify the full path:"
            echo ""
            echo "$response" | jq -r '.components[] | "  --file=\(.path)"'
            echo ""
            exit 1
        else
            # Single match - use it
            component_key=$(echo "$response" | jq -r '.components[0].key // empty')
            matched_path=$(echo "$response" | jq -r '.components[0] | (.path // .name) // empty')
            log_debug "Extracted component key: ${component_key}" >&2
            log_debug "Extracted path: ${matched_path}" >&2
        fi
    fi
    
    if [[ -z "$component_key" ]]; then
        log_error "Could not resolve component key for: ${filename}"
        log_error "Debug: Run with --debug flag to see API response"
        exit 1
    fi
    
    log_success "Resolved: ${matched_path} -> ${component_key}"
    
    # Export for use in main
    FILE_COMPONENT_KEY="$component_key"
    RESOLVED_FILENAME="$matched_path"
}

# -----------------------------------------------------------------------------
# Fetch issues from SonarCloud API
# -----------------------------------------------------------------------------
fetch_issues() {
    local url="${API_URL}/issues/search"
    url="${url}?componentKeys=${FILE_COMPONENT_KEY}"
    url="${url}&issueStatuses=${STATUSES}"
    url="${url}&impactSoftwareQualities=${QUALITIES}"
    url="${url}&ps=500"  # Page size (max)
    
    # Add branch or pull request filter
    if [[ -n "$PULL_REQUEST" ]]; then
        url="${url}&pullRequest=${PULL_REQUEST}"
    elif [[ -n "$BRANCH" ]]; then
        url="${url}&branch=${BRANCH}"
    fi
    
    log_info "Fetching issues from SonarCloud API..." >&2
    log_info "Project: ${PROJECT}" >&2
    log_info "File: ${RESOLVED_FILENAME:-${FILE_COMPONENT_KEY}}" >&2
    log_info "Component Key: ${FILE_COMPONENT_KEY}" >&2
    if [[ -n "$PULL_REQUEST" ]]; then
        log_info "Pull Request: ${PULL_REQUEST}" >&2
    elif [[ -n "$BRANCH" ]]; then
        log_info "Branch: ${BRANCH}" >&2
    else
        log_info "Branch: (default/main)" >&2
    fi
    log_info "Statuses: ${STATUSES}" >&2
    log_info "Qualities: ${QUALITIES}" >&2
    
    api_request "$url"
}

# -----------------------------------------------------------------------------
# Fetch ALL issues from all files in the project (with pagination)
# -----------------------------------------------------------------------------
fetch_all_issues() {
    log_info "Fetching ALL issues from project: ${PROJECT}" >&2
    if [[ -n "$PULL_REQUEST" ]]; then
        log_info "Pull Request: ${PULL_REQUEST}" >&2
    elif [[ -n "$BRANCH" ]]; then
        log_info "Branch: ${BRANCH}" >&2
    else
        log_info "Branch: (default/main)" >&2
    fi
    log_info "Statuses: ${STATUSES}" >&2
    log_info "Qualities: ${QUALITIES}" >&2

    local page=1
    local page_size=500
    local all_issues="[]"
    local total=0
    local fetched=0

    while true; do
        local url="${API_URL}/issues/search"
        url="${url}?componentKeys=${PROJECT}"
        url="${url}&issueStatuses=${STATUSES}"
        url="${url}&impactSoftwareQualities=${QUALITIES}"
        url="${url}&ps=${page_size}"
        url="${url}&p=${page}"

        # Add branch or pull request filter
        if [[ -n "$PULL_REQUEST" ]]; then
            url="${url}&pullRequest=${PULL_REQUEST}"
        elif [[ -n "$BRANCH" ]]; then
            url="${url}&branch=${BRANCH}"
        fi

        local response
        response=$(api_request "$url")

        if [[ $page -eq 1 ]]; then
            total=$(echo "$response" | jq '.total // 0')
            log_info "Total issues in project: ${total}" >&2
        fi

        local page_issues
        page_issues=$(echo "$response" | jq '.issues // []')
        local count
        count=$(echo "$page_issues" | jq 'length')

        if [[ $count -eq 0 ]]; then
            break
        fi

        # Merge issues
        all_issues=$(echo "$all_issues" "$page_issues" | jq -s 'add')

        fetched=$((fetched + count))
        log_info "Fetched ${fetched}/${total} issues..." >&2

        if [[ $fetched -ge $total ]]; then
            break
        fi

        page=$((page + 1))
    done

    # Return combined response
    echo "{\"total\": ${total}, \"issues\": ${all_issues}}"
}

# -----------------------------------------------------------------------------
# Create timestamped output directory
# -----------------------------------------------------------------------------
create_timestamped_dir() {
    local timestamp
    timestamp=$(date +%Y-%m-%d-%H-%M-%S)
    local timestamped_dir="${OUTPUT_DIR}/${timestamp}"

    if [[ ! -d "$timestamped_dir" ]]; then
        log_info "Creating timestamped directory: ${timestamped_dir}" >&2
        mkdir -p "$timestamped_dir"
    fi

    echo "$timestamped_dir"
}

# -----------------------------------------------------------------------------
# Organize issues by file and save to separate JSON files
# -----------------------------------------------------------------------------
save_issues_by_file() {
    local response="$1"
    local output_dir="$2"

    local total_issues
    total_issues=$(echo "$response" | jq '.total // 0')
    local issues_count
    issues_count=$(echo "$response" | jq '.issues | length')

    log_info "Organizing ${issues_count} issues by file..."

    # Get unique component keys (files) from issues
    local components
    components=$(echo "$response" | jq -r '.issues[].component // empty' | sort -u)

    if [[ -z "$components" ]]; then
        log_warning "No issues found to organize"
        return
    fi

    local file_count=0
    local branch_suffix=""

    # Determine branch/PR suffix
    if [[ -n "$PULL_REQUEST" ]]; then
        branch_suffix="_pr${PULL_REQUEST}"
    elif [[ -n "$BRANCH" ]]; then
        local safe_branch="${BRANCH//\//_}"
        branch_suffix="_${safe_branch}"
    else
        local default_branch
        default_branch=$(get_default_branch)
        local safe_branch="${default_branch//\//_}"
        branch_suffix="_${safe_branch}"
    fi

    while IFS= read -r component; do
        if [[ -z "$component" ]]; then
            continue
        fi

        # Extract filename from component key (format: project:path/to/file.ext)
        local file_path
        file_path=$(echo "$component" | sed 's/.*://')

        # Get the parent directory name to disambiguate files with the same basename
        local parent_dir
        parent_dir=$(basename "$(dirname "$file_path")")

        # Get just the filename (basename), not the full path
        local base_filename
        base_filename=$(basename "$file_path")

        # Replace dots with underscores except for the extension
        # e.g., system_utils.hpp -> system_utils_hpp
        local safe_filename
        safe_filename=$(echo "$base_filename" | sed 's/\./_/g')

        # Prepend parent directory to avoid collisions between files with the same name
        # e.g., postgresql/connection_01.cpp -> postgresql_connection_01_cpp
        if [[ -n "$parent_dir" && "$parent_dir" != "." ]]; then
            safe_filename="${parent_dir}_${safe_filename}"
        fi

        # Filter issues for this component
        local file_issues
        file_issues=$(echo "$response" | jq --arg comp "$component" '{
            total: ([.issues[] | select(.component == $comp)] | length),
            component: $comp,
            file_path: ($comp | split(":") | .[1] // $comp),
            issues: [.issues[] | select(.component == $comp)]
        }')

        local issue_count
        issue_count=$(echo "$file_issues" | jq '.total')

        if [[ "$issue_count" -gt 0 ]]; then
            # Format: {issue_count}_{parent_dir}_{filename}_{branch}_issues.json
            local output_file="${output_dir}/${issue_count}_${safe_filename}${branch_suffix}_issues.json"
            echo "$file_issues" | jq '.' > "$output_file"
            log_success "  Saved ${issue_count} issues to: $(basename "$output_file")"
            file_count=$((file_count + 1))
        fi
    done <<< "$components"

    echo ""
    log_info "Summary:"
    echo -e "  Total issues found: ${YELLOW}${total_issues}${NC}"
    echo -e "  Files with issues: ${YELLOW}${file_count}${NC}"
    echo -e "  Output directory: ${YELLOW}${output_dir}${NC}"

    # Show issue types breakdown if issues exist
    if [[ "$issues_count" -gt 0 ]]; then
        echo -e "  ${BLUE}Issues by severity:${NC}"
        echo "$response" | jq -r '
            .issues | group_by(.severity) |
            map({severity: .[0].severity, count: length}) |
            .[] | "    \(.severity // "UNKNOWN"): \(.count)"
        ' 2>/dev/null || true

        echo -e "  ${BLUE}Issues by type:${NC}"
        echo "$response" | jq -r '
            .issues | group_by(.type) |
            map({type: .[0].type, count: length}) |
            .[] | "    \(.type // "UNKNOWN"): \(.count)"
        ' 2>/dev/null || true
    fi
}

# -----------------------------------------------------------------------------
# Get default branch name for the project
# -----------------------------------------------------------------------------
get_default_branch() {
    local url="${API_URL}/project_branches/list"
    url="${url}?project=${PROJECT}"

    local response
    # Run api_request in a subshell so any exit only affects the subshell, not the whole script
    response=$( (api_request "$url" 2>/dev/null) ) || true
    
    if [[ -n "$response" ]]; then
        local main_branch
        main_branch=$(echo "$response" | jq -r '.branches[] | select(.isMain == true) | .name' 2>/dev/null | head -n1)
        if [[ -n "$main_branch" && "$main_branch" != "null" ]]; then
            echo "$main_branch"
            return
        fi
    fi
    
    # Fallback to "main"
    echo "main"
}

# -----------------------------------------------------------------------------
# Extract filename from API response (fallback)
# -----------------------------------------------------------------------------
extract_filename() {
    local response="$1"
    local filename
    local extension=""
    local branch_suffix=""
    
    # Determine branch/PR suffix (always include it)
    if [[ -n "$PULL_REQUEST" ]]; then
        branch_suffix="_pr${PULL_REQUEST}"
    elif [[ -n "$BRANCH" ]]; then
        # Sanitize branch name for filename (replace / with _)
        local safe_branch="${BRANCH//\//_}"
        branch_suffix="_${safe_branch}"
    else
        # Get default branch name
        local default_branch
        default_branch=$(get_default_branch)
        local safe_branch="${default_branch//\//_}"
        branch_suffix="_${safe_branch}"
    fi
    
    # Extract extension from resolved filename
    if [[ -n "${RESOLVED_FILENAME:-}" ]]; then
        local base
        base=$(basename "$RESOLVED_FILENAME")
        # Extract extension (e.g., .cpp -> _cpp, .hpp -> _hpp)
        if [[ "$base" == *.* ]]; then
            extension="_${base##*.}"
        fi
        # Get filename without extension
        filename="${base%.*}"
    elif [[ -n "${FILE_COMPONENT_KEY:-}" ]]; then
        local base
        base=$(echo "$FILE_COMPONENT_KEY" | sed 's/.*://' | sed 's/.*\///')
        base=$(basename "$base")
        if [[ "$base" == *.* ]]; then
            extension="_${base##*.}"
        fi
        filename="${base%.*}"
    else
        # Fallback: try to get from issues[].component
        local component
        component=$(echo "$response" | jq -r '.issues[0]?.component // empty' 2>/dev/null)
        if [[ -n "$component" ]]; then
            local base
            base=$(echo "$component" | sed 's/.*://' | sed 's/.*\///')
            base=$(basename "$base")
            if [[ "$base" == *.* ]]; then
                extension="_${base##*.}"
            fi
            filename="${base%.*}"
        fi
    fi
    
    # Final fallback: use timestamp
    if [[ -z "$filename" ]]; then
        filename="issues_$(date +%Y%m%d_%H%M%S)"
        log_warning "Could not determine filename, using timestamp" >&2
    fi
    
    echo "${filename}${extension}${branch_suffix}_issues.json"
}

# -----------------------------------------------------------------------------
# Save issues to file
# -----------------------------------------------------------------------------
save_issues() {
    local response="$1"
    local output_file="$2"
    
    # Create output directory if it doesn't exist
    if [[ ! -d "$OUTPUT_DIR" ]]; then
        log_info "Creating output directory: ${OUTPUT_DIR}"
        mkdir -p "$OUTPUT_DIR"
    fi
    
    local full_path="${OUTPUT_DIR}/${output_file}"
    
    # Pretty print and save
    echo "$response" | jq '.' > "$full_path"
    
    log_success "Issues saved to: ${full_path}"
    
    # Show summary
    local total_issues
    local open_issues
    total_issues=$(echo "$response" | jq '.total // 0')
    open_issues=$(echo "$response" | jq '.issues | length')
    
    log_info "Summary:"
    echo -e "  Total issues found: ${YELLOW}${total_issues}${NC}"
    echo -e "  Issues in response: ${YELLOW}${open_issues}${NC}"
    
    # Show issue types breakdown if issues exist
    if [[ "$open_issues" -gt 0 ]]; then
        echo -e "  ${BLUE}Issues by severity:${NC}"
        echo "$response" | jq -r '
            .issues | group_by(.severity) | 
            map({severity: .[0].severity, count: length}) |
            .[] | "    \(.severity // "UNKNOWN"): \(.count)"
        ' 2>/dev/null || true
        
        echo -e "  ${BLUE}Issues by type:${NC}"
        echo "$response" | jq -r '
            .issues | group_by(.type) | 
            map({type: .[0].type, count: length}) |
            .[] | "    \(.type // "UNKNOWN"): \(.count)"
        ' 2>/dev/null || true
    fi
}

# -----------------------------------------------------------------------------
# Main function
# -----------------------------------------------------------------------------
main() {
    echo ""
    echo -e "${BLUE}╔══════════════════════════════════════════╗${NC}"
    echo -e "${BLUE}║     SonarCloud Issues Fetcher            ║${NC}"
    echo -e "${BLUE}╚══════════════════════════════════════════╝${NC}"
    echo ""
    
    parse_arguments "$@"
    check_dependencies
    
    echo ""
    
    # Handle --list-files mode
    if [[ "$LIST_FILES" == "true" ]]; then
        list_project_files
        exit 0
    fi
    
    # Handle --list-branches mode
    if [[ "$LIST_BRANCHES" == "true" ]]; then
        list_project_branches
        exit 0
    fi
    
    # Handle --list-prs mode
    if [[ "$LIST_PRS" == "true" ]]; then
        list_project_prs
        exit 0
    fi

    # Create timestamped output directory
    local timestamped_dir
    timestamped_dir=$(create_timestamped_dir)

    # Check if --file was provided
    if [[ -z "$FILE_INPUT" ]]; then
        # Fetch ALL issues from all files
        log_info "No --file specified, fetching ALL issues from project..."
        echo ""

        local response
        response=$(fetch_all_issues)

        echo ""

        # Save issues organized by file
        save_issues_by_file "$response" "$timestamped_dir"
    else
        # Resolve filename to component key if needed
        if is_component_key "$FILE_INPUT"; then
            log_info "Input appears to be a component key"
            FILE_COMPONENT_KEY="$FILE_INPUT"
            RESOLVED_FILENAME=""
        else
            log_info "Input appears to be a filename, resolving component key..."
            resolve_file_uuid "$FILE_INPUT"
        fi

        echo ""

        local response
        response=$(fetch_issues)

        local output_file
        output_file=$(extract_filename "$response")

        log_info "Output filename: ${output_file}"

        # Save to timestamped directory
        local full_path="${timestamped_dir}/${output_file}"

        # Pretty print and save
        echo "$response" | jq '.' > "$full_path"

        log_success "Issues saved to: ${full_path}"

        # Show summary
        local total_issues
        local open_issues
        total_issues=$(echo "$response" | jq '.total // 0')
        open_issues=$(echo "$response" | jq '.issues | length')

        log_info "Summary:"
        echo -e "  Total issues found: ${YELLOW}${total_issues}${NC}"
        echo -e "  Issues in response: ${YELLOW}${open_issues}${NC}"

        # Show issue types breakdown if issues exist
        if [[ "$open_issues" -gt 0 ]]; then
            echo -e "  ${BLUE}Issues by severity:${NC}"
            echo "$response" | jq -r '
                .issues | group_by(.severity) |
                map({severity: .[0].severity, count: length}) |
                .[] | "    \(.severity // "UNKNOWN"): \(.count)"
            ' 2>/dev/null || true

            echo -e "  ${BLUE}Issues by type:${NC}"
            echo "$response" | jq -r '
                .issues | group_by(.type) |
                map({type: .[0].type, count: length}) |
                .[] | "    \(.type // "UNKNOWN"): \(.count)"
            ' 2>/dev/null || true
        fi
    fi

    echo ""
    log_success "Done!"
}

# Run main function
main "$@"