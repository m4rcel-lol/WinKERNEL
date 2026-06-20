#!/usr/bin/env bash
# WinKernel NTKRNL-X — macOS toolchain setup (Mac mini: Intel or Apple Silicon)
# Installs the x86_64-elf cross-compiler and build tools via Homebrew, then
# stages the OVMF firmware for the QEMU UEFI boot path.
#
# Usage:  cd winkernel && ./tools/setup-macos.sh

set -euo pipefail

echo "[*] Checking for Xcode Command Line Tools..."
if ! xcode-select -p >/dev/null 2>&1; then
    echo "    Installing Command Line Tools (a GUI prompt may appear)..."
    xcode-select --install || true
    echo "    Re-run this script once the Command Line Tools finish installing."
    exit 1
fi

echo "[*] Checking for Homebrew..."
if ! command -v brew >/dev/null 2>&1; then
    echo "!!! Homebrew not found. Install it from https://brew.sh and re-run."
    exit 1
fi

echo "[*] Installing build dependencies (x86_64-elf-gcc pulls in binutils)..."
brew install x86_64-elf-gcc nasm xorriso qemu

echo "[*] Verifying toolchain..."
for tool in x86_64-elf-gcc x86_64-elf-ld nasm xorriso qemu-system-x86_64; do
    if ! command -v "$tool" >/dev/null 2>&1; then
        echo "!!! $tool not on PATH. Add Homebrew to your shell:"
        echo "    Apple Silicon: eval \"\$(/opt/homebrew/bin/brew shellenv)\""
        echo "    Intel:         eval \"\$(/usr/local/bin/brew shellenv)\""
        exit 1
    fi
done

echo "[*] Staging OVMF firmware for the UEFI boot path..."
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
WINKERNEL_DIR="$(dirname "$SCRIPT_DIR")"
OVMF_SRC="$(brew --prefix qemu)/share/qemu/edk2-x86_64-code.fd"
if [ -f "$OVMF_SRC" ]; then
    cp "$OVMF_SRC" "$WINKERNEL_DIR/OVMF.fd"
    echo "    Copied $OVMF_SRC -> winkernel/OVMF.fd"
else
    echo "    WARNING: OVMF firmware not found at $OVMF_SRC"
    echo "    The BIOS path (make run-bios) still works without it."
fi

echo ""
echo "[*] Setup complete. Build and run with:"
echo "    cd winkernel"
echo "    make"
echo "    make run-uefi    # UEFI boot path"
echo "    make run-bios    # legacy BIOS boot path"
