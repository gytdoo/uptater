#!/bin/bash
# DESC=Interactively installs yay (AUR helper), intelligently handling dependencies and conflicts.

set -e

TMP_DIR=$(mktemp -d)
trap 'rm -rf "$TMP_DIR"' EXIT

# Force makepkg to use pkexec for dependency resolution and installation
export PACMAN_AUTH=pkexec

declare -A BUILD_PREREQUISITES=(
    [git]="Distributed version control system"
    [base-devel]="Tools for building AUR packages"
)
declare -A AUR_BIN_PREREQUISITES=(
    [git]="Distributed version control system"
    [base-devel]="Tools for building AUR packages"
)
declare -A DOWNLOAD_PREREQUISITES=(
    [wget]="Network downloader (temporary)"
)

die() {
    echo "ERROR: $1" >&2
    exit 1
}

check_if_already_installed() {
    local auto_mode="$1"
    if command -v yay &>/dev/null; then
        local yay_path
        yay_path=$(command -v yay)
        echo "yay is already installed at: $yay_path"

        if [[ -z "$auto_mode" ]]; then
            read -rp "Would you like to reinstall or replace it? [y/N]: " REINSTALL
            if [[ ! "$REINSTALL" =~ ^[Yy]$ ]]; then
                die "Skipping reinstallation. User aborted."
            fi
        fi

        local pkg_name
        pkg_name=$(pacman -Qo "$yay_path" 2>/dev/null | awk '{print $5}')

        if [[ -n "$pkg_name" ]]; then
            local pkgs_to_remove=("$pkg_name")
            if pacman -Q "${pkg_name}-debug" &>/dev/null; then
                pkgs_to_remove+=("${pkg_name}-debug")
            fi
            echo "Removing existing package(s) '${pkgs_to_remove[*]}' to prevent conflicts..."
            pkexec pacman -R --noconfirm "${pkgs_to_remove[@]}" || die "Failed to remove existing packages."
        else
            echo "Existing yay binary is not tracked by pacman. Removing it to prevent conflicts..."
            pkexec rm "$yay_path" || die "Failed to remove untracked yay binary."
        fi
    fi
}

handle_prerequisites() {
    local method="$1"
    local auto_mode="$2"
    declare -n prerequisites

    if [[ "$method" == "build" ]]; then prerequisites=BUILD_PREREQUISITES
    elif [[ "$method" == "aur-bin" ]]; then prerequisites=AUR_BIN_PREREQUISITES
    elif [[ "$method" == "download" ]]; then prerequisites=DOWNLOAD_PREREQUISITES
    else return; fi

    local to_install=()
    for pkg in "${!prerequisites[@]}"; do
        if ! pacman -Qo "$pkg" &>/dev/null && ! pacman -Qg "$pkg" &>/dev/null; then
            to_install+=("$pkg")
        fi
    done

    if [ ${#to_install[@]} -eq 0 ]; then return; fi

    if [[ -z "$auto_mode" ]]; then
        read -rp "Press Enter to install prerequisites, or Ctrl+C to cancel..."
    fi

    # Use pkexec so we get a GUI prompt if run from Uptater dashboard
    pkexec pacman -Sy --needed --noconfirm "${to_install[@]}" || die "Package installation failed."
}

install_from_aur() {
    local pkg_name="$1"
    local repo_url="https://aur.archlinux.org/${pkg_name}.git"
    local repo_dir="$TMP_DIR/$pkg_name"

    echo "--- Building '$pkg_name' from AUR ---"
    git clone "$repo_url" "$repo_dir" || die "Failed to clone repository"
    cd "$repo_dir" || die "Failed to enter directory"

    echo "options=(!debug)" >> PKGBUILD
    # Makepkg uses pkexec automatically because we exported PACMAN_AUTH
    makepkg -si --noconfirm || die "Failed to build and install '$pkg_name'."
}

install_from_github() {
    echo "--- Installing yay binary from GitHub ---"
    local api_url="https://api.github.com/repos/Jguer/yay/releases/latest"
    local tmp_archive="$TMP_DIR/yay_release.tar.gz"
    local asset_url

    if command -v jq &>/dev/null; then
        asset_url=$(curl -fsSL "$api_url" | jq -r '.assets[] | select(.name | endswith("_x86_64.tar.gz")) | .browser_download_url')
    else
        asset_url=$(curl -fsSL "$api_url" | grep "browser_download_url" | grep "_x86_64.tar.gz" | sed -E 's/.*"([^"]+)".*/\1/')
    fi

    [[ -n "$asset_url" ]] || die "Could not find an x86_64 asset URL."
    wget -qO "$tmp_archive" "$asset_url" || die "Failed to download release."
    tar -xzf "$tmp_archive" -C "$TMP_DIR" || die "Failed to extract release."

    local bin_path
    bin_path=$(find "$TMP_DIR" -name yay -type f -executable)
    [[ -n "$bin_path" ]] || die "Could not find 'yay' binary."

    pkexec install -Dm755 "$bin_path" "/usr/bin/yay" || die "Failed to install."
}

main() {
    local auto_choice=""
    local is_auto=""

    # Detect if we are being run by Uptater GUI with arguments
    if [[ -n "$1" ]]; then
        is_auto="1"
        case "$1" in
            yay-bin) auto_choice=1 ;;
            yay)     auto_choice=2 ;;
            github)  auto_choice=3 ;;
            *)       is_auto="" ;;
        esac
    fi

    check_if_already_installed "$is_auto"

    local choice
    if [[ -n "$is_auto" ]]; then
        choice="$auto_choice"
    else
        echo "1) Install pre-compiled binary from AUR (yay-bin)"
        echo "2) Build from source from AUR (yay)"
        echo "3) Install pre-compiled binary from GitHub (Fallback)"
        read -rp "Enter your choice [1]: " choice
        choice=${choice:-1}
    fi

    case "$choice" in
        1) handle_prerequisites "aur-bin" "$is_auto"; install_from_aur "yay-bin" ;;
        2) handle_prerequisites "build" "$is_auto"; install_from_aur "yay" ;;
        3) handle_prerequisites "download" "$is_auto"; install_from_github ;;
        *) die "Invalid choice." ;;
    esac

    echo "Installation complete!"
}

main "$@"
