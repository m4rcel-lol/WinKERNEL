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
QEMU_FLAGS := \
    -m 256M \
    -cpu qemu64 \
    -M q35 \
    -drive if=pflash,format=raw,readonly=on,file=OVMF.fd \
    -cdrom $(ISO_FILE) \
    -boot d \
    -serial stdio \
    -no-reboot \
    -no-shutdown

# ── Default target ────────────────────────────────────────────────────────────
.PHONY: all clean run iso

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

# ── Build ISO ────────────────────────────────────────────────────────────────
$(ISO_FILE): $(KERNEL_ELF)
	@mkdir -p $(ISO_DIR)/boot/limine
	cp $(KERNEL_ELF)    $(ISO_DIR)/kernel.elf
	cp limine.cfg       $(ISO_DIR)/boot/limine/limine.cfg
	cp limine/limine-bios.sys      $(ISO_DIR)/boot/limine/ 2>/dev/null || true
	cp limine/limine-bios-cd.bin   $(ISO_DIR)/boot/limine/ 2>/dev/null || true
	cp limine/limine-uefi-cd.bin   $(ISO_DIR)/boot/limine/ 2>/dev/null || true
	cp limine/BOOTX64.EFI          $(ISO_DIR)/EFI/BOOT/    2>/dev/null || (mkdir -p $(ISO_DIR)/EFI/BOOT && cp limine/BOOTX64.EFI $(ISO_DIR)/EFI/BOOT/)
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
	./limine/limine bios-install $(ISO_FILE) 2>/dev/null || true

# ── Run in QEMU ───────────────────────────────────────────────────────────────
run: $(ISO_FILE)
	qemu-system-x86_64 $(QEMU_FLAGS)

# ── Clean ─────────────────────────────────────────────────────────────────────
clean:
	rm -rf $(BUILD_DIR) $(ISO_FILE)
