# WinKERNEL — Build Progress

Mirrors the phased acceptance checklist in the spec (§10). Update as phases pass.

| Phase | Description | QEMU | [HW] | Status |
|-------|-------------|------|------|--------|
| 0 | Bootstrap — Limine boots kernel, banner over serial, no triple fault (BIOS + UEFI) | ☐ | — | **In progress** — build wiring done, not yet compiled/booted |
| 1 | Memory & Interrupts — PMM + paging + IDT + APIC timer tick | ☐ | — | Not started |
| 2 | Console & BSOD — framebuffer console + KeBugCheck renderer | ☐ | — | Not started |
| 3 | Process/Scheduler — 3+ kernel threads round-robin | ☐ | — | Not started |
| 4 | Drivers: storage + PCI — AHCI/NVMe/virtio-blk sector read | ☐ | ☐ | Not started |
| 5 | Filesystem — ext2 root mount, VFS read/write/readdir | ☐ | ☐ | Not started |
| 6 | Userspace + Shell — execve + ELF loader, shell built-ins | ☐ | ☐ | Not started |
| 7 | Network + USB — packet round-trip, xHCI HID enumerate | ☐ | ☐ | Not started |
| 8 | Hardening & real-hardware validation | — | ☐ | Not started |

## Current focus: Phase 0

**Done**
- Vendored Limine v7.13.3-binary under `winkernel/limine/` (was missing — ISO was unbootable).
- Reworked `Makefile`: builds the Limine host tool, real `bios-install`, parameterized OVMF, split `run-uefi` / `run-bios` targets.
- macOS build path documented (`docs/BUILD.md`) + `tools/setup-macos.sh`.
- `.gitignore` added.

**Blocked / next**
- First real compile + boot must happen on an `x86_64` host (Mac mini). The
  preparation above was done on an `aarch64` box with no `x86_64-elf` toolchain,
  so nothing has been compiled or booted yet.
- Once it builds: verify both boot paths reach the banner over serial with no
  triple fault, then tick Phase 0.
