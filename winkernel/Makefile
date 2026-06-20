# WinKernel NTKRNL-X — Build System
# Requires: x86_64-elf-gcc, nasm, x86_64-elf-ld, xorriso, limine

# ── Toolchain ────────────────────────────────────────────────────────────────
CC      := x86_64-elf-gcc
LD      := x86_64-elf-ld
NASM    := nasm
XORRISO := xorriso

# ── Directories ──────────────────────────────────────────────────────────────
KERNEL_DIR  := kernel
SRC_DIR     := $(KERNEL_DIR)/src
INC_DIR     := $(KERNEL_DIR)/include
BUILD_DIR   := build
ISO_DIR     := $(BUILD_DIR)/iso_root

# ── Output ───────────────────────────────────────────────────────────────────
KERNEL_ELF  := $(BUILD_DIR)/kernel.elf
ISO_FILE    := winkernel.iso

# ── Firmware ──────────────────────────────────────────────────────────────────
# Path to a UEFI firmware image for QEMU's UEFI boot path. Override on the CLI,
# e.g.  make run-uefi OVMF=/opt/homebrew/share/qemu/edk2-x86_64-code.fd
# See docs/BUILD.md for how to obtain this on macOS / Linux.
OVMF ?= OVMF.fd

# ── Vendored Limine host tool (built from limine/limine.c) ────────────────────
LIMINE_TOOL := limine/limine

# ── Compiler flags ───────────────────────────────────────────────────────────
CFLAGS := \
    -std=c17 \
    -ffreestanding \
    -fno-stack-protector \
    -fno-stack-check \
    -fno-PIC \
    -fno-pie \
    -m64 \
    -march=x86-64 \
    -mno-red-zone \
    -mno-mmx \
    -mno-sse \
    -mno-sse2 \
    -mcmodel=kernel \
    -Wall \
    -Wextra \
    -Werror \
    -O2 \
    -I$(INC_DIR) \
    -I./limine

# ── Linker flags ─────────────────────────────────────────────────────────────
LDFLAGS := \
    -nostdlib \
    -static \
    -z max-page-size=0x1000 \
    -T $(KERNEL_DIR)/linker.ld

# ── NASM flags ────────────────────────────────────────────────────────────────
NASMFLAGS := -f elf64

# ── Source files ──────────────────────────────────────────────────────────────
C_SOURCES := \
    $(SRC_DIR)/main.c \
    $(SRC_DIR)/terminal.c \
    $(SRC_DIR)/bsod.c \
    $(SRC_DIR)/shell.c \
    $(SRC_DIR)/hal/hal.c \
    $(SRC_DIR)/hal/pic.c \
    $(SRC_DIR)/hal/timer.c \
    $(SRC_DIR)/hal/pci.c \
    $(SRC_DIR)/ke/gdt.c \
    $(SRC_DIR)/ke/idt.c \
    $(SRC_DIR)/ke/isr.c \
    $(SRC_DIR)/mm/pmm.c \
    $(SRC_DIR)/mm/vmm.c \
    $(SRC_DIR)/mm/heap.c \
    $(SRC_DIR)/ob/ob.c \
    $(SRC_DIR)/io/io.c \
    $(SRC_DIR)/io/keyboard.c \
    $(SRC_DIR)/io/serial.c \
    $(SRC_DIR)/ps/ps.c \
    $(SRC_DIR)/net/rtl8139.c \
    $(SRC_DIR)/gfx/gfx.c \
    $(SRC_DIR)/rtl/string.c \
    $(SRC_DIR)/rtl/printf.c

ASM_SOURCES := \
    $(SRC_DIR)/boot/entry.asm \
    $(SRC_DIR)/ke/gdt.asm \
    $(SRC_DIR)/ke/idt.asm

# ── Object files ──────────────────────────────────────────────────────────────
C_OBJECTS   := $(patsubst $(SRC_DIR)/%.c,   $(BUILD_DIR)/%.o, $(C_SOURCES))
ASM_OBJECTS := $(patsubst $(SRC_DIR)/%.asm, $(BUILD_DIR)/%.asm.o, $(ASM_SOURCES))
ALL_OBJECTS := $(ASM_OBJECTS) $(C_OBJECTS)

# ── QEMU flags ────────────────────────────────────────────────────────────────
# Common flags shared by both boot paths.
QEMU_COMMON := \
    -m 256M \
    -cpu qemu64 \
    -M q35 \
    -cdrom $(ISO_FILE) \
    -boot d \
    -serial stdio \
    -no-reboot \
    -no-shutdown

# ── Default target ────────────────────────────────────────────────────────────
.PHONY: all clean run run-uefi run-bios iso limine-tool

all: $(ISO_FILE)

# ── Compile C sources ─────────────────────────────────────────────────────────
$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

# ── Assemble .asm sources ─────────────────────────────────────────────────────
$(BUILD_DIR)/%.asm.o: $(SRC_DIR)/%.asm
	@mkdir -p $(dir $@)
	$(NASM) $(NASMFLAGS) $< -o $@

# ── Link kernel ELF ──────────────────────────────────────────────────────────
$(KERNEL_ELF): $(ALL_OBJECTS)
	@mkdir -p $(BUILD_DIR)
	$(LD) $(LDFLAGS) $(ALL_OBJECTS) -o $@

# ── Build the vendored Limine host tool (used for the BIOS hybrid install) ─────
$(LIMINE_TOOL): limine/limine.c
	$(MAKE) -C limine

limine-tool: $(LIMINE_TOOL)

# ── Build ISO ────────────────────────────────────────────────────────────────
$(ISO_FILE): $(KERNEL_ELF) $(LIMINE_TOOL)
	@mkdir -p $(ISO_DIR)/boot/limine $(ISO_DIR)/EFI/BOOT
	cp $(KERNEL_ELF)               $(ISO_DIR)/kernel.elf
	cp limine.cfg                  $(ISO_DIR)/boot/limine/limine.cfg
	cp limine/limine-bios.sys      $(ISO_DIR)/boot/limine/
	cp limine/limine-bios-cd.bin   $(ISO_DIR)/boot/limine/
	cp limine/limine-uefi-cd.bin   $(ISO_DIR)/boot/limine/
	cp limine/BOOTX64.EFI          $(ISO_DIR)/EFI/BOOT/
	$(XORRISO) -as mkisofs \
		-b boot/limine/limine-bios-cd.bin \
		-no-emul-boot \
		-boot-load-size 4 \
		-boot-info-table \
		--efi-boot boot/limine/limine-uefi-cd.bin \
		-efi-boot-part \
		--efi-boot-image \
		--protective-msdos-label \
		$(ISO_DIR) \
		-o $(ISO_FILE)
	# Deploy the Limine BIOS stage + hybrid MBR so the ISO also boots when
	# dd'd to a USB stick on a legacy-BIOS machine (Phase 8 [HW]).
	$(LIMINE_TOOL) bios-install $(ISO_FILE)

# ── Run in QEMU: UEFI path (needs OVMF firmware — see docs/BUILD.md) ───────────
run-uefi: $(ISO_FILE)
	qemu-system-x86_64 $(QEMU_COMMON) \
		-drive if=pflash,format=raw,readonly=on,file=$(OVMF)

# ── Run in QEMU: legacy-BIOS path (QEMU's built-in SeaBIOS, no OVMF needed) ────
run-bios: $(ISO_FILE)
	qemu-system-x86_64 $(QEMU_COMMON)

# Default `run` uses the UEFI path.
run: run-uefi

# ── Clean ─────────────────────────────────────────────────────────────────────
clean:
	rm -rf $(BUILD_DIR) $(ISO_FILE)
	$(MAKE) -C limine clean 2>/dev/null || true
