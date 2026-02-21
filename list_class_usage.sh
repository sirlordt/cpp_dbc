#!/bin/bash
# list_class_usage.sh
# Finds all call sites of a class's public methods in specified files.
# Delegates method discovery to list_public_methods.sh.
#
# Usage: ./list_class_usage.sh --class=ClassName [--file=PATH_OR_GLOB] [--dir=PROJECT_DIR]
#
# Examples:
#   ./list_class_usage.sh --class=DatabaseConfigManager --file="libs/cpp_dbc/test/*.cpp"
#   ./list_class_usage.sh --class=MySQLDBConnection --file=libs/cpp_dbc/test/
#   ./list_class_usage.sh --class=RelationalDBConnectionPool --file="libs/cpp_dbc/src/*.cpp"

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
CLASS_NAME=""
FILE_ARG="."
PROJECT_DIR="$SCRIPT_DIR"

usage() {
    echo "Usage: $0 --class=ClassName [--file=PATH_OR_GLOB] [--dir=PROJECT_DIR]"
    echo ""
    echo "Finds all call sites of a class's public methods in specified files."
    echo "Uses list_public_methods.sh to discover the class's public interface."
    echo ""
    echo "Options:"
    echo "  --class=ClassName        C++ class name (required)"
    echo "  --file=PATH_OR_GLOB      Files to search (quote globs to prevent shell expansion)"
    echo "  --dir=PROJECT_DIR        Project root for finding class headers (default: script dir)"
    echo ""
    echo "Examples:"
    echo "  $0 --class=DatabaseConfigManager --file=\"libs/cpp_dbc/test/*.cpp\""
    echo "  $0 --class=MySQLDBConnection --file=libs/cpp_dbc/test/"
    echo "  $0 --class=RelationalDBConnectionPool --file=\"libs/cpp_dbc/src/*.cpp\""
}

for arg in "$@"; do
    case "$arg" in
        --class=*)   CLASS_NAME="${arg#--class=}" ;;
        --file=*)    FILE_ARG="${arg#--file=}" ;;
        --dir=*)     PROJECT_DIR="${arg#--dir=}" ;;
        -h|--help)   usage; exit 0 ;;
        *)
            echo "Error: Unknown argument: $arg" >&2
            usage >&2
            exit 1
            ;;
    esac
done

if [[ -z "$CLASS_NAME" ]]; then
    echo "Error: --class=ClassName is required" >&2
    usage >&2
    exit 1
fi

if [[ ! -d "$PROJECT_DIR" ]]; then
    echo "Error: Project directory not found: $PROJECT_DIR" >&2
    exit 1
fi

# ── Delegate method discovery to list_public_methods.sh ──────────────────────

METHODS_TMPFILE=$(mktemp /tmp/list_class_usage.XXXXXX)
trap 'rm -f "$METHODS_TMPFILE"' EXIT

if ! "$SCRIPT_DIR/list_public_methods.sh" \
        --class="$CLASS_NAME" \
        --dir="$PROJECT_DIR" \
        --output-format=structured \
        > "$METHODS_TMPFILE" 2>&1; then
    # list_public_methods.sh already printed an error; relay it
    cat "$METHODS_TMPFILE" >&2
    exit 1
fi

# ── Python: parse methods + search for usages ─────────────────────────────────

python3 - "$CLASS_NAME" "$FILE_ARG" "$PROJECT_DIR" "$METHODS_TMPFILE" << 'PYEOF'
"""
Parses the structured output of list_public_methods.sh to extract public method
names, then searches target files for call sites of those methods.
"""

import sys
import re
from pathlib import Path

# ── ANSI colours ──────────────────────────────────────────────────────────────

def _colours(enabled: bool):
    if enabled:
        return dict(bold="\033[1m", cyan="\033[36m", green="\033[32m",
                    yellow="\033[33m", gray="\033[90m", red="\033[31m",
                    reset="\033[0m")
    return {k: "" for k in ("bold", "cyan", "green", "yellow", "gray", "red", "reset")}

C = _colours(sys.stdout.isatty())

# ── Constants ─────────────────────────────────────────────────────────────────

SKIP_DIRS = {"build", "cmake-build-debug", "cmake-build-release",
             ".git", "_build", "CMakeFiles"}

_SEP60 = C['gray'] + '─' * 60 + C['reset']
_LBL_W = 10

def _lbl(name: str) -> str:
    return f"  {(name + ':'):<{_LBL_W}} "

# ── Parse list_public_methods.sh structured output ────────────────────────────

_ANSI_RE = re.compile(r'\033\[[0-9;]*m')


def parse_method_names(methods_file: Path) -> tuple[str, list[str]]:
    """
    Parse the structured output of list_public_methods.sh.
    Returns (full_class_name, [unique_method_names]).

    Strategy: collect  Name:  fields only from the 'header:' section,
    skipping the 'ancestors:' section (which also has Name: lines for base classes).
    The 'implementation:' section is a duplicate of header — stop there.
    """
    try:
        lines = methods_file.read_text(errors='replace').splitlines()
    except OSError:
        return '', []

    full_class_name = ''
    method_names: list[str] = []
    seen_names: set[str] = set()
    in_header = False

    for line in lines:
        # Strip ANSI escape codes (output may have them if run from a TTY sub-shell)
        clean = _ANSI_RE.sub('', line)

        # First non-indented, non-empty line → fully-qualified class name
        if not full_class_name and clean.strip() and not clean.startswith(' '):
            full_class_name = clean.strip()
            continue

        stripped = clean.strip()

        # Section tracking
        if stripped == 'header:':
            in_header = True
            continue
        if stripped in ('implementation:', 'ancestors:'):
            if in_header and stripped == 'implementation:':
                break          # header section is complete; stop here
            in_header = False
            continue

        # Collect method names only inside the header section
        if in_header:
            m = re.match(r'\s+Name:\s+(\S+)', clean)
            if m:
                name = m.group(1)
                if name not in seen_names:
                    seen_names.add(name)
                    method_names.append(name)

    return full_class_name, method_names


# ── File discovery ────────────────────────────────────────────────────────────

_SRC_PATTERNS = ('*.cpp', '*.cc', '*.cxx', '*.hpp', '*.h')


def should_include(p: Path, root: Path) -> bool:
    try:
        return not any(
            part in SKIP_DIRS or part.startswith('.')
            for part in p.relative_to(root).parts
        )
    except ValueError:
        return True


def collect_files(file_arg: str) -> list[Path]:
    """
    Expand file_arg into a sorted list of Paths.

    Accepted forms:
      "./path/file.cpp"          → single file
      "./path/dir/"              → all C++ files recursively
      "./path/dir/*.cpp"         → *.cpp files recursively in that dir
    """
    if '*' in file_arg:
        if '/' in file_arg:
            dir_part, pat_part = file_arg.rsplit('/', 1)
        else:
            dir_part, pat_part = '.', file_arg
        base_dir = Path(dir_part) if dir_part else Path('.')
        if not base_dir.exists():
            return []
        return sorted(f for f in base_dir.rglob(pat_part) if should_include(f, base_dir))

    p = Path(file_arg)
    if p.is_file():
        return [p]
    if p.is_dir():
        files: list[Path] = []
        for pat in _SRC_PATTERNS:
            files.extend(f for f in p.rglob(pat) if should_include(f, p))
        return sorted(files)
    return []


# ── Usage search ──────────────────────────────────────────────────────────────

def strip_line_comment(line: str) -> str:
    """Remove trailing // comment, respecting string literals."""
    in_string = False
    i = 0
    while i < len(line):
        c = line[i]
        if c == '\\' and in_string:
            i += 2
            continue
        if c == '"':
            in_string = not in_string
        elif not in_string and c == '/' and i + 1 < len(line) and line[i + 1] == '/':
            return line[:i]
        i += 1
    return line


def build_patterns(method_names: list[str]) -> list[tuple[str, re.Pattern]]:
    """
    For each method name, build a regex that matches:
      obj.method(        instance call with dot
      ptr->method(       pointer call with arrow
      Class::method(     qualified / static call
      obj.method<        template method call
    """
    result = []
    for mname in method_names:
        escaped = re.escape(mname)
        pat = re.compile(r'(?:\.|-\>|::)\s*' + escaped + r'\s*[\(<]')
        result.append((mname, pat))
    return result


def find_usages(files: list[Path], patterns: list[tuple[str, re.Pattern]],
                project_dir: Path) -> list[tuple[str, int, str]]:
    """
    Search files for method call sites.
    Returns sorted list of (rel_path, line_num, method_name).
    Each (file, line, method) combination appears at most once.
    """
    seen: set = set()
    results: list = []

    for filepath in files:
        try:
            raw_lines = filepath.read_text(errors='replace').splitlines()
        except OSError:
            continue

        try:
            rel = str(filepath.relative_to(project_dir))
        except ValueError:
            rel = str(filepath)

        for line_num, raw_line in enumerate(raw_lines, 1):
            stripped = raw_line.strip()
            # Skip pure comment lines
            if stripped.startswith(('*', '/*', '//')):
                continue
            # Strip inline // comment before matching
            clean = strip_line_comment(raw_line)

            for mname, pat in patterns:
                if pat.search(clean):
                    key = (rel, line_num, mname)
                    if key not in seen:
                        seen.add(key)
                        results.append(key)

    results.sort(key=lambda x: (x[0], x[1]))
    return results


# ── Display ───────────────────────────────────────────────────────────────────

def display(full_class_name: str, method_names: list[str],
            usages: list[tuple[str, int, str]]):
    n_methods = len(method_names)
    n_usages  = len(usages)

    print(f"{C['bold']}{C['cyan']}{full_class_name}{C['reset']}")
    print(f"  {C['gray']}{n_methods} public method{'s' if n_methods != 1 else ''} · "
          f"{n_usages} usage{'s' if n_usages != 1 else ''} found{C['reset']}")
    print()

    if not usages:
        print(f"  {C['gray']}(no usages found){C['reset']}")
        return

    for i, (rel, line_num, mname) in enumerate(usages):
        if i > 0:
            print(_SEP60)
        print(f"{_lbl('Method')}{C['bold']}{mname}{C['reset']}")
        print(f"{_lbl('Location')}{C['green']}{rel}:{line_num}{C['reset']}")
    print()


# ── Entry point ───────────────────────────────────────────────────────────────

def main():
    if len(sys.argv) < 5:
        print("Usage: script class_name file_arg project_dir methods_tmpfile",
              file=sys.stderr)
        sys.exit(1)

    class_name   = sys.argv[1]
    file_arg     = sys.argv[2]
    project_dir  = Path(sys.argv[3]).resolve()
    methods_file = Path(sys.argv[4])

    # Step 1: parse method names from list_public_methods.sh output
    full_class_name, method_names = parse_method_names(methods_file)

    if not method_names:
        print(f"{C['red']}Error:{C['reset']} no public methods found for '{class_name}'",
              file=sys.stderr)
        sys.exit(1)

    # Step 2: collect target files
    files = collect_files(file_arg)
    if not files:
        print(f"{C['yellow']}Warning:{C['reset']} no files found matching: {file_arg}",
              file=sys.stderr)
        sys.exit(1)

    # Step 3: build call-site patterns and search
    patterns = build_patterns(method_names)
    usages   = find_usages(files, patterns, project_dir)

    # Step 4: display
    display(full_class_name, method_names, usages)


main()
PYEOF
