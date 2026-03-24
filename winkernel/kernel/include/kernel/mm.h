#pragma once

#include <ntdef.h>
#include <ntstatus.h>

/* ── Page size / alignment ──────────────────────────────────────────────── */
#define PAGE_SIZE           0x1000ULL
#define PAGE_SHIFT          12
#define PAGE_MASK           (~(PAGE_SIZE - 1))

/* ── Page flags ─────────────────────────────────────────────────────────── */
#define PAGE_PRESENT        (1ULL << 0)
#define PAGE_WRITE          (1ULL << 1)
#define PAGE_USER           (1ULL << 2)
#define PAGE_WRITETHROUGH   (1ULL << 3)
#define PAGE_NOCACHE        (1ULL << 4)
#define PAGE_ACCESSED       (1ULL << 5)
#define PAGE_DIRTY          (1ULL << 6)
#define PAGE_HUGE           (1ULL << 7)
#define PAGE_GLOBAL         (1ULL << 8)
#define PAGE_NX             (1ULL << 63)

/* ── HHDM offset (Limine maps all physical RAM here) ───────────────────── */
#define HHDM_OFFSET         0xFFFF800000000000ULL

/* ── Kernel virtual base ────────────────────────────────────────────────── */
#define KERNEL_VIRT_BASE    0xFFFFFFFF80000000ULL

/* ── Physical ↔ virtual (Limine HHDM only) ────────────────────────────────
   All physical RAM is identity-mapped at HHDM offset. This macro works only
   for addresses in that direct map, not for kernel .text/.bss (high half). */
#define PHYS_TO_HHDM(p)     ((PVOID)((ULONG_PTR)(p) + HHDM_OFFSET))
/* Kernel VA → PA: use MmGetPhysicalAddress() after paging is initialized. */

/* ── PMM ────────────────────────────────────────────────────────────────── */
NTSTATUS    MmInitializePhysicalMemory(VOID);
ULONG_PTR   MmAllocatePhysicalPage(VOID);
VOID        MmFreePhysicalPage(ULONG_PTR PhysAddr);
QWORD       MmGetTotalBytes(VOID);
QWORD       MmGetAvailableBytes(VOID);

/* ── VMM ────────────────────────────────────────────────────────────────── */
NTSTATUS    MmInitializePaging(VOID);
NTSTATUS    MmMapPage(ULONG_PTR Phys, ULONG_PTR Virt, DWORD Flags);
VOID        MmUnmapPage(ULONG_PTR Virt);
ULONG_PTR   MmGetPhysicalAddress(ULONG_PTR Virt);

/* ── Kernel pool (heap) ─────────────────────────────────────────────────── */
#define KERNEL_POOL_SIZE    (8ULL * 1024 * 1024)   /* 8 MB */

NTSTATUS    ExInitializeHeap(VOID);
PVOID       ExAllocatePool(SIZE_T Size);
PVOID       ExAllocatePoolZero(SIZE_T Size);
VOID        ExFreePool(PVOID Ptr);
PVOID       ExReallocatePool(PVOID Ptr, SIZE_T NewSize);
SIZE_T      ExGetPoolUsed(VOID);
SIZE_T      ExGetPoolFree(VOID);
