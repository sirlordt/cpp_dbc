#!/bin/bash
# test_coverage.sh
# Analyzes which public C++ class methods are exercised by tests.
# Delegates class discovery, method listing, and usage search to
# list_class.sh, list_public_methods.sh, and list_class_usage.sh.
# Processes multiple classes in parallel.
#
# Usage: ./test_coverage.sh --src=PATHS --test=PATH [OPTIONS]
#        ./test_coverage.sh --check=PRESET [OPTIONS]
#
# Options:
#   --src=PATHS          Comma-separated source dirs/globs for class discovery
#   --test=PATH          Test directory or glob pattern to search
#   --dir=PROJECT_DIR    Project root directory (default: script directory)
#   --min-coverage=N     Only show classes with coverage below N% (default: 0 = all)
#   --class=PATTERNS     Comma-separated class names or glob patterns (quote globs!)
#   --workers=N          Number of parallel workers (default: 8)
#   --check=PRESET       Shortcut; expands to a preset combination of the above.
#                        Explicit options after --check= override the preset.
#                        Presets:
#                          db-driver  drivers coverage (all driver connection classes)
#                          db-pool    pool coverage (*DBConnectionPool, *PooledDBConnection)
#
# Examples:
#   ./test_coverage.sh --src=libs/cpp_dbc/src,libs/cpp_dbc/include \
#                      --test=libs/cpp_dbc/test
#   ./test_coverage.sh --src=libs/cpp_dbc/include \
#                      --test="libs/cpp_dbc/test/*.cpp" --min-coverage=80
#   ./test_coverage.sh --check=db-driver
#   ./test_coverage.sh --check=db-pool
#   ./test_coverage.sh --check=db-pool --min-coverage=80

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
SRC_ARG=""
TEST_ARG=""
PROJECT_DIR="$SCRIPT_DIR"
MIN_COVERAGE="0"
CLASS_FILTER=""
WORKERS="8"

usage() {
    echo "Usage: $0 --src=PATHS --test=PATH [OPTIONS]"
    echo "       $0 --check=PRESET [OPTIONS]"
    echo ""
    echo "Analyzes test coverage per C++ class."
    echo ""
    echo "Options:"
    echo "  --src=PATHS          Source paths for class discovery (comma-separated dirs/globs)"
    echo "  --test=PATH          Test directory or glob pattern"
    echo "  --dir=PROJECT_DIR    Project root directory (default: script directory)"
    echo "  --min-coverage=N     Only show classes with coverage below N% (default: 0 = all)"
    echo "  --class=PATTERNS     Comma-separated class names or glob patterns (quote globs!)"
    echo "  --workers=N          Parallel workers (default: 8)"
    echo "  --check=PRESET       Preset shortcut (explicit options override the preset):"
    echo "                         db-driver  → drivers coverage (all driver connection classes)"
    echo "                         db-pool    → pool coverage (*DBConnectionPool, *PooledDBConnection)"
    echo ""
    echo "Examples:"
    echo "  $0 --src=libs/cpp_dbc/src,libs/cpp_dbc/include --test=libs/cpp_dbc/test"
    echo "  $0 --src=libs/cpp_dbc/include --test=\"libs/cpp_dbc/test/*.cpp\" --min-coverage=80"
    echo "  $0 --check=db-driver"
    echo "  $0 --check=db-pool"
    echo "  $0 --check=db-pool --min-coverage=80"
}

# ── Preset shortcuts ──────────────────────────────────────────────────────────
apply_preset() {
    case "$1" in
        db-driver)
            SRC_ARG="libs/cpp_dbc/include/cpp_dbc/drivers"
            TEST_ARG="libs/cpp_dbc/test"
            WORKERS="4"
            ;;
        db-pool)
            SRC_ARG="libs/cpp_dbc/include/cpp_dbc/core"
            TEST_ARG="libs/cpp_dbc/test"
            CLASS_FILTER="*DBConnectionPool,*PooledDBConnection"
            WORKERS="4"
            ;;
        *)
            echo "Error: Unknown --check preset: '$1'" >&2
            echo "Available presets: db-driver, db-pool" >&2
            exit 1
            ;;
    esac
}

# Pass 1: apply preset defaults (must run before explicit args)
for arg in "$@"; do
    case "$arg" in
        --check=*)  apply_preset "${arg#--check=}" ;;
        -h|--help)  usage; exit 0 ;;
    esac
done

# Pass 2: explicit args override preset values
for arg in "$@"; do
    case "$arg" in
        --src=*)           SRC_ARG="${arg#--src=}" ;;
        --test=*)          TEST_ARG="${arg#--test=}" ;;
        --dir=*)           PROJECT_DIR="${arg#--dir=}" ;;
        --min-coverage=*)  MIN_COVERAGE="${arg#--min-coverage=}" ;;
        --class=*)         CLASS_FILTER="${arg#--class=}" ;;
        --workers=*)       WORKERS="${arg#--workers=}" ;;
        --check=*)         ;;  # already handled in pass 1
        -h|--help)         ;;  # already handled in pass 1
        *)
            echo "Error: Unknown argument: $arg" >&2
            usage >&2
            exit 1
            ;;
    esac
done

if [[ -z "$SRC_ARG" ]]; then
    echo "Error: --src=PATHS is required" >&2
    usage >&2
    exit 1
fi

if [[ -z "$TEST_ARG" ]]; then
    echo "Error: --test=PATH is required" >&2
    usage >&2
    exit 1
fi

if [[ ! -d "$PROJECT_DIR" ]]; then
    echo "Error: Project directory not found: $PROJECT_DIR" >&2
    exit 1
fi

# ── Log file setup ────────────────────────────────────────────────────────────
TIMESTAMP=$(date +"%Y-%m-%d-%H-%M-%S")
LOG_DIR="$PROJECT_DIR/logs/test/coverage/$TIMESTAMP"
LOG_FILE="$LOG_DIR/test_coverage.log"
mkdir -p "$LOG_DIR"

python3 - "$SRC_ARG" "$TEST_ARG" "$PROJECT_DIR" "$SCRIPT_DIR" \
          "$MIN_COVERAGE" "$CLASS_FILTER" "$WORKERS" "$LOG_FILE" << 'PYEOF'
"""Parallel C++ test coverage analyzer (embedded).

For each class discovered in --src paths, determines which of its public
methods appear in --test files, then reports per-class coverage.

Delegates to:
  list_class.sh           → class discovery
  list_public_methods.sh  → public interface enumeration
  list_class_usage.sh     → call-site detection in test files
"""

import sys
import re
import fnmatch
import subprocess
import concurrent.futures
from pathlib import Path

# ── ANSI colours ──────────────────────────────────────────────────────────────

def _colours(enabled):
    if enabled:
        return dict(bold="\033[1m", cyan="\033[36m", green="\033[32m",
                    yellow="\033[33m", gray="\033[90m", red="\033[31m",
                    reset="\033[0m")
    return {k: "" for k in ("bold", "cyan", "green", "yellow", "gray", "red", "reset")}


C = _colours(sys.stdout.isatty())

_ANSI_RE = re.compile(r'\033\[[0-9;]*m')

def strip_ansi(text):
    return _ANSI_RE.sub('', text)


# ── Output parsers ────────────────────────────────────────────────────────────

def parse_class_list(text):
    """
    Extract class names from list_class.sh stdout.
    Output format (no TTY): header line, blank line, "Name1, Name2, ..."
    """
    classes = set()
    for line in strip_ansi(text).splitlines():
        stripped = line.strip()
        if not stripped:
            continue
        if stripped.startswith(('Classes found', 'Warning:', 'Error:')):
            continue
        # Data line: comma-separated valid C++ identifiers
        for token in stripped.split(','):
            name = token.strip()
            if re.match(r'^[A-Za-z_][A-Za-z0-9_]{2,}$', name):
                classes.add(name)
    return sorted(classes)


def parse_structured_methods(text):
    """
    Extract public method names from list_public_methods.sh structured output.

    Mirrors parse_method_names() in list_class_usage.sh:
      - only reads Name: entries from the 'header:' section
      - stops at 'implementation:' (which duplicates 'header:')
      - skips 'ancestors:' (those Name: entries are base-class names, not methods)

    Returns (full_class_name, [method_names]).
    """
    lines = [strip_ansi(l) for l in text.splitlines()]
    full_class_name = ''
    method_names: list = []
    seen: set = set()
    in_header = False

    for line in lines:
        stripped = line.strip()

        # First non-indented, non-empty line is the fully-qualified class name
        if not full_class_name and stripped and not line.startswith(' '):
            full_class_name = stripped
            continue

        if stripped == 'header:':
            in_header = True
            continue

        if stripped in ('implementation:', 'ancestors:'):
            if in_header and stripped == 'implementation:':
                break           # header section complete; stop here
            in_header = False
            continue

        if in_header:
            m = re.match(r'\s+Name:\s+(\S+)', line)
            if m:
                name = m.group(1)
                if name not in seen:
                    seen.add(name)
                    method_names.append(name)

    return full_class_name, method_names


def parse_usage_methods(text):
    """
    Extract method names that appear in list_class_usage.sh output.
    Matches '  Method:    methodName' lines (after ANSI stripping).
    """
    used: set = set()
    for line in strip_ansi(text).splitlines():
        m = re.match(r'\s+Method:\s+(\S+)', line)
        if m:
            used.add(m.group(1))
    return used


# ── Subprocess runner ─────────────────────────────────────────────────────────

def run_script(args, project_dir):
    """Run a shell script; return (stdout, stderr, returncode)."""
    try:
        result = subprocess.run(
            args, capture_output=True, text=True,
            cwd=str(project_dir), timeout=60
        )
        return result.stdout, result.stderr, result.returncode
    except subprocess.TimeoutExpired:
        return '', 'timeout after 60 s', -1
    except Exception as exc:
        return '', str(exc), -1


# ── Per-script wrappers ───────────────────────────────────────────────────────

def get_classes_from_path(src_path, script_dir, project_dir):
    """Return sorted list of C++ class names found in src_path."""
    stdout, _stderr, _rc = run_script(
        [f"{script_dir}/list_class.sh", f"--folder={src_path}"],
        project_dir
    )
    return parse_class_list(stdout)


def get_methods_for_class(class_name, script_dir, project_dir):
    """
    Return (full_class_name, [public_method_names]) for class_name.
    Returns ('', []) if the class cannot be found or has no public methods.
    """
    stdout, _stderr, rc = run_script(
        [f"{script_dir}/list_public_methods.sh",
         f"--class={class_name}",
         f"--dir={project_dir}",
         "--output-format=structured"],
        project_dir
    )
    if rc != 0:
        return '', []
    return parse_structured_methods(stdout)


def get_test_usages(class_name, test_arg, script_dir, project_dir):
    """
    Return the set of method names called inside test_arg files.
    Returns an empty set on error (treated as 0% coverage for that class).
    """
    stdout, _stderr, _rc = run_script(
        [f"{script_dir}/list_class_usage.sh",
         f"--class={class_name}",
         f"--file={test_arg}",
         f"--dir={project_dir}"],
        project_dir
    )
    return parse_usage_methods(stdout)


# ── Per-class processing ───────────────────────────────────────────────────────

def process_class(task):
    """
    Compute coverage for a single class.
    Returns a result dict, or None if the class has no discoverable public methods.
    """
    class_name, test_arg, script_dir, project_dir = task

    full_name, all_methods = get_methods_for_class(class_name, script_dir, project_dir)
    if not all_methods:
        return None     # not in headers or no public interface — skip silently

    # Destructors are excluded: they are implicitly exercised via RAII
    all_methods = [m for m in all_methods if not m.startswith('~')]
    if not all_methods:
        return None

    used_methods = get_test_usages(class_name, test_arg, script_dir, project_dir)

    covered   = [m for m in all_methods if m in used_methods]
    uncovered = [m for m in all_methods if m not in used_methods]
    pct       = len(covered) / len(all_methods) * 100

    return {
        'class_name': full_name or class_name,
        'short_name': class_name,
        'total':      len(all_methods),
        'covered':    covered,
        'uncovered':  uncovered,
        'pct':        pct,
    }


# ── Display ───────────────────────────────────────────────────────────────────

_BAR_W = 20
_SEP   = C['gray'] + '═' * 70 + C['reset']


def cov_colour(pct):
    if pct >= 80:
        return C['green']
    if pct >= 50:
        return C['yellow']
    return C['red']


def progress_bar(pct):
    filled = round(pct / 100 * _BAR_W)
    return '[' + '█' * filled + '░' * (_BAR_W - filled) + ']'


def build_report(all_results, min_pct):
    """Build the coverage report as a list of strings (ANSI included)."""
    lines: list = []
    out = lines.append

    results = [r for r in all_results if r is not None]

    if not results:
        out(f"  {C['yellow']}No classes with a discoverable public interface were found.{C['reset']}")
        return lines

    total_methods = sum(r['total'] for r in results)
    total_covered = sum(len(r['covered']) for r in results)
    avg_pct       = total_covered / total_methods * 100 if total_methods else 0.0

    # Which classes to show in the detailed section
    shown = results if min_pct == 0 else [r for r in results if r['pct'] < min_pct]
    shown.sort(key=lambda r: (r['pct'], r['short_name']))

    # ── Report header ──────────────────────────────────────────────────────
    cc = cov_colour(avg_pct)
    out(f"{C['bold']}{C['cyan']}Test Coverage Report{C['reset']}")
    out(f"  {C['gray']}{len(results)} class{'es' if len(results) != 1 else ''} · "
        f"{total_methods} public method{'s' if total_methods != 1 else ''} · "
        f"average {cc}{avg_pct:.0f}%{C['reset']}")
    if min_pct > 0:
        out(f"  {C['gray']}Showing {len(shown)} class{'es' if len(shown) != 1 else ''} "
            f"below {min_pct:.0f}% threshold{C['reset']}")
    out('')

    if not shown:
        out(f"  {C['green']}All classes meet the {min_pct:.0f}% coverage threshold.{C['reset']}")
        out('')
    else:
        for r in shown:
            pct = r['pct']
            cc  = cov_colour(pct)
            out(_SEP)
            out(f"  {C['bold']}{r['short_name']}{C['reset']}")
            out(f"  {cc}{progress_bar(pct)} {pct:5.1f}%{C['reset']}  "
                f"{C['gray']}{len(r['covered'])}/{r['total']} covered{C['reset']}")
            out('')

            if r['covered']:
                out(f"  {C['green']}Covered   ({len(r['covered'])}):{C['reset']}")
                for m in r['covered']:
                    out(f"    {C['gray']}·{C['reset']} {m}")
            if r['uncovered']:
                out(f"  {C['red']}Uncovered ({len(r['uncovered'])}):{C['reset']}")
                for m in r['uncovered']:
                    out(f"    {C['gray']}·{C['reset']} {m}")
            out('')

        out(_SEP)
        out('')

    # ── Summary table ──────────────────────────────────────────────────────
    n_full    = sum(1 for r in results if r['pct'] == 100.0)
    n_partial = sum(1 for r in results if 0.0 < r['pct'] < 100.0)
    n_zero    = sum(1 for r in results if r['pct'] == 0.0)
    cc        = cov_colour(avg_pct)

    out(f"{C['bold']}Summary{C['reset']}")
    out(f"  Total classes:     {len(results)}")
    out(f"  Total methods:     {total_methods}")
    out(f"  {C['green']}Covered:{C['reset']}           {total_covered}  ({cc}{avg_pct:.0f}%{C['reset']})")
    out(f"  {C['red']}Uncovered:{C['reset']}         {total_methods - total_covered}")
    if n_full:
        out(f"  {C['green']}Full coverage:{C['reset']}     {n_full} class{'es' if n_full != 1 else ''}")
    if n_partial:
        out(f"  {C['yellow']}Partial coverage:{C['reset']}  {n_partial} class{'es' if n_partial != 1 else ''}")
    if n_zero:
        out(f"  {C['red']}No coverage:{C['reset']}       {n_zero} class{'es' if n_zero != 1 else ''}")
    out('')

    return lines


# ── Entry point ───────────────────────────────────────────────────────────────

def main():
    if len(sys.argv) < 9:
        print("Usage: script src_arg test_arg project_dir script_dir "
              "min_coverage class_filter workers log_file", file=sys.stderr)
        sys.exit(1)

    src_arg      = sys.argv[1]          # comma-separated source paths
    test_arg     = sys.argv[2]          # test path or glob
    project_dir  = Path(sys.argv[3]).resolve()
    script_dir   = sys.argv[4]          # absolute path to directory with *.sh scripts
    class_filter = sys.argv[6]          # comma-separated class names or ""
    log_file     = Path(sys.argv[8])    # destination for the clean log

    try:
        min_coverage = float(sys.argv[5])
    except ValueError:
        print(f"Error: --min-coverage must be a number, got: {sys.argv[5]!r}",
              file=sys.stderr)
        sys.exit(1)

    try:
        workers = int(sys.argv[7])
        if workers < 1:
            raise ValueError("must be >= 1")
    except ValueError as exc:
        print(f"Error: --workers must be a positive integer: {exc}", file=sys.stderr)
        sys.exit(1)

    # ── Collect classes ────────────────────────────────────────────────────
    src_paths = [p.strip() for p in src_arg.split(',') if p.strip()]
    print(f"{C['gray']}Scanning {len(src_paths)} source path(s)...{C['reset']}",
          file=sys.stderr)

    all_classes: set = set()
    for src in src_paths:
        all_classes.update(get_classes_from_path(src, script_dir, project_dir))

    if class_filter:
        patterns = [p.strip() for p in class_filter.split(',') if p.strip()]
        all_classes = {cls for cls in all_classes
                       if any(fnmatch.fnmatch(cls, pat) for pat in patterns)}

    if not all_classes:
        if class_filter:
            msg = f"no classes matched: {class_filter}"
        else:
            msg = f"no classes found in: {src_arg}"
        print(f"{C['yellow']}Warning:{C['reset']} {msg}", file=sys.stderr)
        sys.exit(1)

    sorted_classes = sorted(all_classes)
    print(f"{C['gray']}Processing {len(sorted_classes)} class(es) "
          f"with {workers} parallel worker(s)...{C['reset']}",
          file=sys.stderr)

    # ── Process in parallel ────────────────────────────────────────────────
    tasks = [(cls, test_arg, script_dir, project_dir) for cls in sorted_classes]
    all_results: list = []

    with concurrent.futures.ThreadPoolExecutor(max_workers=workers) as executor:
        future_map = {executor.submit(process_class, t): t[0] for t in tasks}
        for future in concurrent.futures.as_completed(future_map):
            cls = future_map[future]
            try:
                all_results.append(future.result())
            except Exception as exc:
                print(f"{C['yellow']}Warning:{C['reset']} {cls}: {exc}",
                      file=sys.stderr)

    # ── Display + log ──────────────────────────────────────────────────────
    report_lines = build_report(all_results, min_coverage)

    # Print to stdout (with ANSI colours when connected to a terminal)
    for line in report_lines:
        print(line)

    # Write clean copy (no ANSI) to log file
    log_file.parent.mkdir(parents=True, exist_ok=True)
    with open(log_file, 'w') as lf:
        for line in report_lines:
            lf.write(strip_ansi(line) + '\n')
    print(f"{C['gray']}Log: {log_file}{C['reset']}", file=sys.stderr)


main()
PYEOF
