/* WinKernel NTKRNL-X — Kernel pool (free-list heap, 16-byte aligned) */

#include <kernel/mm.h>
#include <kernel/bsod.h>
#include <kernel/rtl.h>
#include <ntdef.h>

/* ── Block header/footer magic ──────────────────────────────────────────── */
#define HEAP_MAGIC_FREE     0xDEADBEEF
#define HEAP_MAGIC_USED     0xC0FFEE00
#define HEAP_FOOTER_MAGIC   0xFEEDFACE
#define HEAP_ALIGN          16

/* ── Block header ───────────────────────────────────────────────────────── */
typedef struct _HEAP_BLOCK {
    DWORD               Magic;
    SIZE_T              Size;       /* usable bytes (excludes header+footer) */
    BOOL                Free;
    struct _HEAP_BLOCK* Next;
    struct _HEAP_BLOCK* Prev;
} HEAP_BLOCK;

/* ── Block footer ───────────────────────────────────────────────────────── */
typedef struct _HEAP_FOOTER {
    DWORD Magic;
} HEAP_FOOTER;

#define BLOCK_OVERHEAD  (sizeof(HEAP_BLOCK) + sizeof(HEAP_FOOTER))

/* ── Heap storage (8 MB static pool) ───────────────────────────────────── */
static BYTE         g_HeapStorage[KERNEL_POOL_SIZE] __attribute__((aligned(16)));
static HEAP_BLOCK*  g_HeapHead = NULL;
static SIZE_T       g_UsedBytes = 0;

/* ── _Footer — get footer pointer for a block ───────────────────────────── */
static HEAP_FOOTER* _Footer(HEAP_BLOCK* Block) {
    return (HEAP_FOOTER*)((BYTE*)Block + sizeof(HEAP_BLOCK) + Block->Size);
}

/* ── _ValidateBlock ─────────────────────────────────────────────────────── */
static VOID _ValidateBlock(HEAP_BLOCK* Block) {
    if (Block->Magic != HEAP_MAGIC_FREE && Block->Magic != HEAP_MAGIC_USED) {
        KeBugCheckEx(STOP_HEAP_CORRUPTION, (ULONG_PTR)Block, Block->Magic, 0, 0);
    }
    HEAP_FOOTER* foot = _Footer(Block);
    if (foot->Magic != HEAP_FOOTER_MAGIC) {
        KeBugCheckEx(STOP_HEAP_CORRUPTION, (ULONG_PTR)Block, foot->Magic, 1, 0);
    }
}

/* ── ExInitializeHeap ───────────────────────────────────────────────────── */

NTSTATUS ExInitializeHeap(VOID) {
    g_HeapHead = (HEAP_BLOCK*)g_HeapStorage;
    g_HeapHead->Magic = HEAP_MAGIC_FREE;
    g_HeapHead->Size  = KERNEL_POOL_SIZE - BLOCK_OVERHEAD;
    g_HeapHead->Free  = TRUE;
    g_HeapHead->Next  = NULL;
    g_HeapHead->Prev  = NULL;

    HEAP_FOOTER* foot = _Footer(g_HeapHead);
    foot->Magic = HEAP_FOOTER_MAGIC;

    g_UsedBytes = 0;
    return STATUS_SUCCESS;
}

/* ── ExAllocatePool ─────────────────────────────────────────────────────── */

PVOID ExAllocatePool(SIZE_T Size) {
    if (Size == 0) return NULL;

    /* Align size to HEAP_ALIGN */
    Size = ALIGN_UP(Size, HEAP_ALIGN);

    HEAP_BLOCK* cur = g_HeapHead;
    while (cur) {
        _ValidateBlock(cur);

        if (cur->Free && cur->Size >= Size) {
            /* Split if there's enough room for a new free block */
            if (cur->Size >= Size + BLOCK_OVERHEAD + HEAP_ALIGN) {
                HEAP_BLOCK* next = (HEAP_BLOCK*)((BYTE*)cur + sizeof(HEAP_BLOCK) + Size + sizeof(HEAP_FOOTER));
                next->Magic = HEAP_MAGIC_FREE;
                next->Size  = cur->Size - Size - BLOCK_OVERHEAD;
                next->Free  = TRUE;
                next->Next  = cur->Next;
                next->Prev  = cur;

                HEAP_FOOTER* nfoot = _Footer(next);
                nfoot->Magic = HEAP_FOOTER_MAGIC;

                if (cur->Next) cur->Next->Prev = next;
                cur->Next = next;
                cur->Size = Size;

                /* Update current footer */
                HEAP_FOOTER* cfoot = _Footer(cur);
                cfoot->Magic = HEAP_FOOTER_MAGIC;
            }

            cur->Free  = FALSE;
            cur->Magic = HEAP_MAGIC_USED;
            g_UsedBytes += cur->Size;

            return (PVOID)((BYTE*)cur + sizeof(HEAP_BLOCK));
        }

        cur = cur->Next;
    }

    return NULL;    /* Out of pool memory */
}

/* ── ExAllocatePoolZero ─────────────────────────────────────────────────── */

PVOID ExAllocatePoolZero(SIZE_T Size) {
    PVOID ptr = ExAllocatePool(Size);
    if (ptr) RtlZeroMemory(ptr, Size);
    return ptr;
}

/* ── ExFreePool ─────────────────────────────────────────────────────────── */

VOID ExFreePool(PVOID Ptr) {
    if (!Ptr) return;

    HEAP_BLOCK* block = (HEAP_BLOCK*)((BYTE*)Ptr - sizeof(HEAP_BLOCK));
    _ValidateBlock(block);

    if (block->Free) {
        KeBugCheckEx(STOP_HEAP_CORRUPTION, (ULONG_PTR)Ptr, 0xDEAD0001, 2, 0);
    }

    block->Free  = TRUE;
    block->Magic = HEAP_MAGIC_FREE;
    g_UsedBytes -= block->Size;

    /* Coalesce with next block */
    if (block->Next && block->Next->Free) {
        _ValidateBlock(block->Next);
        block->Size += BLOCK_OVERHEAD + block->Next->Size;
        block->Next  = block->Next->Next;
        if (block->Next) block->Next->Prev = block;

        HEAP_FOOTER* foot = _Footer(block);
        foot->Magic = HEAP_FOOTER_MAGIC;
    }

    /* Coalesce with previous block */
    if (block->Prev && block->Prev->Free) {
        _ValidateBlock(block->Prev);
        block->Prev->Size += BLOCK_OVERHEAD + block->Size;
        block->Prev->Next  = block->Next;
        if (block->Next) block->Next->Prev = block->Prev;

        HEAP_FOOTER* foot = _Footer(block->Prev);
        foot->Magic = HEAP_FOOTER_MAGIC;
    }
}

/* ── ExReallocatePool ───────────────────────────────────────────────────── */

PVOID ExReallocatePool(PVOID Ptr, SIZE_T NewSize) {
    if (!Ptr)    return ExAllocatePool(NewSize);
    if (!NewSize) { ExFreePool(Ptr); return NULL; }

    HEAP_BLOCK* block = (HEAP_BLOCK*)((BYTE*)Ptr - sizeof(HEAP_BLOCK));
    _ValidateBlock(block);

    if (block->Size >= NewSize) return Ptr;

    PVOID newptr = ExAllocatePool(NewSize);
    if (!newptr) return NULL;

    RtlCopyMemory(newptr, Ptr, block->Size);
    ExFreePool(Ptr);
    return newptr;
}

/* ── Accessors ──────────────────────────────────────────────────────────── */

SIZE_T ExGetPoolUsed(VOID) { return g_UsedBytes; }
SIZE_T ExGetPoolFree(VOID) { return KERNEL_POOL_SIZE - g_UsedBytes - BLOCK_OVERHEAD; }
