/* WinKernel NTKRNL-X — Virtual Memory Manager (4-level paging) */

#include <kernel/mm.h>
#include <kernel/rtl.h>
#include <ntdef.h>
#include <limine.h>

/* ── Limine kernel address request ─────────────────────────────────────── */
__attribute__((used, section(".limine_requests")))
static volatile struct limine_kernel_address_request kaddr_request = {
    .id       = LIMINE_KERNEL_ADDRESS_REQUEST,
    .revision = 0,
    .response = NULL
};

/* ── External from pmm.c ────────────────────────────────────────────────── */
extern ULONG_PTR MmGetHhdmOffset(VOID);

/* ── Page table entry helpers ───────────────────────────────────────────── */
#define PML4_IDX(v)  (((v) >> 39) & 0x1FF)
#define PDPT_IDX(v)  (((v) >> 30) & 0x1FF)
#define PD_IDX(v)    (((v) >> 21) & 0x1FF)
#define PT_IDX(v)    (((v) >> 12) & 0x1FF)

typedef QWORD PTE;

/* ── Active PML4 (physical address) ────────────────────────────────────── */
static ULONG_PTR g_Pml4Phys = 0;

/* ── _GetOrCreateTable — get/allocate a page table ─────────────────────── */
static PTE* _GetOrCreateTable(PTE* Parent, DWORD Index, DWORD Flags) {
    ULONG_PTR hhdm = MmGetHhdmOffset();

    if (!(Parent[Index] & PAGE_PRESENT)) {
        ULONG_PTR phys = MmAllocatePhysicalPage();
        if (!phys) return NULL;
        RtlZeroMemory((PVOID)(phys + hhdm), PAGE_SIZE);
        Parent[Index] = phys | Flags | PAGE_PRESENT | PAGE_WRITE;
    }

    ULONG_PTR table_phys = Parent[Index] & ~0xFFFULL;
    return (PTE*)(table_phys + hhdm);
}

/* ── MmMapPage ──────────────────────────────────────────────────────────── */

NTSTATUS MmMapPage(ULONG_PTR Phys, ULONG_PTR Virt, DWORD Flags) {
    ULONG_PTR hhdm = MmGetHhdmOffset();
    PTE* pml4 = (PTE*)(g_Pml4Phys + hhdm);

    PTE* pdpt = _GetOrCreateTable(pml4, PML4_IDX(Virt), PAGE_WRITE | PAGE_USER);
    if (!pdpt) return STATUS_NO_MEMORY;

    PTE* pd = _GetOrCreateTable(pdpt, PDPT_IDX(Virt), PAGE_WRITE | PAGE_USER);
    if (!pd) return STATUS_NO_MEMORY;

    PTE* pt = _GetOrCreateTable(pd, PD_IDX(Virt), PAGE_WRITE | PAGE_USER);
    if (!pt) return STATUS_NO_MEMORY;

    pt[PT_IDX(Virt)] = (Phys & ~0xFFFULL) | (QWORD)Flags | PAGE_PRESENT;

    /* Invalidate TLB entry */
    __asm__ volatile ("invlpg (%0)" :: "r"(Virt) : "memory");

    return STATUS_SUCCESS;
}

/* ── MmUnmapPage ────────────────────────────────────────────────────────── */

VOID MmUnmapPage(ULONG_PTR Virt) {
    ULONG_PTR hhdm = MmGetHhdmOffset();
    PTE* pml4 = (PTE*)(g_Pml4Phys + hhdm);

    if (!(pml4[PML4_IDX(Virt)] & PAGE_PRESENT)) return;
    PTE* pdpt = (PTE*)((pml4[PML4_IDX(Virt)] & ~0xFFFULL) + hhdm);

    if (!(pdpt[PDPT_IDX(Virt)] & PAGE_PRESENT)) return;
    PTE* pd = (PTE*)((pdpt[PDPT_IDX(Virt)] & ~0xFFFULL) + hhdm);

    if (!(pd[PD_IDX(Virt)] & PAGE_PRESENT)) return;
    PTE* pt = (PTE*)((pd[PD_IDX(Virt)] & ~0xFFFULL) + hhdm);

    pt[PT_IDX(Virt)] = 0;
    __asm__ volatile ("invlpg (%0)" :: "r"(Virt) : "memory");
}

/* ── MmGetPhysicalAddress ───────────────────────────────────────────────── */

ULONG_PTR MmGetPhysicalAddress(ULONG_PTR Virt) {
    ULONG_PTR hhdm = MmGetHhdmOffset();
    PTE* pml4 = (PTE*)(g_Pml4Phys + hhdm);

    if (!(pml4[PML4_IDX(Virt)] & PAGE_PRESENT)) return 0;
    PTE* pdpt = (PTE*)((pml4[PML4_IDX(Virt)] & ~0xFFFULL) + hhdm);

    if (!(pdpt[PDPT_IDX(Virt)] & PAGE_PRESENT)) return 0;
    PTE* pd = (PTE*)((pdpt[PDPT_IDX(Virt)] & ~0xFFFULL) + hhdm);

    if (!(pd[PD_IDX(Virt)] & PAGE_PRESENT)) return 0;
    PTE* pt = (PTE*)((pd[PD_IDX(Virt)] & ~0xFFFULL) + hhdm);

    if (!(pt[PT_IDX(Virt)] & PAGE_PRESENT)) return 0;
    return (pt[PT_IDX(Virt)] & ~0xFFFULL) | (Virt & 0xFFF);
}

/* ── MmInitializePaging ─────────────────────────────────────────────────── */

NTSTATUS MmInitializePaging(VOID) {
    /* Read current CR3 — Limine already set up paging for us.
       We record the physical address of the active PML4 and use it. */
    ULONG_PTR cr3;
    __asm__ volatile ("mov %0, cr3" : "=r"(cr3));
    g_Pml4Phys = cr3 & ~0xFFFULL;

    /* The kernel is already mapped by Limine at KERNEL_VIRT_BASE.
       HHDM is already active. We just record the state and return. */
    return STATUS_SUCCESS;
}
