/* WinKernel NTKRNL-X — Global Descriptor Table (64-bit) */

#include <kernel/ke.h>
#include <kernel/rtl.h>
#include <ntdef.h>

/* ── GDT storage (null + code64 + data64 + user_code64 + user_data64 + TSS) */
#define GDT_ENTRY_COUNT  8   /* 6 standard + 2 for 16-byte TSS descriptor */

static GDT_ENTRY g_Gdt[GDT_ENTRY_COUNT];
static GDT_POINTER g_GdtPtr;
static TSS64 g_Tss;

/* ── Kernel stack for TSS RSP0 ──────────────────────────────────────────── */
static BYTE g_TssStack[8192] __attribute__((aligned(16)));

/* ── _SetGdtEntry ───────────────────────────────────────────────────────── */
static VOID _SetGdtEntry(DWORD Index, DWORD Base, DWORD Limit,
                         BYTE Access, BYTE Gran) {
    g_Gdt[Index].BaseLow    = (WORD)(Base & 0xFFFF);
    g_Gdt[Index].BaseMiddle = (BYTE)((Base >> 16) & 0xFF);
    g_Gdt[Index].BaseHigh   = (BYTE)((Base >> 24) & 0xFF);
    g_Gdt[Index].LimitLow   = (WORD)(Limit & 0xFFFF);
    g_Gdt[Index].Granularity= (BYTE)(((Limit >> 16) & 0x0F) | (Gran & 0xF0));
    g_Gdt[Index].Access     = Access;
}

/* ── _SetTssDescriptor — 16-byte system segment descriptor ─────────────── */
static VOID _SetTssDescriptor(DWORD Index, ULONG_PTR Base, DWORD Limit) {
    /* Low 8 bytes */
    g_Gdt[Index].LimitLow   = (WORD)(Limit & 0xFFFF);
    g_Gdt[Index].BaseLow    = (WORD)(Base & 0xFFFF);
    g_Gdt[Index].BaseMiddle = (BYTE)((Base >> 16) & 0xFF);
    g_Gdt[Index].Access     = 0x89;   /* Present, DPL=0, 64-bit TSS available */
    g_Gdt[Index].Granularity= (BYTE)(((Limit >> 16) & 0x0F));
    g_Gdt[Index].BaseHigh   = (BYTE)((Base >> 24) & 0xFF);

    /* High 8 bytes (stored in next GDT slot) */
    DWORD* high = (DWORD*)&g_Gdt[Index + 1];
    high[0] = (DWORD)(Base >> 32);
    high[1] = 0;
}

/* ── External ASM: reload segment registers after LGDT ─────────────────── */
extern VOID _GdtFlush(QWORD GdtPtr, WORD CodeSel, WORD DataSel);
extern VOID _TssFlush(WORD TssSel);

/* ── KeInitializeGdt ────────────────────────────────────────────────────── */

NTSTATUS KeInitializeGdt(VOID) {
    RtlZeroMemory(g_Gdt, sizeof(g_Gdt));
    RtlZeroMemory(&g_Tss, sizeof(g_Tss));

    /* 0: Null descriptor */
    _SetGdtEntry(0, 0, 0, 0x00, 0x00);

    /* 1: Kernel code (0x08) — 64-bit, DPL=0 */
    _SetGdtEntry(1, 0, 0xFFFFF, 0x9A, 0xA0);

    /* 2: Kernel data (0x10) — 64-bit, DPL=0 */
    _SetGdtEntry(2, 0, 0xFFFFF, 0x92, 0xC0);

    /* 3: User code (0x18) — 64-bit, DPL=3 */
    _SetGdtEntry(3, 0, 0xFFFFF, 0xFA, 0xA0);

    /* 4: User data (0x20) — 64-bit, DPL=3 */
    _SetGdtEntry(4, 0, 0xFFFFF, 0xF2, 0xC0);

    /* 5–6: TSS descriptor (16 bytes, occupies two GDT slots) */
    g_Tss.RSP0      = (QWORD)((ULONG_PTR)g_TssStack + sizeof(g_TssStack));
    g_Tss.IOPBOffset= sizeof(TSS64);
    _SetTssDescriptor(5, (ULONG_PTR)&g_Tss, sizeof(TSS64) - 1);

    /* Load GDTR */
    g_GdtPtr.Limit = (WORD)(sizeof(g_Gdt) - 1);
    g_GdtPtr.Base  = (QWORD)(ULONG_PTR)g_Gdt;

    _GdtFlush((QWORD)(ULONG_PTR)&g_GdtPtr, GDT_KERNEL_CODE, GDT_KERNEL_DATA);
    _TssFlush(GDT_TSS_LOW);

    return STATUS_SUCCESS;
}
