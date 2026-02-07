#!/bin/bash

# Script to discover and run cpp_dbc examples
# Dynamically discovers examples from .cpp files in the examples directory

# Get the directory of this script
SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )"
PROJECT_ROOT="$( cd "${SCRIPT_DIR}/../.." &> /dev/null && pwd )"
EXAMPLES_DIR="${SCRIPT_DIR}/examples"

# Source common functions (provides colors, print utilities, etc.)
source "$PROJECT_ROOT/scripts/common/functions.sh"

# Possible build directories where examples might be located
BUILD_DIRS=(
    "${PROJECT_ROOT}/build/libs/cpp_dbc/build"
    "${PROJECT_ROOT}/libs/cpp_dbc/build"
    "${PROJECT_ROOT}/build"
)

show_usage() {
    echo "Usage: $0 [OPTIONS] [example_name|pattern]"
    echo ""
    echo "Discover and run cpp_dbc examples."
    echo ""
    echo "Options:"
    echo "  --help, -h              Show this help message"
    echo "  --list, -l              List all available examples (compiled and not compiled)"
    echo "  --run, -r               Run all available (compiled) examples sequentially"
    echo "  --run=NAME, -r=NAME     Run the specified example"
    echo "  --run=PATTERN           Run examples matching wildcard pattern (*, ?)"
    echo "  --category=CAT          List or run examples from a specific category (subdirectory)"
    echo ""
    echo "Wildcard patterns:"
    echo "  *                       Matches any sequence of characters"
    echo "  ?                       Matches any single character"
    echo ""
    echo "If no options are provided, lists available examples."
    echo ""
    echo "Examples:"
    echo "  $0 --list                       # List all examples"
    echo "  $0 --run                        # Run all examples"
    echo "  $0 --run=10_011_example_config  # Run specific example"
    echo "  $0 10_011_example_config        # Same as above"
    echo "  $0 --run='23_*'                 # Run all examples starting with 23_"
    echo "  $0 --run='*_basic'              # Run all examples ending with _basic"
    echo "  $0 --run='2?_001_*'             # Run examples matching pattern"
    echo "  $0 --category=relational        # List relational examples"
    echo "  $0 --run --category=kv          # Run all kv examples"
    echo ""
    echo "Build examples with:"
    echo "  ./helper.sh --run-build=examples,yaml"
    echo "  ./helper.sh --run-build=examples,yaml,sqlite,postgres,mysql"
    echo "  ./helper.sh --run-build=examples,yaml,mongodb"
    echo "  ./helper.sh --run-build=examples,yaml,redis"
    echo "  ./helper.sh --run-build=examples,yaml,scylladb"
    echo "  ./helper.sh --run-build=examples,yaml,firebird"
    echo "  ./helper.sh --mc-combo-01    # Build with all drivers"
}

# Find the build directory containing examples
find_build_dir() {
    for dir in "${BUILD_DIRS[@]}"; do
        if [ -d "$dir" ]; then
            # Check if any executable exists in this directory (excluding known non-example files)
            local found_exe
            found_exe=$(find "$dir" -maxdepth 1 -type f -executable -name "*example*" 2>/dev/null | head -1)
            if [ -n "$found_exe" ]; then
                echo "$dir"
                return 0
            fi
        fi
    done
    return 1
}

# Discover all example source files and their categories
# Populates global array DISCOVERED_EXAMPLES with "category|example_name" entries
discover_examples() {
    local filter_category="$1"
    DISCOVERED_EXAMPLES=()

    # Find all .cpp files in the examples directory and subdirectories
    # Using maxdepth 4 to handle nested structures like relational/postgresql/*.cpp
    local cpp_files
    mapfile -t cpp_files < <(find "$EXAMPLES_DIR" -maxdepth 4 -name "*.cpp" -type f | sort)

    for cpp_file in "${cpp_files[@]}"; do
        # Get the relative path from EXAMPLES_DIR
        local rel_path="${cpp_file#${EXAMPLES_DIR}/}"

        # Extract category (top-level subdirectory) and example name
        local base_name
        base_name=$(basename "$cpp_file" .cpp)

        # Determine category from the first directory component in the path
        # e.g., "relational/postgresql/file.cpp" -> category is "relational"
        #       "common/file.cpp" -> category is "common"
        local category
        if [[ "$rel_path" == */* ]]; then
            # Has subdirectory - extract first path component
            category="${rel_path%%/*}"
        else
            # File is directly in examples directory
            category="root"
        fi

        # Apply category filter if specified
        if [ -n "$filter_category" ] && [ "$category" != "$filter_category" ]; then
            continue
        fi

        DISCOVERED_EXAMPLES+=("${category}|${base_name}")
    done
}

# Get human-readable category description
get_category_description() {
    local category="$1"
    case "$category" in
        relational)
            echo "Relational Database Examples (MySQL, PostgreSQL, SQLite)"
            ;;
        document)
            echo "Document Database Examples (MongoDB)"
            ;;
        kv)
            echo "Key-Value Database Examples (Redis)"
            ;;
        columnar)
            echo "Columnar Database Examples (ScyllaDB)"
            ;;
        graph)
            echo "Graph Database Examples"
            ;;
        timeseries)
            echo "Time Series Database Examples"
            ;;
        root)
            echo "General Examples"
            ;;
        *)
            echo "$category Examples"
            ;;
    esac
}

# Get build hint for a category
get_build_hint() {
    local category="$1"
    case "$category" in
        relational)
            echo "./helper.sh --run-build=examples,yaml,sqlite,postgres,mysql"
            ;;
        document)
            echo "./helper.sh --run-build=examples,yaml,mongodb"
            ;;
        kv)
            echo "./helper.sh --run-build=examples,yaml,redis"
            ;;
        columnar)
            echo "./helper.sh --run-build=examples,yaml,scylladb"
            ;;
        graph)
            echo "./helper.sh --run-build=examples,yaml  # (graph driver required)"
            ;;
        timeseries)
            echo "./helper.sh --run-build=examples,yaml  # (timeseries driver required)"
            ;;
        *)
            echo "./helper.sh --run-build=examples,yaml"
            ;;
    esac
}

# List all examples with their status
list_examples() {
    local filter_category="$1"
    local build_dir

    echo -e "${CYAN}=== cpp_dbc Examples ===${NC}"
    echo ""

    build_dir=$(find_build_dir) || true

    if [ -z "$build_dir" ]; then
        echo -e "${YELLOW}No compiled examples found.${NC}"
        echo ""
        echo "Build examples using helper.sh:"
        echo "  ./helper.sh --run-build=examples,yaml,sqlite,postgres,mysql,firebird,mongodb,scylladb,redis"
        echo "  ./helper.sh --mc-combo-01  # Builds all drivers + tests + examples"
        echo ""
    else
        echo -e "Build directory: ${GREEN}${build_dir}${NC}"
        echo ""
    fi

    discover_examples "$filter_category"

    local current_category=""
    local total_ready=0
    local total_not_built=0

    for entry in "${DISCOVERED_EXAMPLES[@]}"; do
        IFS='|' read -r category example_name <<< "$entry"

        # Skip empty entries
        [ -z "$category" ] && continue

        # Print category header when it changes
        if [ "$category" != "$current_category" ]; then
            if [ -n "$current_category" ]; then
                echo ""
            fi
            local desc
            desc=$(get_category_description "$category")
            echo -e "${BLUE}[$desc]${NC}"
            current_category="$category"
        fi

        # Check if example is compiled
        if [ -n "$build_dir" ] && [ -f "${build_dir}/${example_name}" ] && [ -x "${build_dir}/${example_name}" ]; then
            echo -e "  ${GREEN}[READY]${NC} $example_name"
            ((total_ready++)) || true
        else
            echo -e "  ${RED}[NOT BUILT]${NC} $example_name"
            ((total_not_built++)) || true
        fi
    done

    echo ""
    echo -e "${CYAN}Summary:${NC} ${GREEN}${total_ready} ready${NC}, ${RED}${total_not_built} not built${NC}"

    if [ -z "$build_dir" ] || [ $total_ready -eq 0 ]; then
        return 1
    fi
    return 0
}

# Find the category of an example by its name
find_example_category() {
    local example_name="$1"

    discover_examples ""

    for entry in "${DISCOVERED_EXAMPLES[@]}"; do
        IFS='|' read -r category name <<< "$entry"
        if [ "$name" == "$example_name" ]; then
            echo "$category"
            return 0
        fi
    done

    return 1
}

# Run a specific example
run_example() {
    local example="$1"
    local build_dir

    build_dir=$(find_build_dir)
    if [ -z "$build_dir" ]; then
        echo -e "${RED}Error: No compiled examples found.${NC}"
        echo ""
        echo "Build examples first using helper.sh:"
        echo "  ./helper.sh --run-build=examples,yaml,sqlite,postgres,mysql,firebird,mongodb,scylladb,redis"
        echo "  ./helper.sh --mc-combo-01  # Builds all drivers + tests + examples"
        echo ""
        echo "Available examples (not compiled):"
        list_examples ""
        return 1
    fi

    local example_path="${build_dir}/${example}"

    if [ ! -f "$example_path" ]; then
        echo -e "${RED}Error: Example '${example}' not found or not compiled.${NC}"
        echo ""

        local category
        category=$(find_example_category "$example") || category="unknown"

        if [ "$category" != "unknown" ]; then
            echo -e "Example '${YELLOW}${example}${NC}' exists in source but is not compiled."
            echo ""
            echo "Build this example using helper.sh:"
            local hint
            hint=$(get_build_hint "$category")
            echo -e "  ${GREEN}${hint}${NC}"
        else
            echo -e "Example '${YELLOW}${example}${NC}' does not exist."
            echo ""
            echo "Available examples:"
            list_examples ""
        fi
        return 1
    fi

    if [ ! -x "$example_path" ]; then
        echo -e "${RED}Error: Example '${example}' is not executable${NC}"
        return 1
    fi

    echo -e "${CYAN}=== Running: ${example} ===${NC}"
    echo -e "Path: ${GREEN}${example_path}${NC}"
    echo ""

    # Change to build directory so examples can find their config files
    cd "$build_dir"

    # Run the example
    "./${example}"
    local exit_code=$?

    echo ""
    if [ $exit_code -eq 0 ]; then
        echo -e "${GREEN}Example completed successfully.${NC}"
    else
        echo -e "${RED}Example exited with code: ${exit_code}${NC}"
    fi

    return $exit_code
}

# Run examples matching a wildcard pattern
# Note: contains_wildcard and matches_wildcard_pattern come from scripts/common/functions.sh
run_examples_by_pattern() {
    local pattern="$1"
    local filter_category="$2"
    local build_dir
    local failed=0
    local succeeded=0
    local skipped=0
    local matched=0

    build_dir=$(find_build_dir)
    if [ -z "$build_dir" ]; then
        echo -e "${RED}Error: No compiled examples found.${NC}"
        echo ""
        echo "Build examples first using helper.sh:"
        echo "  ./helper.sh --run-build=examples,yaml,sqlite,postgres,mysql,firebird,mongodb,scylladb,redis"
        echo "  ./helper.sh --mc-combo-01  # Builds all drivers + tests + examples"
        echo ""
        echo "Available examples (not compiled):"
        list_examples ""
        return 1
    fi

    echo -e "${CYAN}=== Running Examples Matching Pattern: ${pattern} ===${NC}"
    echo ""

    discover_examples "$filter_category"

    # Collect matching examples
    local matching_examples=()
    for entry in "${DISCOVERED_EXAMPLES[@]}"; do
        IFS='|' read -r category example_name <<< "$entry"
        [ -z "$category" ] && continue

        # Check if example name matches the pattern
        if matches_wildcard_pattern "$example_name" "$pattern"; then
            matching_examples+=("${category}|${example_name}")
        fi
    done

    if [ ${#matching_examples[@]} -eq 0 ]; then
        echo -e "${YELLOW}Warning: No examples found matching pattern '${pattern}'${NC}"
        echo ""
        echo "Available examples:"
        list_examples "$filter_category"
        echo ""
        echo "To build examples, use helper.sh:"
        echo "  ./helper.sh --run-build=examples,yaml,sqlite,postgres,mysql,firebird,mongodb,scylladb,redis"
        echo "  ./helper.sh --mc-combo-01  # Builds all drivers + tests + examples"
        return 1
    fi

    echo -e "Found ${GREEN}${#matching_examples[@]}${NC} example(s) matching pattern."
    echo ""

    # Show which examples will be run vs skipped
    local ready_count=0
    local not_built_count=0
    for entry in "${matching_examples[@]}"; do
        IFS='|' read -r category example_name <<< "$entry"
        if [ -f "${build_dir}/${example_name}" ] && [ -x "${build_dir}/${example_name}" ]; then
            ((ready_count++)) || true
        else
            ((not_built_count++)) || true
        fi
    done

    if [ $not_built_count -gt 0 ]; then
        echo -e "${YELLOW}Warning: ${not_built_count} of ${#matching_examples[@]} matching examples are not compiled${NC}"
        echo ""
        # Determine which categories are involved
        declare -A categories_needed
        for entry in "${matching_examples[@]}"; do
            IFS='|' read -r category example_name <<< "$entry"
            if [ ! -f "${build_dir}/${example_name}" ] || [ ! -x "${build_dir}/${example_name}" ]; then
                categories_needed["$category"]=1
            fi
        done

        echo "To compile missing examples, use:"
        for cat in "${!categories_needed[@]}"; do
            local hint
            hint=$(get_build_hint "$cat")
            echo -e "  ${GREEN}${hint}${NC}  # For ${cat} examples"
        done
        echo ""
    fi

    for entry in "${matching_examples[@]}"; do
        IFS='|' read -r category example_name <<< "$entry"
        ((matched++)) || true

        if [ -f "${build_dir}/${example_name}" ] && [ -x "${build_dir}/${example_name}" ]; then
            echo -e "${BLUE}--- Running: ${example_name} (${category}) ---${NC}"
            cd "$build_dir"
            if "./${example_name}"; then
                ((succeeded++)) || true
                echo -e "${GREEN}[PASSED]${NC} ${example_name}"
            else
                ((failed++)) || true
                echo -e "${RED}[FAILED]${NC} ${example_name}"
            fi
            echo ""
        else
            ((skipped++)) || true
            echo -e "${YELLOW}[SKIPPED]${NC} ${example_name} (not built)"
        fi
    done

    echo -e "${CYAN}=== Summary ===${NC}"
    echo -e "  ${BLUE}Matched: ${matched}${NC}"
    echo -e "  ${GREEN}Succeeded: ${succeeded}${NC}"
    echo -e "  ${RED}Failed: ${failed}${NC}"
    echo -e "  ${YELLOW}Skipped (not built): ${skipped}${NC}"

    if [ $failed -gt 0 ]; then
        return 1
    fi
    return 0
}

# Run all available examples
run_all_examples() {
    local filter_category="$1"
    local build_dir
    local failed=0
    local succeeded=0
    local skipped=0

    build_dir=$(find_build_dir)
    if [ -z "$build_dir" ]; then
        echo -e "${RED}Error: No compiled examples found.${NC}"
        echo ""
        echo "Build examples first using helper.sh:"
        echo "  ./helper.sh --run-build=examples,yaml,sqlite,postgres,mysql,firebird,mongodb,scylladb,redis"
        echo "  ./helper.sh --mc-combo-01  # Builds all drivers + tests + examples"
        return 1
    fi

    echo -e "${CYAN}=== Running All Available Examples ===${NC}"
    echo ""

    discover_examples "$filter_category"

    for entry in "${DISCOVERED_EXAMPLES[@]}"; do
        IFS='|' read -r category example_name <<< "$entry"
        [ -z "$category" ] && continue

        if [ -f "${build_dir}/${example_name}" ] && [ -x "${build_dir}/${example_name}" ]; then
            echo -e "${BLUE}--- Running: ${example_name} (${category}) ---${NC}"
            cd "$build_dir"
            if "./${example_name}"; then
                ((succeeded++)) || true
                echo -e "${GREEN}[PASSED]${NC} ${example_name}"
            else
                ((failed++)) || true
                echo -e "${RED}[FAILED]${NC} ${example_name}"
            fi
            echo ""
        else
            ((skipped++)) || true
        fi
    done

    echo -e "${CYAN}=== Summary ===${NC}"
    echo -e "  ${GREEN}Succeeded: ${succeeded}${NC}"
    echo -e "  ${RED}Failed: ${failed}${NC}"
    echo -e "  ${YELLOW}Skipped (not built): ${skipped}${NC}"

    if [ $failed -gt 0 ]; then
        return 1
    fi
    return 0
}

# Main script logic
main() {
    local action="list"
    local example_name=""
    local category=""

    # Parse arguments
    while [[ $# -gt 0 ]]; do
        case "$1" in
            --help|-h)
                show_usage
                exit 0
                ;;
            --list|-l)
                action="list"
                shift
                ;;
            --run|-r|--run=|-r=)
                action="run_all"
                shift
                ;;
            --run=*|-r=*)
                example_name="${1#*=}"
                # Check if it's a wildcard pattern or exact name
                if contains_wildcard "$example_name"; then
                    action="run_pattern"
                else
                    action="run"
                fi
                shift
                ;;
            --category=*)
                category="${1#*=}"
                shift
                ;;
            -*)
                echo "Unknown option: $1"
                show_usage
                exit 1
                ;;
            *)
                # Assume it's an example name or pattern to run
                if [ -z "$example_name" ]; then
                    example_name="$1"
                    if contains_wildcard "$example_name"; then
                        action="run_pattern"
                    else
                        action="run"
                    fi
                fi
                shift
                ;;
        esac
    done

    case "$action" in
        list)
            list_examples "$category"
            ;;
        run)
            if [ -z "$example_name" ]; then
                echo "Error: No example name specified"
                echo "Use --list to see available examples"
                exit 1
            fi
            run_example "$example_name"
            ;;
        run_pattern)
            run_examples_by_pattern "$example_name" "$category"
            ;;
        run_all)
            run_all_examples "$category"
            ;;
    esac
}

main "$@"
