/* WinKernel NTKRNL-X — Interrupt Descriptor Table setup */

#include <kernel/ke.h>
#include <kernel/rtl.h>
#include <ntdef.h>

/* ── IDT storage ────────────────────────────────────────────────────────── */
static IDT_ENTRY   g_Idt[IDT_ENTRIES];
static IDT_POINTER g_IdtPtr;

/* ── Stub table from idt.asm ────────────────────────────────────────────── */
extern ULONG_PTR _IsrStubTable[IDT_ENTRIES];

/* ── External flush ─────────────────────────────────────────────────────── */
extern VOID _IdtFlush(QWORD IdtPtr);

/* ── _SetIdtGate ────────────────────────────────────────────────────────── */
static VOID _SetIdtGate(DWORD Vector, ULONG_PTR Handler,
                        WORD Selector, BYTE TypeAttr, BYTE IST) {
    g_Idt[Vector].OffsetLow    = (WORD)(Handler & 0xFFFF);
    g_Idt[Vector].Selector     = Selector;
    g_Idt[Vector].IST          = IST;
    g_Idt[Vector].TypeAttr     = TypeAttr;
    g_Idt[Vector].OffsetMiddle = (WORD)((Handler >> 16) & 0xFFFF);
    g_Idt[Vector].OffsetHigh   = (DWORD)((Handler >> 32) & 0xFFFFFFFF);
    g_Idt[Vector].Reserved     = 0;
}

/* ── KeInitializeIdt ────────────────────────────────────────────────────── */

NTSTATUS KeInitializeIdt(VOID) {
    RtlZeroMemory(g_Idt, sizeof(g_Idt));

    for (DWORD i = 0; i < IDT_ENTRIES; i++) {
        /* 0x8E = Present | DPL=0 | Interrupt Gate (64-bit) */
        _SetIdtGate(i, _IsrStubTable[i], GDT_KERNEL_CODE, 0x8E, 0);
    }

    /* Double fault (#DF = vector 8) uses IST1 for a known-good stack */
    _SetIdtGate(8, _IsrStubTable[8], GDT_KERNEL_CODE, 0x8E, 1);

    g_IdtPtr.Limit = (WORD)(sizeof(g_Idt) - 1);
    g_IdtPtr.Base  = (QWORD)(ULONG_PTR)g_Idt;

    _IdtFlush((QWORD)(ULONG_PTR)&g_IdtPtr);

    return STATUS_SUCCESS;
}
