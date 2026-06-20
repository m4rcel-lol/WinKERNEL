# WinKERNEL — Build Instructions

This document covers a reproducible build from a clean checkout to a bootable
hybrid ISO, testable in QEMU on **macOS** and **Linux**.

> **Toolchain note.** WinKERNEL is an `x86_64` freestanding kernel built with an
> `x86_64-elf` cross-compiler. You **cannot** build it with Apple's `clang` or a
> native ARM/`aarch64` GCC — those don't emit `x86_64` ELF objects. On macOS the
> cross-compiler comes from Homebrew (`x86_64-elf-gcc`).

---

## 1. Building on macOS (Mac mini — Intel **or** Apple Silicon)

Both Intel and Apple Silicon Macs work. On Apple Silicon, QEMU runs the x86_64
guest under full software emulation (TCG) — slower than a real x86 host, but
perfectly fine for bring-up testing.

### 1.1 Install the toolchain

```sh
# 1. Xcode Command Line Tools (gives you make, git, clang for the host tool)
xcode-select --install      # skip if already installed

# 2. Homebrew (skip if you already have it) — https://brew.sh
/bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"

# 3. The actual build dependencies
brew install x86_64-elf-gcc nasm xorriso qemu
```

`x86_64-elf-gcc` automatically pulls in `x86_64-elf-binutils` (which provides
`x86_64-elf-ld`). Verify everything is on your PATH:

```sh
x86_64-elf-gcc --version
x86_64-elf-ld  --version
nasm --version
xorriso --version
qemu-system-x86_64 --version
```

If `x86_64-elf-gcc` is "not found" after install, your shell may not have
Homebrew's bin on PATH. On Apple Silicon add `eval "$(/opt/homebrew/bin/brew shellenv)"`
to your `~/.zshrc`; on Intel it's `/usr/local/bin`.

> A convenience script that runs all of the above lives at
> [`tools/setup-macos.sh`](../tools/setup-macos.sh).

### 1.2 Get the UEFI firmware (for the UEFI boot path)

Homebrew's QEMU ships an EDK2/OVMF firmware image. Copy it into the build dir as
`OVMF.fd` so the Makefile finds it by default:

```sh
cd winkernel
cp "$(brew --prefix qemu)/share/qemu/edk2-x86_64-code.fd" OVMF.fd
```

(You can skip this if you only test the BIOS path. You can also point the build
at the firmware without copying: `make run-uefi OVMF="$(brew --prefix qemu)/share/qemu/edk2-x86_64-code.fd"`.)

### 1.3 Build and run

```sh
cd winkernel
make                # produces winkernel.iso (and builds the vendored Limine host tool)

make run-uefi       # boot under UEFI (OVMF) — Phase 0 UEFI path
make run-bios       # boot under legacy BIOS (QEMU SeaBIOS) — Phase 0 BIOS path
```

Serial output appears in your terminal (QEMU is launched with `-serial stdio`).
To quit QEMU: `Ctrl-A` then `X`.

---

## 2. Building on Linux (x86_64 host)

```sh
cd winkernel
./tools/setup.sh          # installs deps + builds an x86_64-elf cross-compiler + Limine
export PATH="$HOME/opt/cross/bin:$PATH"
make
make run-uefi             # uses OVMF.fd copied by setup.sh
make run-bios
```

---

## 3. Phase 0 acceptance check (§10 of the spec)

Phase 0 passes when **both** boot paths reach the kernel and print the WinKERNEL
banner + stage status lines over serial with **no triple fault / reboot loop**:

```sh
make run-uefi    # UEFI path reaches the banner and halts cleanly
make run-bios    # BIOS path reaches the banner and halts cleanly
```

`-no-reboot -no-shutdown` are set, so a triple fault leaves QEMU stopped (it will
not silently reboot), making failures obvious.

---

## 4. Bootloader

Limine is **vendored** under `winkernel/limine/` (pinned to **v7.13.3-binary**,
upstream commit `ca3a5b20d2501e3819983c5cf838c70c30ee532a`). The binary blobs
(`limine-bios.sys`, `limine-bios-cd.bin`, `limine-uefi-cd.bin`, `BOOTX64.EFI`)
are committed; the host `limine` utility is compiled locally by `make` from the
committed `limine.c`. See `winkernel/limine/VERSION` and `LIMINE-LICENSE`.

---

## 5. Known status

> ⚠️ The build wiring (vendored Limine, Makefile, this doc) was prepared on an
> `aarch64` host where the `x86_64-elf` toolchain is unavailable, so **the build
> has not yet been compiled/booted end-to-end**. The Mac mini is the first real
> compile. If `make` errors, capture the output — fixing the first real build is
> the immediate next step.
