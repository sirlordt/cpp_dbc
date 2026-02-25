#!/bin/bash
# list_public_methods.sh
# Outlines the public interface of a C++ class: methods with header and implementation locations.
#
# Usage: ./list_public_methods.sh --class=ClassName [--dir=PROJECT_DIR] [--output-format=lineal|structured]
#
# Example:
#   ./list_public_methods.sh --class=RelationalDBConnectionPool
#   ./list_public_methods.sh --class=MySQLDBConnection --output-format=structured

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
CLASS_NAME=""
PROJECT_DIR="$SCRIPT_DIR"
OUTPUT_FORMAT="structured"

usage() {
    echo "Usage: $0 --class=ClassName [--dir=PROJECT_DIR] [--output-format=FORMAT]"
    echo ""
    echo "Lists public methods of a C++ class with header and implementation locations."
    echo ""
    echo "Options:"
    echo "  --class=ClassName              Name of the C++ class to outline (required)"
    echo "  --dir=PROJECT_DIR              Project root directory (default: script directory)"
    echo "  --output-format=structured     Name / Return / Params / Location breakdown  (default)"
    echo "  --output-format=lineal         One-line signature + => file:line"
    echo ""
    echo "Examples:"
    echo "  $0 --class=RelationalDBConnectionPool"
    echo "  $0 --class=MySQLDBConnection --output-format=structured"
    echo "  $0 --class=SQLiteDBConnection --dir=/path/to/project"
}

for arg in "$@"; do
    case "$arg" in
        --class=*)         CLASS_NAME="${arg#--class=}" ;;
        --dir=*)           PROJECT_DIR="${arg#--dir=}" ;;
        --output-format=*) OUTPUT_FORMAT="${arg#--output-format=}" ;;
        -h|--help)         usage; exit 0 ;;
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

if [[ "$OUTPUT_FORMAT" != "lineal" && "$OUTPUT_FORMAT" != "structured" ]]; then
    echo "Error: --output-format must be 'lineal' or 'structured'" >&2
    exit 1
fi

if [[ ! -d "$PROJECT_DIR" ]]; then
    echo "Error: Project directory not found: $PROJECT_DIR" >&2
    exit 1
fi

python3 - "$CLASS_NAME" "$PROJECT_DIR" "$OUTPUT_FORMAT" << 'PYEOF'
"""C++ class public method outliner.

Finds a C++ class by name across header files, extracts its public methods,
and maps each method to its header declaration and .cpp implementation.
"""

import sys
import re
from pathlib import Path
from typing import Optional

# ── ANSI colours ─────────────────────────────────────────────────────────────

def _colours(enabled: bool):
    if enabled:
        return dict(bold="\033[1m", cyan="\033[36m", green="\033[32m",
                    yellow="\033[33m", gray="\033[90m", red="\033[31m",
                    reset="\033[0m")
    return {k: "" for k in ("bold","cyan","green","yellow","gray","red","reset")}

C = _colours(sys.stdout.isatty())

# ── Directories to skip ───────────────────────────────────────────────────────

SKIP_DIRS = {"build", "cmake-build-debug", "cmake-build-release",
             ".git", "_build", "CMakeFiles"}

# ── Low-level string helpers ──────────────────────────────────────────────────

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


def count_braces(line: str) -> int:
    """Net { minus } in a line, ignoring string literals and // comments."""
    count = 0
    in_string = False
    i = 0
    while i < len(line):
        c = line[i]
        if c == '\\' and in_string:
            i += 2
            continue
        if c == '"':
            in_string = not in_string
        elif not in_string:
            if c == '/' and i + 1 < len(line) and line[i + 1] == '/':
                break
            elif c == '{':
                count += 1
            elif c == '}':
                count -= 1
        i += 1
    return count


# ── Namespace extraction ──────────────────────────────────────────────────────

# Matches: namespace A  or  namespace A::B::C  (with optional trailing {)
_NS_RE = re.compile(r'^namespace\s+([\w:]+)\s*(?:\{|$)')

def extract_namespace(lines: list, class_line_idx: int) -> str:
    """
    Return the fully-qualified namespace containing the class at class_line_idx.

    Handles both old-style  namespace A { namespace B { ... } }
    and C++17-style          namespace A::B { ... }
    """
    # Each entry: (name, awaited_depth)
    # A namespace A::B pushed at brace_depth d means it is active when depth >= d+1.
    # We store awaited_depth = brace_depth_at_declaration + 1.
    namespaces: list = []   # (name, awaited_depth) in declaration order
    brace_depth = 0

    for i in range(class_line_idx):
        stripped = strip_line_comment(lines[i]).strip()
        if not stripped:
            continue

        m = _NS_RE.match(stripped)
        if m:
            ns_name = m.group(1)
            # All parts of a C++17 A::B::C share the same brace block → same awaited_depth
            awaited = brace_depth + 1
            for part in ns_name.split('::'):
                namespaces.append((part, awaited))

        brace_depth += count_braces(stripped)

    # Active namespaces at class_line_idx: those whose awaited_depth <= current brace_depth
    active = [(name, d) for name, d in namespaces if d <= brace_depth]
    # Sort by depth (declaration order for same depth is already preserved by list order)
    active.sort(key=lambda x: x[1])
    return '::'.join(name for name, _ in active)


# ── Method declaration detection ─────────────────────────────────────────────

_ACCESS_RE = re.compile(r'^(public|private|protected)\s*:')
_SKIP_STARTS = ('*', '/*', '//', '#', 'friend class', 'friend struct',
                'typedef', 'using ', 'static_assert')


def is_method_declaration(line: str) -> bool:
    """True if *line* looks like a method / constructor / destructor declaration."""
    stripped = strip_line_comment(line).strip()
    if not stripped:
        return False
    if any(stripped.startswith(s) for s in _SKIP_STARTS):
        return False
    if '(' not in stripped:
        return False
    # Skip access-specifier labels
    if _ACCESS_RE.match(stripped):
        return False
    # Skip nested class / struct definitions starting a body
    if re.match(r'^(struct|class|enum)\s+\w', stripped) and '{' in stripped:
        return False
    return True


# ── Signature collection ──────────────────────────────────────────────────────

def collect_method_signature(lines: list, start_idx: int):
    """
    Collect a (possibly multi-line) method signature starting at start_idx.

    Stops when parentheses are balanced (handles nested parens in template args
    and default values).  Returns (sig_lines, next_idx).
    """
    sig_lines = []
    i = start_idx
    paren_depth = 0
    seen_open = False

    while i < len(lines) and len(sig_lines) < 30:
        raw_line = lines[i]
        sig_lines.append(raw_line.rstrip())
        stripped = strip_line_comment(raw_line)
        i += 1

        for c in stripped:
            if c == '(':
                paren_depth += 1
                seen_open = True
            elif c == ')':
                paren_depth -= 1

        if seen_open and paren_depth <= 0:
            break

    return sig_lines, i


def has_inline_body(sig_lines: list) -> bool:
    """True if the signature already contains the method body (inline definition)."""
    # Check for { after the closing ) — could be on same or next collected line.
    full = ' '.join(strip_line_comment(l) for l in sig_lines)
    last_paren = full.rfind(')')
    if last_paren < 0:
        return False
    after = full[last_paren + 1:]
    return '{' in after


def is_deleted_or_defaulted(sig_lines: list) -> bool:
    full = ' '.join(strip_line_comment(l) for l in sig_lines)
    last_paren = full.rfind(')')
    after = full[last_paren + 1:] if last_paren >= 0 else full
    return bool(re.search(r'=\s*(delete|default)\s*;', after))


# ── Method name extraction ────────────────────────────────────────────────────

def get_method_name(sig_lines: list) -> str:
    """Extract the bare method name from a signature."""
    first = strip_line_comment(sig_lines[0]).strip()
    if '(' not in first:
        return first.split()[-1] if first.split() else first

    before_paren = first[:first.index('(')]

    # Operator overload
    op_m = re.search(r'operator\s*(\S+)', before_paren)
    if op_m:
        return 'operator' + op_m.group(1).strip()

    # Destructor
    dest_m = re.search(r'~\w+', before_paren)
    if dest_m:
        return dest_m.group(0)

    # Regular: last word before (
    words = re.findall(r'[\w]+', before_paren)
    return words[-1] if words else before_paren.strip()


# ── Public method extraction ──────────────────────────────────────────────────

def extract_public_methods(lines: list, class_start_idx: int) -> list:
    """
    Return [(header_line_1indexed, sig_lines), ...] for every public method
    in the class whose definition starts at class_start_idx (0-indexed).
    """
    # Find the opening { of the class body
    open_brace_idx = -1
    for j in range(class_start_idx, min(class_start_idx + 10, len(lines))):
        if '{' in strip_line_comment(lines[j]):
            open_brace_idx = j
            break
    if open_brace_idx < 0:
        return []

    methods = []
    depth = 0          # Brace depth relative to before the class {
    in_public = False
    i = open_brace_idx

    while i < len(lines):
        line = lines[i]
        stripped = strip_line_comment(line).strip()

        # ── first line: the one that opens the class body ──
        if i == open_brace_idx:
            depth += count_braces(stripped)   # becomes 1
            i += 1
            continue

        # ── check if we've left the class ──
        if depth <= 0:
            break

        # ── access specifiers (only at class top level) ──
        if depth == 1 and _ACCESS_RE.match(stripped):
            in_public = stripped.startswith('public')
            i += 1
            continue

        # ── collect public method declarations at top level ──
        if in_public and depth == 1 and is_method_declaration(line):
            sig_lines, next_i = collect_method_signature(lines, i)
            methods.append((i + 1, sig_lines))   # 1-indexed line number

            # Account for braces inside the signature (inline bodies)
            for sl in sig_lines:
                depth += count_braces(strip_line_comment(sl))

            i = next_i
            continue

        # ── default: advance depth ──
        depth += count_braces(stripped)
        i += 1

    return methods


# ── Implementation search ─────────────────────────────────────────────────────

def find_implementation(source_files: list, class_name: str, method_name: str,
                        skip_before: dict = None):
    """
    Search .cpp files for  ClassName::methodName(  or  ClassName::methodName<
    Returns (source_path, line_num) or (None, None).

    skip_before: {str(src_file): min_line} — skips matches on lines already
    returned for a previous overload of the same name in the same file.
    """
    escaped_class  = re.escape(class_name)
    escaped_method = re.escape(method_name)
    pattern = re.compile(
        r'\b' + escaped_class + r'\s*::\s*' + escaped_method + r'\s*[\(\<]'
    )
    for src_file in source_files:
        min_line = (skip_before or {}).get(str(src_file), 0)
        try:
            with open(src_file, 'r', errors='replace') as fh:
                for line_num, line in enumerate(fh, 1):
                    if line_num <= min_line:
                        continue
                    if pattern.search(line):
                        return src_file, line_num
        except OSError:
            pass
    return None, None


# ── Class detection in a single file ─────────────────────────────────────────

_CLASS_DECL_RE_TMPL = r'(?:template\s*<[^>]*>\s*)?(?:class|struct)\s+{name}\b'

def find_class_in_file(filepath: Path, class_name: str, project_dir: Path) -> list:
    """
    Return list of dicts describing each definition of *class_name* in *filepath*.
    Skips forward declarations and friend declarations.
    """
    try:
        with open(filepath, 'r', errors='replace') as fh:
            lines = fh.read().splitlines()
    except OSError:
        return []

    pattern = re.compile(_CLASS_DECL_RE_TMPL.format(name=re.escape(class_name)))
    results = []

    for idx, raw_line in enumerate(lines):
        line = strip_line_comment(raw_line).strip()
        if not pattern.search(line):
            continue

        # Skip friend declarations
        if 'friend' in line:
            continue

        # Skip forward declarations: end with ; and no {
        if line.endswith(';') and '{' not in line:
            continue

        # Confirm a class body exists within the next 8 lines
        has_body = any('{' in strip_line_comment(lines[k])
                       for k in range(idx, min(idx + 8, len(lines))))
        if not has_body:
            continue

        try:
            rel = str(filepath.relative_to(project_dir))
        except ValueError:
            rel = str(filepath)

        ns = extract_namespace(lines, idx)
        results.append({
            "class_name":  class_name,
            "namespace":   ns,
            "header_file": rel,
            "header_line": idx + 1,
            "lines":       lines,
        })

    return results


# ── Signature parsing (for structured output) ────────────────────────────────

def split_params(s: str) -> list:
    """Split a parameter string by comma at nesting depth 0 (respects <>, (), [])."""
    params, current, depth = [], [], 0
    for c in s:
        if c in '<([{':
            depth += 1
            current.append(c)
        elif c in '>)]}':
            depth -= 1
            current.append(c)
        elif c == ',' and depth == 0:
            p = ''.join(current).strip()
            if p:
                params.append(p)
            current = []
        else:
            current.append(c)
    last = ''.join(current).strip()
    if last:
        params.append(last)
    return params


def _classify_kind(return_type: str, name: str, class_name: str) -> str:
    """
    Classify a method into one of: constructor, destructor, factory, method.

    factory: static method whose return type contains the class name inside a
             pointer/smart-pointer (shared_ptr, unique_ptr, raw *).
    """
    if name == class_name:
        return 'constructor'
    if name.startswith('~'):
        return 'destructor'
    if (re.search(r'\bstatic\b', return_type) and
            re.search(r'\b' + re.escape(class_name) + r'\b', return_type)):
        return 'factory'
    return 'method'


def parse_signature(sig_lines: list, class_name: str) -> dict:
    """
    Decompose a method signature into:
      return_type, name, kind, params (list), trailing, is_constructor, is_destructor
    """
    full = re.sub(r'\s+', ' ', ' '.join(strip_line_comment(l) for l in sig_lines)).strip()
    name = get_method_name(sig_lines)

    # Locate the opening ( of the param list by finding name + (
    m = re.search(re.escape(name) + r'\s*\(', full)
    if not m:
        return dict(return_type='', name=name, kind='method', params=[], trailing='',
                    is_constructor=False, is_destructor=False)

    param_start = full.index('(', m.start())

    # Everything before the method name is the return-type area
    return_type = full[:m.start()].strip()

    is_constructor = (name == class_name)
    is_destructor  = name.startswith('~')

    # Constructors/destructors have no return type; strip 'explicit'
    if is_constructor or is_destructor:
        return_type = re.sub(r'\bexplicit\b\s*', '', return_type).strip()

    kind = _classify_kind(return_type, name, class_name)

    # Find the matching closing )
    paren_depth, param_end = 0, param_start
    for i in range(param_start, len(full)):
        if full[i] == '(':
            paren_depth += 1
        elif full[i] == ')':
            paren_depth -= 1
            if paren_depth == 0:
                param_end = i
                break

    params_str = full[param_start + 1:param_end].strip()
    trailing   = full[param_end + 1:].strip().rstrip(';').strip()

    return dict(
        return_type    = return_type,
        name           = name,
        kind           = kind,
        params         = split_params(params_str) if params_str else [],
        trailing       = trailing,
        is_constructor = is_constructor,
        is_destructor  = is_destructor,
    )


# ── Signature formatting ──────────────────────────────────────────────────────

def format_sig(sig_lines: list, base_indent: str = "    ") -> str:
    """
    Format a (possibly multi-line) signature for terminal display.
    Aligns continuation lines after the opening '('.
    """
    if len(sig_lines) == 1:
        return sig_lines[0].strip()

    first = sig_lines[0].strip()
    paren_pos = first.find('(')
    align = ' ' * (paren_pos + 1) if paren_pos >= 0 else '  '

    out = [first]
    for line in sig_lines[1:]:
        out.append(align + line.strip())
    return '\n'.join(out)


# ── Structured method renderer ────────────────────────────────────────────────

_SEP60    = C['gray'] + '─' * 60 + C['reset']   # between individual methods
_SECT_SEP = C['bold'] + '═' * 60 + C['reset']   # below header: / implementation:
_LBL_W = 10   # "Location:" = 9 chars + 1 space padding → values start at col 12


def _lbl(name: str) -> str:
    """Left-justified label padded to _LBL_W, with 2-space base indent."""
    return f"  {(name + ':'):<{_LBL_W}} "


_KIND_COLOUR = {
    'constructor': C['cyan'],
    'destructor':  C['cyan'],
    'factory':     C['yellow'],
    'method':      C['reset'],
}


def print_method_structured(parsed: dict, location: str, first: bool = False):
    """Print one method in structured (Name/Kind/Return/Params/Location) format."""
    if not first:
        print(_SEP60)

    # Name
    print(f"{_lbl('Name')}{C['bold']}{parsed['name']}{C['reset']}")

    # Kind
    kind = parsed.get('kind', 'method')
    kcolour = _KIND_COLOUR.get(kind, C['reset'])
    print(f"{_lbl('Kind')}{kcolour}{kind}{C['reset']}")

    # Return type
    if parsed['is_destructor']:
        print(f"{_lbl('Return')}void")
    elif parsed['is_constructor']:
        print(f"{_lbl('Return')}{C['gray']}(none){C['reset']}")
    elif parsed['return_type']:
        print(f"{_lbl('Return')}{parsed['return_type']}")

    # Params
    prefix = _lbl('Params')
    cont   = ' ' * len(prefix)
    params = parsed['params']
    if params:
        for i, param in enumerate(params):
            comma = ',' if i < len(params) - 1 else ''
            line  = prefix if i == 0 else cont
            print(f"{line}{param}{comma}")
    else:
        print(f"{prefix}{C['gray']}(none){C['reset']}")

    # Trailing qualifiers (const, override, noexcept, = 0, = delete, ...)
    if parsed['trailing']:
        print(f"{_lbl('Trailing')}{C['gray']}{parsed['trailing']}{C['reset']}")

    # Location
    print(f"{_lbl('Location')}{location}")
    print()


# ── Ancestor extraction ───────────────────────────────────────────────────────

def find_inheritance_colon(s: str) -> int:
    """
    Find the index of the ':' that begins the inheritance list.
    Skips '::' (scope resolution) and ':' inside template '<>' arguments.
    """
    depth = 0   # template < > nesting level
    i = 0
    while i < len(s):
        c = s[i]
        if c == '<':
            depth += 1
        elif c == '>':
            depth -= 1
        elif c == ':' and depth == 0:
            prev = s[i - 1] if i > 0 else ''
            nxt  = s[i + 1] if i + 1 < len(s) else ''
            if prev != ':' and nxt != ':':
                return i
        i += 1
    return -1


def extract_ancestors(lines: list, class_start_idx: int) -> list:
    """
    Return list of {'name': str, 'access': str} for each base class
    in the declaration starting at class_start_idx (0-indexed).
    """
    decl_parts: list[str] = []
    for i in range(class_start_idx, min(class_start_idx + 20, len(lines))):
        stripped = strip_line_comment(lines[i]).strip()
        if '{' in stripped:
            decl_parts.append(stripped[:stripped.index('{')])
            break
        decl_parts.append(stripped)

    full_decl = ' '.join(decl_parts)
    colon_pos = find_inheritance_colon(full_decl)
    if colon_pos < 0:
        return []

    inheritance_part = full_decl[colon_pos + 1:].strip()
    parts = split_params(inheritance_part)

    ancestors = []
    for part in parts:
        part = part.strip().rstrip(';{ ').strip()
        if not part:
            continue
        m = re.match(r'^(?:virtual\s+)?(public|protected|private)\s+(.*)', part)
        if m:
            access    = m.group(1)
            base_name = m.group(2).strip()
        else:
            access    = 'public'
            base_name = re.sub(r'\bvirtual\b\s*', '', part).strip()
        if base_name:
            ancestors.append({'name': base_name, 'access': access})
    return ancestors


def find_ancestor_location(base_name: str, header_files: list,
                           project_dir: Path) -> str:
    """
    Return 'relpath:line' if the base class is found in project headers,
    or 'system class' if not found.
    """
    # Strip one level of template arguments (handles common patterns)
    raw = re.sub(r'<[^<>]*(?:<[^<>]*>[^<>]*)*>', '', base_name).strip()
    # Last component after :: gives the bare class name to search for
    class_only = raw.split('::')[-1].strip()
    if not class_only:
        return 'system class'

    pattern = re.compile(r'(?:class|struct)\s+' + re.escape(class_only) + r'\b')

    for hdr in header_files:
        try:
            with open(hdr, 'r', errors='replace') as fh:
                hdr_lines = fh.readlines()
            for line_num, line in enumerate(hdr_lines, 1):
                stripped = strip_line_comment(line).strip()
                if not pattern.search(stripped):
                    continue
                if 'friend' in stripped:
                    continue
                # Skip pure forward declarations (end with ; and no body)
                if stripped.endswith(';') and '{' not in stripped:
                    continue
                has_body = any(
                    '{' in strip_line_comment(hdr_lines[k])
                    for k in range(line_num - 1, min(line_num + 8, len(hdr_lines)))
                )
                if not has_body:
                    continue
                try:
                    rel = str(Path(hdr).relative_to(project_dir))
                except ValueError:
                    rel = str(hdr)
                return f"{rel}:{line_num}"
        except OSError:
            pass
    return 'system class'


_ACCESS_COLOUR = {
    'public':    C['cyan'],
    'protected': C['yellow'],
    'private':   C['gray'],
}


def print_ancestor_structured(anc: dict, location: str, first: bool = False):
    """Print one ancestor entry in structured format."""
    if not first:
        print(_SEP60)
    print(f"{_lbl('Name')}{C['bold']}{anc['name']}{C['reset']}")
    col = _ACCESS_COLOUR.get(anc['access'], C['reset'])
    print(f"{_lbl('Kind')}{col}{anc['access']}{C['reset']}")
    print(f"{_lbl('Location')}{location}")
    print()


# ── Main display ──────────────────────────────────────────────────────────────

def display(class_defs: list, source_files: list, header_files: list,
            project_dir: Path, fmt: str = 'lineal'):
    for n, cdef in enumerate(class_defs):
        if n > 0:
            print()
            print(C['gray'] + '─' * 72 + C['reset'])
            print()

        ns   = cdef['namespace']
        name = cdef['class_name']
        full_name = f"{ns}::{name}" if ns else name

        # ── Class header ──────────────────────────────────────────────────
        print(f"{C['bold']}{C['cyan']}{full_name}{C['reset']}")
        print(f"  {C['gray']}defined in:{C['reset']} "
              f"{cdef['header_file']}:{cdef['header_line']}")

        lines = cdef['lines']
        raw_methods = extract_public_methods(lines, cdef['header_line'] - 1)

        if not raw_methods:
            print(f"\n  {C['gray']}(no public methods found){C['reset']}")
            continue

        # ── Collect all methods with their locations ───────────────────────
        # Each entry: (hdr_line, sig_lines, mname, is_hdr_only, impl_file, impl_line)
        #   is_hdr_only = True for inline bodies, = delete, = default (no .cpp)

        all_methods = []
        impl_skip: dict = {}   # mname -> {str(src_file): last_found_line}

        for hdr_line, sig_lines in raw_methods:
            mname      = get_method_name(sig_lines)
            hdr_only   = has_inline_body(sig_lines) or is_deleted_or_defaulted(sig_lines)
            impl_file  = None
            impl_line  = None

            if not hdr_only:
                skip = impl_skip.get(mname, {})
                impl_file, impl_line = find_implementation(source_files, name, mname, skip)
                if impl_file and impl_line:
                    impl_skip.setdefault(mname, {})[str(impl_file)] = impl_line

            all_methods.append((hdr_line, sig_lines, mname, hdr_only, impl_file, impl_line))

        # ── Helpers ───────────────────────────────────────────────────────

        def _rel(p):
            try:
                return str(Path(p).relative_to(project_dir))
            except ValueError:
                return str(p)

        def _print_lineal(sig_lines, arrow_color, filepath, lineno):
            if filepath:
                loc = (f"  {C['gray']}=>{C['reset']} "
                       f"{arrow_color}{filepath}:{lineno}{C['reset']}")
            else:
                loc = f"  {C['gray']}=> (no implementation found){C['reset']}"
            sig  = format_sig(sig_lines)
            rows = sig.split('\n')
            if len(rows) == 1:
                print(f"    {rows[0]}{loc}")
            else:
                for row in rows[:-1]:
                    print(f"    {row}")
                print(f"    {rows[-1]}{loc}")
            print()

        def _print_structured(sig_lines, filepath, lineno, first=False):
            parsed = parse_signature(sig_lines, name)
            if filepath:
                loc = f"{arrow_color}{filepath}:{lineno}{C['reset']}"
            else:
                loc = f"{C['gray']}(no implementation found){C['reset']}"
            print_method_structured(parsed, loc, first=first)

        # ── ancestors: section ────────────────────────────────────────────
        print()
        print(f"  {C['bold']}ancestors:{C['reset']}")
        print(_SECT_SEP)
        ancestors = extract_ancestors(lines, cdef['header_line'] - 1)
        if ancestors:
            for i, anc in enumerate(ancestors):
                loc = find_ancestor_location(anc['name'], header_files, project_dir)
                print_ancestor_structured(anc, loc, first=(i == 0))
        else:
            print(f"  {C['gray']}(no base classes){C['reset']}")
            print()

        # ── header: section — every method, pointing to its .hpp line ─────
        print(f"  {C['bold']}header:{C['reset']}")
        print(_SECT_SEP)
        arrow_color = C['green']
        for i, (hdr_line, sig_lines, mname, hdr_only, impl_file, impl_line) in enumerate(all_methods):
            if fmt == 'structured':
                _print_structured(sig_lines, cdef['header_file'], hdr_line, first=(i == 0))
            else:
                _print_lineal(sig_lines, arrow_color,
                              cdef['header_file'], hdr_line)

        # ── implementation: section — methods that have a .cpp definition ─
        print(f"  {C['bold']}implementation:{C['reset']}")
        print(_SECT_SEP)
        arrow_color = C['yellow']
        has_any_impl = False
        impl_idx = 0
        for hdr_line, sig_lines, mname, hdr_only, impl_file, impl_line in all_methods:
            if hdr_only:
                continue          # inline / = delete / = default — header only
            has_any_impl = True
            rel_impl = _rel(impl_file) if impl_file else None
            if fmt == 'structured':
                _print_structured(sig_lines, rel_impl, impl_line, first=(impl_idx == 0))
            else:
                _print_lineal(sig_lines, arrow_color, rel_impl, impl_line)
            impl_idx += 1
        if not has_any_impl:
            print()


# ── Entry point ───────────────────────────────────────────────────────────────

def main():
    if len(sys.argv) < 3:
        print(f"Usage: {sys.argv[0]} class_name project_dir [fmt]", file=sys.stderr)
        sys.exit(1)

    class_name   = sys.argv[1]
    project_dir  = Path(sys.argv[2]).resolve()
    fmt          = sys.argv[3] if len(sys.argv) > 3 else 'structured'

    if not project_dir.exists():
        print(f"Error: project directory not found: {project_dir}", file=sys.stderr)
        sys.exit(1)

    def should_include(p: Path) -> bool:
        return not any(part in SKIP_DIRS or part.startswith('.')
                       for part in p.relative_to(project_dir).parts)

    headers = [h for h in project_dir.rglob('*.hpp') if should_include(h)]
    headers += [h for h in project_dir.rglob('*.h')   if should_include(h)]
    sources  = [s for s in project_dir.rglob('*.cpp') if should_include(s)]

    class_defs = []
    for hdr in headers:
        class_defs.extend(find_class_in_file(hdr, class_name, project_dir))

    if not class_defs:
        print(f"{C['red']}Error:{C['reset']} class '{class_name}' not found "
              f"(searched {len(headers)} header files)", file=sys.stderr)
        sys.exit(1)

    display(class_defs, sources, headers, project_dir, fmt)


main()
PYEOF
