#!/usr/bin/env bash
#
# manage_valgrind.sh - Manage Valgrind installation from source
#
# NOTE FOR LLMs (Claude, ChatGPT, etc.):
# ANY change to this script, no matter how small, MUST update SCRIPT_VERSION
# to the current system date/time. Do NOT invent dates. Use actual system time.
# Format: ISO 8601 (e.g., 2026-02-15T14:30:45-0800)
# Command: date '+%Y-%m-%dT%H:%M:%S%z'
#
SCRIPT_VERSION="2026-02-15T15:13:04-0800"

# Installation directory for compiled version
CUSTOM_VALGRIND_PREFIX="/usr/local/valgrind-custom"
CUSTOM_VALGRIND_BIN="${CUSTOM_VALGRIND_PREFIX}/bin/valgrind"
WRAPPER_SCRIPT="/usr/local/bin/valgrind"

set -euo pipefail

# Configuration directory for tracking installed dependencies
CONFIG_DIR="${HOME}/.config/manage_valgrind"
DEPS_FILE="${CONFIG_DIR}/installed_deps.txt"

# ============================================================================
# Color and Icon Configuration
# ============================================================================

# Detect if terminal supports colors
if command -v tput &>/dev/null && [[ -t 1 ]] && [[ $(tput colors 2>/dev/null || echo 0) -gt 0 ]]; then
    USE_COLORS=true
    COLOR_RESET=$(tput sgr0)
    COLOR_RED=$(tput setaf 1)
    COLOR_GREEN=$(tput setaf 2)
    COLOR_YELLOW=$(tput setaf 3)
    COLOR_BLUE=$(tput setaf 4)
    COLOR_CYAN=$(tput setaf 6)
    COLOR_BOLD=$(tput bold)
else
    USE_COLORS=false
    COLOR_RESET=""
    COLOR_RED=""
    COLOR_GREEN=""
    COLOR_YELLOW=""
    COLOR_BLUE=""
    COLOR_CYAN=""
    COLOR_BOLD=""
fi

# ASCII Icons
ICON_INFO="[i]"
ICON_SUCCESS="[+]"
ICON_ERROR="[!]"
ICON_WARNING="[*]"
ICON_ARROW="==>"

# ============================================================================
# Helper Functions
# ============================================================================

print_version() {
    echo "${COLOR_CYAN}${COLOR_BOLD}manage_valgrind.sh${COLOR_RESET} - Version: ${COLOR_YELLOW}${SCRIPT_VERSION}${COLOR_RESET}"
    echo ""
}

print_info() {
    echo "${COLOR_BLUE}${ICON_INFO}${COLOR_RESET} $*"
}

print_success() {
    echo "${COLOR_GREEN}${ICON_SUCCESS}${COLOR_RESET} $*"
}

print_error() {
    echo "${COLOR_RED}${ICON_ERROR}${COLOR_RESET} $*" >&2
}

print_warning() {
    echo "${COLOR_YELLOW}${ICON_WARNING}${COLOR_RESET} $*"
}

print_step() {
    echo "${COLOR_CYAN}${ICON_ARROW}${COLOR_RESET} ${COLOR_BOLD}$*${COLOR_RESET}"
}

show_help() {
    cat <<EOF
${COLOR_BOLD}USAGE:${COLOR_RESET}
    $0 [OPTION]

${COLOR_BOLD}OPTIONS:${COLOR_RESET}
    ${COLOR_GREEN}--install[=PATH]${COLOR_RESET}
        Download, compile, and install Valgrind
        PATH: Installation directory (default: $CUSTOM_VALGRIND_PREFIX)
        Does NOT replace system Valgrind. Only downloads, compiles, and installs.
        Automatically installs missing dependencies (with confirmation).
        Validates that PATH doesn't conflict with system installation.

    ${COLOR_GREEN}--install-default${COLOR_RESET}
        Download, compile, and install in $CUSTOM_VALGRIND_PREFIX (if needed)
        Configure system to use compiled version as default via wrapper script
        Creates wrapper script at $WRAPPER_SCRIPT that executes compiled version

    ${COLOR_GREEN}--uninstall${COLOR_RESET}
        Uninstall Valgrind from $CUSTOM_VALGRIND_PREFIX if available
        Warns if wrapper script points to custom version (you may want to switch first)
        Optionally uninstalls dependencies that were installed by this script

    ${COLOR_GREEN}--default${COLOR_RESET}
        Configure system to use compiled version as default
        Errors if $CUSTOM_VALGRIND_BIN is not available

    ${COLOR_GREEN}--valgrind-versions${COLOR_RESET}
        Show Valgrind versions (compiled and system package)

    ${COLOR_GREEN}--valgrind-switch${COLOR_RESET}
        Interactive menu to switch between system and compiled versions
        Updates wrapper script at $WRAPPER_SCRIPT to point to selected version

    ${COLOR_GREEN}--help${COLOR_RESET}
        Show this help message

${COLOR_BOLD}EXAMPLES:${COLOR_RESET}
    # Install Valgrind from source in $CUSTOM_VALGRIND_PREFIX
    $0 --install

    # Install and set as system default
    $0 --install-default

    # Set existing custom installation as default
    $0 --default

    # Switch between versions interactively
    $0 --valgrind-switch

    # Uninstall custom version
    $0 --uninstall

${COLOR_BOLD}HOW IT WORKS:${COLOR_RESET}
    - Compiled version: $CUSTOM_VALGRIND_BIN
    - System version: /usr/bin/valgrind (from package manager)
    - Wrapper script: $WRAPPER_SCRIPT (permanent, always exists)
    - PATH priority: /usr/local/bin comes before /usr/bin
    - The wrapper uses 'exec' to execute the selected version (zero overhead)
    - Wrapper persists across switches (solves bash command hash cache issues)

${COLOR_BOLD}NOTES:${COLOR_RESET}
    - Build directory: /tmp/valgrind (temporary)
    - Requires sudo for installation
    - Automatically detects and installs missing dependencies
    - Tracks installed dependencies in: ~/.config/manage_valgrind/
    - Supports: apt, dnf, yum, pacman, zypper

EOF
}

get_valgrind_version() {
    local valgrind_path="$1"

    if [[ ! -f "$valgrind_path" ]]; then
        echo "not installed"
        return 1
    fi

    # Get version and extract just the version number
    local version
    version=$("$valgrind_path" --version 2>/dev/null | head -1)
    echo "$version"
    return 0
}

detect_package_manager() {
    if command -v apt-get &>/dev/null; then
        echo "apt"
    elif command -v dnf &>/dev/null; then
        echo "dnf"
    elif command -v yum &>/dev/null; then
        echo "yum"
    elif command -v pacman &>/dev/null; then
        echo "pacman"
    elif command -v zypper &>/dev/null; then
        echo "zypper"
    else
        echo "unknown"
    fi
}

get_package_names() {
    local pkg_manager="$1"
    local cmd="$2"

    # Map command names to package names for different distros
    case "$pkg_manager" in
        apt)
            case "$cmd" in
                gcc|g++|make) echo "build-essential" ;;
                *) echo "$cmd" ;;
            esac
            ;;
        dnf|yum)
            case "$cmd" in
                gcc|g++|make) echo "gcc gcc-c++ make" ;;
                *) echo "$cmd" ;;
            esac
            ;;
        pacman)
            case "$cmd" in
                gcc|g++|make) echo "base-devel" ;;
                *) echo "$cmd" ;;
            esac
            ;;
        zypper)
            case "$cmd" in
                gcc|g++|make) echo "gcc gcc-c++ make" ;;
                *) echo "$cmd" ;;
            esac
            ;;
        *)
            echo "$cmd"
            ;;
    esac
}

install_dependencies() {
    local pkg_manager="$1"
    shift
    local packages=("$@")

    print_step "Installing dependencies with $pkg_manager"

    case "$pkg_manager" in
        apt)
            sudo apt-get update -qq
            sudo apt-get install -y "${packages[@]}" || return 1
            ;;
        dnf)
            sudo dnf install -y "${packages[@]}" || return 1
            ;;
        yum)
            sudo yum install -y "${packages[@]}" || return 1
            ;;
        pacman)
            sudo pacman -S --noconfirm "${packages[@]}" || return 1
            ;;
        zypper)
            sudo zypper install -y "${packages[@]}" || return 1
            ;;
        *)
            print_error "Unsupported package manager: $pkg_manager"
            return 1
            ;;
    esac

    return 0
}

save_installed_deps() {
    local deps=("$@")

    # Create config directory if it doesn't exist
    mkdir -p "$CONFIG_DIR"

    # Save installed dependencies
    printf "%s\n" "${deps[@]}" > "$DEPS_FILE"

    print_info "Saved installed dependencies to $DEPS_FILE"
}

check_requirements() {
    local missing_cmds=()
    local required_cmds=(git make gcc g++ autoconf automake libtool)

    # Check which commands are missing
    for cmd in "${required_cmds[@]}"; do
        if ! command -v "$cmd" &>/dev/null; then
            missing_cmds+=("$cmd")
        fi
    done

    # If all dependencies are present, we're done
    if [[ ${#missing_cmds[@]} -eq 0 ]]; then
        return 0
    fi

    # Detect package manager
    local pkg_manager
    pkg_manager=$(detect_package_manager)

    if [[ "$pkg_manager" == "unknown" ]]; then
        print_error "Could not detect package manager"
        print_error "Missing required dependencies: ${missing_cmds[*]}"
        print_info "Please install them manually"
        return 1
    fi

    print_warning "Missing required dependencies: ${missing_cmds[*]}"
    print_info "Detected package manager: $pkg_manager"

    # Build list of packages to install (handling build-essential, base-devel, etc.)
    local packages_to_install=()
    local unique_packages=()

    for cmd in "${missing_cmds[@]}"; do
        local pkg_names
        pkg_names=$(get_package_names "$pkg_manager" "$cmd")
        for pkg in $pkg_names; do
            # Avoid duplicates
            if [[ ! " ${unique_packages[*]} " =~ " ${pkg} " ]]; then
                unique_packages+=("$pkg")
            fi
        done
    done

    print_info "Packages to install: ${unique_packages[*]}"

    # Ask user for confirmation
    read -p "Install these dependencies? (Y/n): " -n 1 -r
    echo
    if [[ $REPLY =~ ^[Nn]$ ]]; then
        print_error "Dependencies are required for installation"
        return 1
    fi

    # Install dependencies
    install_dependencies "$pkg_manager" "${unique_packages[@]}" || {
        print_error "Failed to install dependencies"
        return 1
    }

    # Save which dependencies we installed
    save_installed_deps "${unique_packages[@]}"

    print_success "Dependencies installed successfully"

    return 0
}

configure_as_default() {
    local valgrind_path="$1"

    print_step "Configuring Valgrind as system default"

    # Verify target exists
    if [[ ! -f "$valgrind_path" ]]; then
        print_error "Target Valgrind not found: $valgrind_path"
        return 1
    fi

    # Create/update the permanent wrapper script
    print_info "Creating/updating wrapper script at $WRAPPER_SCRIPT"

    local wrapper_content
    wrapper_content=$(cat <<'WRAPPER_EOF'
#!/usr/bin/env bash
#
# Valgrind Version Switcher Wrapper
# Managed by manage_valgrind.sh
#
# This wrapper permanently exists at /usr/local/bin/valgrind to avoid
# bash command hash cache issues when switching between versions.
# The wrapper uses 'exec' to replace itself with the target process,
# ensuring zero performance overhead and proper signal/exit code handling.
#
# DO NOT EDIT MANUALLY - managed by manage_valgrind.sh --valgrind-switch
#
# ACTIVE_TARGET_MARKER: TARGET_PATH_PLACEHOLDER

exec "TARGET_PATH_PLACEHOLDER" "$@"
WRAPPER_EOF
)

    # Replace placeholder with actual path
    wrapper_content="${wrapper_content//TARGET_PATH_PLACEHOLDER/$valgrind_path}"

    # Write wrapper script
    echo "$wrapper_content" | sudo tee "$WRAPPER_SCRIPT" >/dev/null || {
        print_error "Failed to create wrapper script"
        return 1
    }

    # Make executable
    sudo chmod +x "$WRAPPER_SCRIPT" || {
        print_error "Failed to make wrapper executable"
        return 1
    }

    print_success "Wrapper script configured successfully"
    print_info "Wrapper: $WRAPPER_SCRIPT"
    print_info "Target: $valgrind_path"

    return 0
}

remove_default_configuration() {
    print_step "Removing default configuration"

    # Remove wrapper script if it exists
    if [[ -f "$WRAPPER_SCRIPT" ]]; then
        sudo rm -f "$WRAPPER_SCRIPT" 2>/dev/null || {
            print_warning "Failed to remove wrapper script"
            return 1
        }
        print_success "Removed wrapper script: $WRAPPER_SCRIPT"
    else
        print_info "Wrapper script not found (nothing to remove)"
    fi
}

# ============================================================================
# Main Functions
# ============================================================================

do_install() {
    local install_prefix="${1:-$CUSTOM_VALGRIND_PREFIX}"

    print_step "Installing Valgrind from source"
    print_info "Installation prefix: $install_prefix"

    # Validate installation path doesn't conflict with system installation
    if [[ -f /usr/bin/valgrind ]]; then
        local system_bin_dir="/usr/bin"
        local target_bin_dir="${install_prefix}/bin"

        # Normalize paths for comparison
        system_bin_dir=$(cd "$system_bin_dir" 2>/dev/null && pwd || echo "/usr/bin")
        target_bin_dir_parent=$(dirname "$target_bin_dir")

        # Check if trying to install in /usr (system location)
        if [[ "$install_prefix" == "/usr" || "$target_bin_dir" == "/usr/bin" ]]; then
            print_error "Installation path conflicts with system Valgrind"
            print_error "System Valgrind is installed at: /usr/bin/valgrind"
            print_error "Requested installation path: $target_bin_dir"
            echo ""
            print_info "The installation path /usr is reserved for system packages"
            print_info "Please select a different path, such as:"
            print_info "  - $CUSTOM_VALGRIND_PREFIX (default)"
            print_info "  - /opt/valgrind"
            print_info "  - \$HOME/valgrind"
            return 1
        fi
    fi

    # Capture system Valgrind version before installation (if exists)
    local system_valgrind_version=""
    local system_valgrind_path=""

    # Check for system Valgrind
    if command -v valgrind &>/dev/null; then
        system_valgrind_path=$(command -v valgrind)
        if [[ "$system_valgrind_path" != "${install_prefix}/bin/valgrind" ]]; then
            system_valgrind_version=$(get_valgrind_version "$system_valgrind_path")
            print_info "Found system Valgrind: $system_valgrind_version at $system_valgrind_path"
        fi
    fi

    check_requirements || return 1

    # Check if already installed at target location
    if [[ -f "${install_prefix}/bin/valgrind" ]]; then
        print_warning "Valgrind already exists in ${install_prefix}/bin"
        read -p "Do you want to reinstall? (y/N): " -n 1 -r
        echo
        if [[ ! $REPLY =~ ^[Yy]$ ]]; then
            print_info "Installation cancelled"
            return 0
        fi
    fi

    local tmp_dir="/tmp/valgrind"

    # Clean up old build directory
    if [[ -d "$tmp_dir" ]]; then
        print_info "Removing old build directory: $tmp_dir"
        rm -rf "$tmp_dir"
    fi

    # Clone repository
    print_step "Cloning Valgrind repository"
    cd /tmp
    git clone https://sourceware.org/git/valgrind.git || {
        print_error "Failed to clone repository"
        return 1
    }

    cd "$tmp_dir"

    # Build
    print_step "Running autogen.sh"
    ./autogen.sh || {
        print_error "autogen.sh failed"
        return 1
    }

    print_step "Configuring (prefix=$install_prefix)"
    ./configure --prefix="$install_prefix" || {
        print_error "configure failed"
        return 1
    }

    print_step "Compiling (using $(nproc) cores)"
    make -j"$(nproc)" || {
        print_error "compilation failed"
        return 1
    }

    # Install
    print_step "Installing to $install_prefix (requires sudo)"
    sudo make install || {
        print_error "installation failed"
        return 1
    }

    print_success "Valgrind successfully installed to $install_prefix"

    # Cleanup
    print_step "Cleaning up build directory"
    cd /
    rm -rf "$tmp_dir"

    print_success "Installation complete!"
    echo ""

    # Show version comparison
    print_step "Version Information"

    if [[ -f "${install_prefix}/bin/valgrind" ]]; then
        local new_version
        new_version=$(get_valgrind_version "${install_prefix}/bin/valgrind")
        print_success "New version installed: ${COLOR_BOLD}$new_version${COLOR_RESET}"
        print_info "Location: ${install_prefix}/bin/valgrind"
    fi

    echo ""

    if [[ -n "$system_valgrind_version" ]]; then
        print_info "Previous system version: ${COLOR_BOLD}$system_valgrind_version${COLOR_RESET}"
        print_info "Location: $system_valgrind_path"
    else
        print_info "No previous system Valgrind was found"
    fi
}

do_install_default() {
    print_step "Installing Valgrind and setting as default"

    # Check if already installed
    if [[ ! -f "$CUSTOM_VALGRIND_BIN" ]]; then
        # Not installed, need to install first
        do_install || return 1
    else
        print_info "Valgrind already installed at $CUSTOM_VALGRIND_BIN"
    fi

    # Check if system valgrind exists
    if [[ -f /usr/bin/valgrind ]]; then
        local system_version
        system_version=$(get_valgrind_version "/usr/bin/valgrind")
        print_info "Found system Valgrind: $system_version"
    else
        print_info "No system Valgrind found"
    fi

    # Configure as default
    configure_as_default "$CUSTOM_VALGRIND_BIN" || return 1

    print_success "Installation and default configuration complete!"
}

uninstall_dependencies() {
    if [[ ! -f "$DEPS_FILE" ]]; then
        print_info "No dependency tracking file found"
        print_info "Dependencies were not installed by this script or tracking file was deleted"
        return 0
    fi

    # Read installed dependencies
    local installed_deps=()
    while IFS= read -r line; do
        [[ -n "$line" ]] && installed_deps+=("$line")
    done < "$DEPS_FILE"

    if [[ ${#installed_deps[@]} -eq 0 ]]; then
        print_info "No dependencies were installed by this script"
        return 0
    fi

    print_warning "The following dependencies were installed by this script:"
    printf "  - %s\n" "${installed_deps[@]}"
    echo ""
    print_warning "Removing these packages may affect other programs on your system"

    read -p "Do you want to uninstall these dependencies? (y/N): " -n 1 -r
    echo
    if [[ ! $REPLY =~ ^[Yy]$ ]]; then
        print_info "Keeping dependencies installed"
        return 0
    fi

    # Detect package manager
    local pkg_manager
    pkg_manager=$(detect_package_manager)

    if [[ "$pkg_manager" == "unknown" ]]; then
        print_error "Could not detect package manager"
        print_info "Please remove dependencies manually: ${installed_deps[*]}"
        return 1
    fi

    print_step "Uninstalling dependencies with $pkg_manager"

    case "$pkg_manager" in
        apt)
            sudo apt-get remove -y "${installed_deps[@]}" || {
                print_warning "Some dependencies may not have been removed"
            }
            ;;
        dnf)
            sudo dnf remove -y "${installed_deps[@]}" || {
                print_warning "Some dependencies may not have been removed"
            }
            ;;
        yum)
            sudo yum remove -y "${installed_deps[@]}" || {
                print_warning "Some dependencies may not have been removed"
            }
            ;;
        pacman)
            sudo pacman -R --noconfirm "${installed_deps[@]}" || {
                print_warning "Some dependencies may not have been removed"
            }
            ;;
        zypper)
            sudo zypper remove -y "${installed_deps[@]}" || {
                print_warning "Some dependencies may not have been removed"
            }
            ;;
    esac

    # Remove tracking file
    rm -f "$DEPS_FILE"
    rmdir "$CONFIG_DIR" 2>/dev/null || true

    print_success "Dependencies uninstalled"
}

do_uninstall() {
    print_step "Uninstalling Valgrind from $CUSTOM_VALGRIND_PREFIX"

    # Check if installed
    if [[ ! -f "$CUSTOM_VALGRIND_BIN" ]]; then
        print_error "Valgrind is not installed at $CUSTOM_VALGRIND_BIN"
        print_info "Nothing to uninstall"
        return 1
    fi

    # Confirm
    print_warning "This will remove Valgrind from $CUSTOM_VALGRIND_PREFIX"
    read -p "Are you sure? (y/N): " -n 1 -r
    echo
    if [[ ! $REPLY =~ ^[Yy]$ ]]; then
        print_info "Uninstall cancelled"
        return 0
    fi

    # Check if wrapper script points to our custom version
    print_step "Checking wrapper script configuration"
    if [[ -f "$WRAPPER_SCRIPT" ]]; then
        if grep -q "$CUSTOM_VALGRIND_BIN" "$WRAPPER_SCRIPT" 2>/dev/null; then
            print_info "Wrapper script currently points to custom Valgrind"
            print_warning "After uninstall, you may want to run: $0 --valgrind-switch"
            print_warning "to switch to system Valgrind, or remove wrapper with: sudo rm $WRAPPER_SCRIPT"
        fi
    fi

    # Manual removal of all files
    print_info "Removing Valgrind files from $CUSTOM_VALGRIND_PREFIX"

    sudo rm -rf "$CUSTOM_VALGRIND_PREFIX" || {
        print_error "Failed to remove $CUSTOM_VALGRIND_PREFIX"
        return 1
    }

    print_success "Valgrind uninstalled from $CUSTOM_VALGRIND_PREFIX"

    # Ask about dependencies
    echo ""
    uninstall_dependencies
}

do_default() {
    print_step "Configuring Valgrind default"

    # Check if custom valgrind exists
    if [[ ! -f "$CUSTOM_VALGRIND_BIN" ]]; then
        print_error "$CUSTOM_VALGRIND_BIN does not exist"
        print_info "Run '$0 --install' first to install Valgrind"
        return 1
    fi

    configure_as_default "$CUSTOM_VALGRIND_BIN" || return 1

    print_success "Default configuration complete!"
}

do_version() {
    print_step "Valgrind Version Information"
    echo ""

    # Check for compiled version
    if [[ -f "$CUSTOM_VALGRIND_BIN" ]]; then
        local custom_version
        custom_version=$(get_valgrind_version "$CUSTOM_VALGRIND_BIN")
        print_success "Compiled version: ${COLOR_BOLD}$custom_version${COLOR_RESET}"
        print_info "Location: $CUSTOM_VALGRIND_BIN"
    else
        print_warning "Compiled version: ${COLOR_BOLD}Not installed${COLOR_RESET}"
        print_info "Run '$0 --install' to install from source"
    fi

    echo ""

    # Check for system version
    if [[ -f /usr/bin/valgrind ]]; then
        local system_version
        system_version=$(get_valgrind_version "/usr/bin/valgrind")
        print_info "System version (package): ${COLOR_BOLD}$system_version${COLOR_RESET}"
        print_info "Location: /usr/bin/valgrind"
    else
        print_info "System version (package): ${COLOR_BOLD}Not installed${COLOR_RESET}"
    fi

    echo ""

    # Show currently active version via wrapper script
    if [[ -f "$WRAPPER_SCRIPT" ]]; then
        # Extract target path from wrapper script
        local wrapper_target
        wrapper_target=$(grep "^# ACTIVE_TARGET_MARKER:" "$WRAPPER_SCRIPT" 2>/dev/null | sed 's/^# ACTIVE_TARGET_MARKER: //')

        if [[ -n "$wrapper_target" && -f "$wrapper_target" ]]; then
            local active_version
            active_version=$(get_valgrind_version "$wrapper_target")
            print_step "Currently active (via wrapper): ${COLOR_GREEN}${COLOR_BOLD}$active_version${COLOR_RESET}"
            print_info "Wrapper: $WRAPPER_SCRIPT"
            print_info "Target: $wrapper_target"
        else
            print_warning "Wrapper script exists but target is invalid: $wrapper_target"
        fi
    elif command -v valgrind &>/dev/null; then
        local active_valgrind
        active_valgrind=$(command -v valgrind)
        local active_version
        active_version=$(get_valgrind_version "$active_valgrind")

        print_step "Currently active (no wrapper): ${COLOR_GREEN}${COLOR_BOLD}$active_version${COLOR_RESET}"
        print_info "Path: $active_valgrind"
    else
        print_warning "No Valgrind is currently active in PATH"
    fi
}

do_switch() {
    print_step "Switch Valgrind Version"
    echo ""

    # Collect available Valgrind installations
    local options=()
    local paths=()
    local versions=()

    # Check for system version
    if [[ -f /usr/bin/valgrind ]]; then
        local system_version
        system_version=$(get_valgrind_version "/usr/bin/valgrind")
        options+=("System version (package)")
        paths+=("/usr/bin/valgrind")
        versions+=("$system_version")
    fi

    # Check for compiled version
    if [[ -f "$CUSTOM_VALGRIND_BIN" ]]; then
        local compiled_version
        compiled_version=$(get_valgrind_version "$CUSTOM_VALGRIND_BIN")
        options+=("Compiled version (custom)")
        paths+=("$CUSTOM_VALGRIND_BIN")
        versions+=("$compiled_version")
    fi

    # Check if we have any options
    if [[ ${#options[@]} -eq 0 ]]; then
        print_error "No Valgrind installations found"
        print_info "Install Valgrind with: $0 --install"
        return 1
    fi

    # Only one option available
    if [[ ${#options[@]} -eq 1 ]]; then
        print_warning "Only one Valgrind installation found:"
        print_info "${options[0]}: ${versions[0]}"
        print_info "Location: ${paths[0]}"
        print_info "Nothing to switch"
        return 0
    fi

    # Determine currently active version by reading wrapper script
    local active_path=""
    if [[ -f "$WRAPPER_SCRIPT" ]]; then
        # Extract target path from wrapper script
        active_path=$(grep "^# ACTIVE_TARGET_MARKER:" "$WRAPPER_SCRIPT" 2>/dev/null | sed 's/^# ACTIVE_TARGET_MARKER: //')
        if [[ -n "$active_path" ]]; then
            print_info "Wrapper script currently points to: $active_path"
        fi
    elif command -v valgrind &>/dev/null; then
        # No wrapper yet, check what's in PATH
        active_path=$(command -v valgrind)
        print_info "No wrapper script found, current PATH points to: $active_path"
    fi

    # Display menu
    print_info "${COLOR_BOLD}Available Valgrind installations:${COLOR_RESET}"
    echo ""

    for i in "${!options[@]}"; do
        local num=$((i + 1))
        local is_active=""

        if [[ "${paths[$i]}" == "$active_path" ]]; then
            is_active=" ${COLOR_GREEN}${COLOR_BOLD}[CURRENTLY ACTIVE]${COLOR_RESET}"
        fi

        echo "  ${COLOR_CYAN}$num)${COLOR_RESET} ${COLOR_BOLD}${options[$i]}${COLOR_RESET}: ${versions[$i]}$is_active"
        echo "     Location: ${paths[$i]}"
        echo ""
    done

    # Prompt for selection
    while true; do
        read -p "Select version to activate (1-${#options[@]}, or q to quit): " selection

        # Check for quit
        if [[ "$selection" == "q" || "$selection" == "Q" ]]; then
            print_info "Switch cancelled"
            return 0
        fi

        # Validate input is a number
        if ! [[ "$selection" =~ ^[0-9]+$ ]]; then
            print_error "Invalid input. Please enter a number between 1 and ${#options[@]}, or 'q' to quit"
            continue
        fi

        # Validate range
        if [[ "$selection" -lt 1 || "$selection" -gt ${#options[@]} ]]; then
            print_error "Invalid selection. Please enter a number between 1 and ${#options[@]}"
            continue
        fi

        # Valid selection, break loop
        break
    done

    # Convert to array index (0-based)
    local selected_idx=$((selection - 1))
    local selected_path="${paths[$selected_idx]}"
    local selected_version="${versions[$selected_idx]}"
    local selected_name="${options[$selected_idx]}"

    # Check if already active
    if [[ "$selected_path" == "$active_path" ]]; then
        print_warning "$selected_name is already active"
        print_info "Version: $selected_version"
        print_info "Path: $selected_path"
        return 0
    fi

    # Configure as default
    echo ""
    print_step "Switching to: $selected_name"
    print_info "Version: $selected_version"
    print_info "Path: $selected_path"
    echo ""

    configure_as_default "$selected_path" || {
        print_error "Failed to switch Valgrind version"
        return 1
    }

    print_success "Successfully switched to: $selected_version"

    # Verify the switch by checking wrapper script content
    echo ""
    print_step "Verifying switch"

    if [[ ! -f "$WRAPPER_SCRIPT" ]]; then
        print_error "Wrapper script not found after configuration"
        return 1
    fi

    # Extract target from wrapper script
    local wrapper_target
    wrapper_target=$(grep "^# ACTIVE_TARGET_MARKER:" "$WRAPPER_SCRIPT" 2>/dev/null | sed 's/^# ACTIVE_TARGET_MARKER: //')

    if [[ "$wrapper_target" == "$selected_path" ]]; then
        print_success "Wrapper script configuration verified"
        print_info "Wrapper: $WRAPPER_SCRIPT"
        print_info "Target: $wrapper_target"

        # Test that the wrapper actually works
        if "$WRAPPER_SCRIPT" --version &>/dev/null; then
            local wrapper_version
            wrapper_version=$("$WRAPPER_SCRIPT" --version 2>/dev/null | head -1)
            print_success "Wrapper execution test: ${COLOR_BOLD}$wrapper_version${COLOR_RESET}"
        else
            print_warning "Wrapper script exists but execution test failed"
        fi
    else
        print_error "Wrapper target mismatch!"
        print_error "Expected: $selected_path"
        print_error "Found: $wrapper_target"
        return 1
    fi

    echo ""
    print_info "${COLOR_BOLD}No need to run 'hash -r' - wrapper script persists at same path${COLOR_RESET}"
    print_info "Your shell will automatically use the new version"
}

# ============================================================================
# Main Entry Point
# ============================================================================

main() {
    local exit_code=0

    # Show version at start
    if [[ $# -eq 0 ]]; then
        show_help
        exit_code=0
    else
        # Parse --install with optional path
        if [[ "$1" == --install || "$1" == --install=* ]]; then
            local install_path="/usr/local"
            if [[ "$1" == --install=* ]]; then
                install_path="${1#--install=}"
                # Validate path is provided
                if [[ -z "$install_path" ]]; then
                    print_error "Installation path cannot be empty"
                    print_info "Usage: --install[=/path/to/install]"
                    exit_code=1
                else
                    do_install "$install_path" || exit_code=$?
                fi
            else
                do_install || exit_code=$?
            fi
        else
            case "$1" in
                --install-default)
                    do_install_default || exit_code=$?
                    ;;
                --uninstall)
                    do_uninstall || exit_code=$?
                    ;;
                --default)
                    do_default || exit_code=$?
                    ;;
                --valgrind-versions)
                    do_version || exit_code=$?
                    ;;
                --valgrind-switch)
                    do_switch || exit_code=$?
                    ;;
                --help)
                    show_help
                    exit_code=0
                    ;;
                *)
                    print_error "Unknown option: $1"
                    echo ""
                    show_help
                    exit_code=1
                    ;;
            esac
        fi
    fi

    # Show script version at the end (always)
    echo ""
    echo ""
    print_version

    return $exit_code
}

main "$@"
