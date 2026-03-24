/* Limine v5 boot protocol header — original declarations, no external code */
#pragma once

#include <stdint.h>

/* ── Magic numbers ──────────────────────────────────────────────────────── */
#define LIMINE_COMMON_MAGIC     0xc7b1dd30df4c8b88, 0x0a82e883a194f07b

/* ── Request/response IDs ───────────────────────────────────────────────── */
#define LIMINE_FRAMEBUFFER_REQUEST  { LIMINE_COMMON_MAGIC, 0x9d5827dcd881dd75, 0xa3148604f6fab11b }
#define LIMINE_MEMMAP_REQUEST       { LIMINE_COMMON_MAGIC, 0x67cf3d9d378a806f, 0xe304acdfc50c3c62 }
#define LIMINE_HHDM_REQUEST         { LIMINE_COMMON_MAGIC, 0x48dcf1cb8ad2b852, 0x63984e959a98244b }
#define LIMINE_KERNEL_ADDRESS_REQUEST { LIMINE_COMMON_MAGIC, 0x71ba76863cc55f63, 0xb2644a48c516a487 }
#define LIMINE_BOOTLOADER_INFO_REQUEST { LIMINE_COMMON_MAGIC, 0xf55038d8e2a1202f, 0x279426fcf5f59740 }

/* ── Revision ───────────────────────────────────────────────────────────── */
#define LIMINE_REQUEST_REVISION     0

/* ── Framebuffer ────────────────────────────────────────────────────────── */
#define LIMINE_FRAMEBUFFER_RGB      1

struct limine_framebuffer {
    void*       address;
    uint64_t    width;
    uint64_t    height;
    uint64_t    pitch;
    uint16_t    bpp;
    uint8_t     memory_model;
    uint8_t     red_mask_size;
    uint8_t     red_mask_shift;
    uint8_t     green_mask_size;
    uint8_t     green_mask_shift;
    uint8_t     blue_mask_size;
    uint8_t     blue_mask_shift;
    uint8_t     unused[7];
    uint64_t    edid_size;
    void*       edid;
    uint64_t    mode_count;
    void**      modes;
};

struct limine_framebuffer_response {
    uint64_t                    revision;
    uint64_t                    framebuffer_count;
    struct limine_framebuffer** framebuffers;
};

struct limine_framebuffer_request {
    uint64_t                            id[4];
    uint64_t                            revision;
    struct limine_framebuffer_response* response;
};

/* ── Memory map ─────────────────────────────────────────────────────────── */
#define LIMINE_MEMMAP_USABLE                0
#define LIMINE_MEMMAP_RESERVED              1
#define LIMINE_MEMMAP_ACPI_RECLAIMABLE      2
#define LIMINE_MEMMAP_ACPI_NVS              3
#define LIMINE_MEMMAP_BAD_MEMORY            4
#define LIMINE_MEMMAP_BOOTLOADER_RECLAIMABLE 5
#define LIMINE_MEMMAP_KERNEL_AND_MODULES    6
#define LIMINE_MEMMAP_FRAMEBUFFER           7

struct limine_memmap_entry {
    uint64_t    base;
    uint64_t    length;
    uint64_t    type;
};

struct limine_memmap_response {
    uint64_t                    revision;
    uint64_t                    entry_count;
    struct limine_memmap_entry** entries;
};

struct limine_memmap_request {
    uint64_t                        id[4];
    uint64_t                        revision;
    struct limine_memmap_response*  response;
};

/* ── HHDM ───────────────────────────────────────────────────────────────── */
struct limine_hhdm_response {
    uint64_t    revision;
    uint64_t    offset;
};

struct limine_hhdm_request {
    uint64_t                    id[4];
    uint64_t                    revision;
    struct limine_hhdm_response* response;
};

/* ── Kernel address ─────────────────────────────────────────────────────── */
struct limine_kernel_address_response {
    uint64_t    revision;
    uint64_t    physical_base;
    uint64_t    virtual_base;
};

struct limine_kernel_address_request {
    uint64_t                            id[4];
    uint64_t                            revision;
    struct limine_kernel_address_response* response;
};

/* ── Bootloader info ────────────────────────────────────────────────────── */
struct limine_bootloader_info_response {
    uint64_t    revision;
    char*       name;
    char*       version;
};

struct limine_bootloader_info_request {
    uint64_t                                id[4];
    uint64_t                                revision;
    struct limine_bootloader_info_response* response;
};
