/* WinKernel NTKRNL-X — IDT setup */

#include <kernel/ke.h>
#include <kernel/rtl.h>
#include <ntdef.h>

static IDT_ENTRY   g_Idt[IDT_ENTRIES];
static IDT_POINTER g_IdtPtr;

/* Stub table lives in idt.asm (.data section) */
extern void* _IsrStubTable[IDT_ENTRIES];

extern VOID _IdtFlush(ULONG_PTR IdtPtr);

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

NTSTATUS KeInitializeIdt(VOID) {
    RtlZeroMemory(g_Idt, sizeof(g_Idt));

    for (DWORD i = 0; i < IDT_ENTRIES; i++) {
        _SetIdtGate(i, (ULONG_PTR)_IsrStubTable[i], GDT_KERNEL_CODE, 0x8E, 0);
    }

    /* Double fault uses IST1 (dedicated stack, set in TSS) */
    _SetIdtGate(8, (ULONG_PTR)_IsrStubTable[8], GDT_KERNEL_CODE, 0x8E, 1);

    g_IdtPtr.Limit = (WORD)(sizeof(g_Idt) - 1);
    g_IdtPtr.Base  = (QWORD)(ULONG_PTR)g_Idt;

    _IdtFlush((ULONG_PTR)&g_IdtPtr);

    return STATUS_SUCCESS;
}
