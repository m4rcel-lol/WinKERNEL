#!/usr/bin/env bash
# WinKernel NTKRNL-X — Toolchain setup script (Ubuntu/Debian)
# Run this once to install all required build dependencies.

set -e

echo "[*] Installing build dependencies..."
sudo apt-get update -qq
sudo apt-get install -y \
    build-essential \
    nasm \
    xorriso \
    mtools \
    qemu-system-x86 \
    ovmf \
    wget \
    curl \
    git

echo "[*] Building x86_64-elf cross-compiler..."
BINUTILS_VER=2.41
GCC_VER=13.2.0
PREFIX="$HOME/opt/cross"
TARGET=x86_64-elf
export PATH="$PREFIX/bin:$PATH"

mkdir -p "$HOME/src"
cd "$HOME/src"

# Download binutils
if [ ! -f "binutils-$BINUTILS_VER.tar.gz" ]; then
    wget -q "https://ftp.gnu.org/gnu/binutils/binutils-$BINUTILS_VER.tar.gz"
fi
tar -xf "binutils-$BINUTILS_VER.tar.gz"

# Build binutils
mkdir -p build-binutils && cd build-binutils
"../binutils-$BINUTILS_VER/configure" \
    --target=$TARGET \
    --prefix="$PREFIX" \
    --with-sysroot \
    --disable-nls \
    --disable-werror \
    --quiet
make -j$(nproc) --quiet
make install --quiet
cd ..

# Download GCC
if [ ! -f "gcc-$GCC_VER.tar.gz" ]; then
    wget -q "https://ftp.gnu.org/gnu/gcc/gcc-$GCC_VER/gcc-$GCC_VER.tar.gz"
fi
tar -xf "gcc-$GCC_VER.tar.gz"
cd "gcc-$GCC_VER"
contrib/download_prerequisites --quiet
cd ..

# Build GCC (C only, freestanding)
mkdir -p build-gcc && cd build-gcc
"../gcc-$GCC_VER/configure" \
    --target=$TARGET \
    --prefix="$PREFIX" \
    --disable-nls \
    --enable-languages=c \
    --without-headers \
    --quiet
make -j$(nproc) all-gcc --quiet
make -j$(nproc) all-target-libgcc --quiet
make install-gcc --quiet
make install-target-libgcc --quiet
cd ..

echo "[*] Cross-compiler installed at $PREFIX/bin"
echo "    Add to PATH: export PATH=\"$PREFIX/bin:\$PATH\""

# Download Limine
echo "[*] Downloading Limine bootloader..."
LIMINE_VER=7.x-binary
cd "$(dirname "$0")/.."
if [ ! -d "limine_bin" ]; then
    git clone --depth=1 --branch "$LIMINE_VER" \
        https://github.com/limine-bootloader/limine.git limine_bin
fi

# Copy needed files
cp limine_bin/limine-bios.sys      limine/ 2>/dev/null || true
cp limine_bin/limine-bios-cd.bin   limine/ 2>/dev/null || true
cp limine_bin/limine-uefi-cd.bin   limine/ 2>/dev/null || true
cp limine_bin/BOOTX64.EFI          limine/ 2>/dev/null || true
cp limine_bin/limine                limine/ 2>/dev/null || true

# Get OVMF
echo "[*] Copying OVMF firmware..."
OVMF_PATH=$(find /usr -name "OVMF.fd" 2>/dev/null | head -1)
if [ -n "$OVMF_PATH" ]; then
    cp "$OVMF_PATH" .
    echo "    OVMF.fd copied from $OVMF_PATH"
else
    echo "    WARNING: OVMF.fd not found. Install ovmf package and copy manually."
fi

echo ""
echo "[*] Setup complete. Build with:"
echo "    export PATH=\"$PREFIX/bin:\$PATH\""
echo "    cd winkernel && make"
echo "    make run"
