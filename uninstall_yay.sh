#!/bin/bash
# DESC=Intelligently uninstalls yay and cleans its cache.

set -e

die() {
    echo "ERROR: $1" >&2
    exit 1
}

main() {
    YAY_PATH=$(command -v yay || true)
    PKG_NAME=""

    if [[ -n "$YAY_PATH" ]]; then
        PKG_NAME=$(pacman -Qo "$YAY_PATH" 2>/dev/null | awk '{print $5}')
    fi

    if [[ -z "$PKG_NAME" ]]; then
        if pacman -Q yay-bin &>/dev/null; then
            PKG_NAME="yay-bin"
        elif pacman -Q yay &>/dev/null; then
            PKG_NAME="yay"
        elif pacman -Q yay-git &>/dev/null; then
            PKG_NAME="yay-git"
        fi
    fi

    if [[ -n "$PKG_NAME" ]]; then
        local pkgs_to_remove=("$PKG_NAME")
        if pacman -Q "${PKG_NAME}-debug" &>/dev/null; then
            pkgs_to_remove+=("${PKG_NAME}-debug")
        fi
        echo "--- Removing pacman package(s): ${pkgs_to_remove[*]} ---"
        # Used pkexec and noconfirm to skip terminal blocks
        pkexec pacman -Rns --noconfirm "${pkgs_to_remove[@]}" || die "Failed to remove package(s)."
    elif [[ -f "$YAY_PATH" ]]; then
        echo "--- Removing untracked yay binary at $YAY_PATH ---"
        pkexec rm "$YAY_PATH" || die "Failed to remove binary."
    else
        echo "yay does not appear to be installed. Nothing to do."
    fi

    if [ -d "$HOME/.cache/yay" ]; then
        echo "--- Removing yay cache ---"
        rm -rf "$HOME/.cache/yay"
    fi

    echo "Uninstallation complete!"
}

main
