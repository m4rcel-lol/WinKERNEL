/* WinKernel NTKRNL-X — Physical Memory Manager (bitmap allocator) */

#include <kernel/mm.h>
#include <kernel/rtl.h>
#include <ntdef.h>
#include <limine.h>

/* ── Limine memory map request ──────────────────────────────────────────── */
__attribute__((used, section(".limine_requests")))
static volatile struct limine_memmap_request memmap_request = {
    .id       = LIMINE_MEMMAP_REQUEST,
    .revision = 0,
    .response = NULL
};

__attribute__((used, section(".limine_requests")))
static volatile struct limine_hhdm_request hhdm_request = {
    .id       = LIMINE_HHDM_REQUEST,
    .revision = 0,
    .response = NULL
};

/* ── PMM state ──────────────────────────────────────────────────────────── */
#define PMM_BITMAP_MAX_FRAMES   (512 * 1024)   /* supports up to 2 GB */

static BYTE     g_Bitmap[PMM_BITMAP_MAX_FRAMES / 8];
static QWORD    g_TotalFrames   = 0;
static QWORD    g_FreeFrames    = 0;
static QWORD    g_TotalBytes    = 0;
static ULONG_PTR g_HhdmOffset  = 0;

/* ── Bitmap helpers ─────────────────────────────────────────────────────── */

static VOID _BitmapSet(QWORD Frame) {
    g_Bitmap[Frame / 8] |= (BYTE)(1 << (Frame % 8));
}

static VOID _BitmapClear(QWORD Frame) {
    g_Bitmap[Frame / 8] &= (BYTE)~(1 << (Frame % 8));
}

static BOOL _BitmapTest(QWORD Frame) {
    return (g_Bitmap[Frame / 8] >> (Frame % 8)) & 1;
}

/* ── MmInitializePhysicalMemory ─────────────────────────────────────────── */

NTSTATUS MmInitializePhysicalMemory(VOID) {
    if (!memmap_request.response) return STATUS_FAILURE;

    /* Get HHDM offset */
    if (hhdm_request.response) {
        g_HhdmOffset = (ULONG_PTR)hhdm_request.response->offset;
    } else {
        g_HhdmOffset = HHDM_OFFSET;
    }

    /* Mark all frames as used initially */
    RtlFillMemory(g_Bitmap, sizeof(g_Bitmap), 0xFF);

    struct limine_memmap_response* resp = memmap_request.response;

    /* First pass: find total memory */
    for (QWORD i = 0; i < resp->entry_count; i++) {
        struct limine_memmap_entry* e = resp->entries[i];
        QWORD end_frame = (e->base + e->length) / PAGE_SIZE;
        if (end_frame > g_TotalFrames) g_TotalFrames = end_frame;
        if (g_TotalFrames > PMM_BITMAP_MAX_FRAMES)
            g_TotalFrames = PMM_BITMAP_MAX_FRAMES;
        g_TotalBytes += e->length;
    }

    /* Second pass: free usable frames */
    for (QWORD i = 0; i < resp->entry_count; i++) {
        struct limine_memmap_entry* e = resp->entries[i];
        if (e->type != LIMINE_MEMMAP_USABLE) continue;

        QWORD base_frame = e->base / PAGE_SIZE;
        QWORD frame_count = e->length / PAGE_SIZE;

        for (QWORD f = base_frame; f < base_frame + frame_count; f++) {
            if (f >= PMM_BITMAP_MAX_FRAMES) break;
            _BitmapClear(f);
            g_FreeFrames++;
        }
    }

    /* Frame 0 is always reserved (null page) */
    if (!_BitmapTest(0)) {
        _BitmapSet(0);
        g_FreeFrames--;
    }

    return STATUS_SUCCESS;
}

/* ── MmAllocatePhysicalPage ─────────────────────────────────────────────── */

ULONG_PTR MmAllocatePhysicalPage(VOID) {
    for (QWORD f = 1; f < g_TotalFrames; f++) {
        if (!_BitmapTest(f)) {
            _BitmapSet(f);
            g_FreeFrames--;
            return (ULONG_PTR)(f * PAGE_SIZE);
        }
    }
    return 0;   /* Out of memory */
}

/* ── MmFreePhysicalPage ─────────────────────────────────────────────────── */

VOID MmFreePhysicalPage(ULONG_PTR PhysAddr) {
    QWORD frame = PhysAddr / PAGE_SIZE;
    if (frame == 0 || frame >= g_TotalFrames) return;
    if (_BitmapTest(frame)) {
        _BitmapClear(frame);
        g_FreeFrames++;
    }
}

/* ── Accessors ──────────────────────────────────────────────────────────── */

QWORD MmGetTotalBytes(VOID)     { return g_TotalBytes; }
QWORD MmGetAvailableBytes(VOID) { return g_FreeFrames * PAGE_SIZE; }
ULONG_PTR MmGetHhdmOffset(VOID) { return g_HhdmOffset; }
