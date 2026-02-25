#!/bin/bash
# list_class.sh
# Lists unique C++ class names found in matching source files.
#
# Usage: ./list_class.sh [--folder=PATH_OR_GLOB]
#
# The --folder argument can be:
#   A directory:    ./libs/cpp_dbc/src/      (searches *.cpp, *.hpp, *.h recursively)
#   A glob pattern: "./libs/cpp_dbc/src/drivers/*.cpp"  (must be quoted)
#
# Examples:
#   ./list_class.sh --folder="./libs/cpp_dbc/src/drivers/*.cpp"
#   ./list_class.sh --folder="./libs/cpp_dbc/include/*.hpp"
#   ./list_class.sh --folder=./libs/cpp_dbc/src/
#   ./list_class.sh --folder=.

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
FOLDER_ARG="."

usage() {
    echo "Usage: $0 [--folder=PATH_OR_GLOB]"
    echo ""
    echo "Lists unique C++ class names found in matching files."
    echo ""
    echo "Options:"
    echo "  --folder=PATH_OR_GLOB    Directory or glob pattern (quote globs to prevent shell expansion)"
    echo ""
    echo "Examples:"
    echo "  $0 --folder=\"./libs/cpp_dbc/src/drivers/*.cpp\""
    echo "  $0 --folder=\"./libs/cpp_dbc/include/*.hpp\""
    echo "  $0 --folder=./libs/cpp_dbc/src/"
}

for arg in "$@"; do
    case "$arg" in
        --folder=*) FOLDER_ARG="${arg#--folder=}" ;;
        -h|--help)  usage; exit 0 ;;
        *)
            echo "Error: Unknown argument: $arg" >&2
            usage >&2
            exit 1
            ;;
    esac
done

python3 - "$FOLDER_ARG" << 'PYEOF'
"""C++ class name extractor.

Searches C++ source files for class definitions and implementation markers,
then reports all unique class names found.
"""

import sys
import re
from pathlib import Path

# ── ANSI colours ─────────────────────────────────────────────────────────────

def _colours(enabled: bool):
    if enabled:
        return dict(bold="\033[1m", cyan="\033[36m", green="\033[32m",
                    yellow="\033[33m", gray="\033[90m", red="\033[31m",
                    reset="\033[0m")
    return {k: "" for k in ("bold", "cyan", "green", "yellow", "gray", "red", "reset")}

C = _colours(sys.stdout.isatty())

# ── Directories to skip ───────────────────────────────────────────────────────

SKIP_DIRS = {"build", "cmake-build-debug", "cmake-build-release",
             ".git", "_build", "CMakeFiles"}

# ── Names that follow 'class'/'struct' but are NOT class names ───────────────

NON_CLASS_NAMES = {
    "public", "private", "protected", "virtual", "friend",
    "override", "final", "inline", "static", "const", "constexpr",
    "namespace", "typename", "template", "operator", "alignas",
    "decltype", "explicit", "mutable", "volatile",
}

# ── Low-level helpers ─────────────────────────────────────────────────────────

def strip_line_comment(line: str) -> str:
    """Remove trailing // comment from a line (respects string literals)."""
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

# ── Class name extraction ─────────────────────────────────────────────────────

# Pattern 1: class/struct Name declaration (min 3 chars)
#   Matches: class MyClass, struct FooBar, template<...> class FooBar
_DECL_RE = re.compile(r'\b(?:class|struct)\s+([A-Za-z_][A-Za-z0-9_]{2,})\b')

# Pattern 2: ClassName:: implementation marker (UpperCamelCase, min 3 chars)
#   Matches: MySQLDBConnection::connect, FirebirdDBConnection::~FirebirdDBConnection
#   Filters: std:: (lowercase start), short abbreviations
#   Negative lookahead skips  Namespace::Class::  (namespace-level prefix)
_IMPL_RE = re.compile(r'\b([A-Z][A-Za-z0-9_]{2,})::(?![\w~]+::)(?=[\w~])')

# Pattern 3: namespace component names — used to subtract false positives
#   Matches both: namespace A  and  namespace A::B::C
_NS_RE = re.compile(r'\bnamespace\s+([\w:]+)')


def collect_namespace_names(files: list) -> set:
    """
    Return the set of all namespace *component* names declared across the files.
    e.g. 'namespace cpp_dbc::MySQL {' contributes {'cpp_dbc', 'MySQL'}.
    These names are subtracted from class candidates to remove false positives.
    """
    ns_names: set = set()
    for filepath in files:
        try:
            with open(filepath, 'r', errors='replace') as fh:
                lines = fh.readlines()
        except OSError:
            continue
        for raw_line in lines:
            stripped = strip_line_comment(raw_line).strip()
            if not stripped or stripped.startswith(('*', '/*', '//')):
                continue
            for m in _NS_RE.finditer(stripped):
                for part in m.group(1).split('::'):
                    part = part.strip()
                    if part:
                        ns_names.add(part)
    return ns_names


def extract_classes_from_file(filepath: Path) -> set:
    """Return set of class names found in a single file."""
    try:
        with open(filepath, 'r', errors='replace') as fh:
            lines = fh.readlines()
    except OSError:
        return set()

    classes: set = set()

    for raw_line in lines:
        stripped = strip_line_comment(raw_line).strip()
        if not stripped:
            continue
        # Skip pure comment lines
        if stripped.startswith(('*', '/*', '//')):
            continue
        # Skip preprocessor directives (too noisy)
        if stripped.startswith('#'):
            continue

        # Pattern 1: class/struct declarations
        for m in _DECL_RE.finditer(stripped):
            name = m.group(1)
            if name not in NON_CLASS_NAMES:
                classes.add(name)

        # Pattern 2: ClassName:: implementation markers
        for m in _IMPL_RE.finditer(stripped):
            name = m.group(1)
            if name not in NON_CLASS_NAMES:
                classes.add(name)

    return classes

# ── File discovery ─────────────────────────────────────────────────────────────

_DEFAULT_PATTERNS = ("*.cpp", "*.hpp", "*.h")


def should_include(p: Path, root: Path) -> bool:
    try:
        return not any(
            part in SKIP_DIRS or part.startswith('.')
            for part in p.relative_to(root).parts
        )
    except ValueError:
        return True


def collect_files(folder_arg: str) -> tuple[Path, list[Path]]:
    """
    Parse folder_arg into (base_dir, [file_list]).

    Accepted forms:
      "./path/to/dir/"          → search all C++ files recursively
      "./path/to/dir"           → same
      "./path/to/dir/*.cpp"     → search only *.cpp recursively
      "./path/to/dir/*.hpp"     → search only *.hpp recursively
    """
    if '*' in folder_arg:
        # Split on the last '/' before the wildcard
        if '/' in folder_arg:
            dir_part, pat_part = folder_arg.rsplit('/', 1)
        else:
            dir_part, pat_part = '.', folder_arg
        base_dir = Path(dir_part) if dir_part else Path('.')
        patterns = (pat_part,)
    else:
        base_dir = Path(folder_arg)
        patterns = _DEFAULT_PATTERNS

    if not base_dir.exists():
        print(f"{C['red']}Error:{C['reset']} directory not found: {base_dir}",
              file=sys.stderr)
        sys.exit(1)

    files: list[Path] = []
    for pat in patterns:
        files.extend(f for f in base_dir.rglob(pat) if should_include(f, base_dir))

    return base_dir, files


# ── Entry point ───────────────────────────────────────────────────────────────

def main():
    folder_arg = sys.argv[1] if len(sys.argv) > 1 else '.'
    base_dir, files = collect_files(folder_arg)

    if not files:
        print(f"{C['yellow']}Warning:{C['reset']} no files found in: {base_dir}",
              file=sys.stderr)
        sys.exit(1)

    all_classes: set = set()
    for f in sorted(files):
        all_classes.update(extract_classes_from_file(f))

    # Remove namespace names — they appear via ClassName:: patterns but are not classes
    ns_names = collect_namespace_names(files)
    all_classes -= ns_names

    if not all_classes:
        print(f"  {C['gray']}(no classes found){C['reset']}")
        return

    sorted_names = sorted(all_classes)
    n_files = len(files)
    n_classes = len(sorted_names)

    print(f"{C['bold']}{C['cyan']}Classes found{C['reset']} "
          f"{C['gray']}({n_classes} unique · {n_files} file{'s' if n_files != 1 else ''} searched):{C['reset']}")
    print()
    print(', '.join(sorted_names))


main()
PYEOF
