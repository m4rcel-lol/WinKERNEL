/* WinKernel NTKRNL-X — CPU exception and IRQ dispatch */

#include <kernel/ke.h>
#include <kernel/hal.h>
#include <kernel/bsod.h>
#include <kernel/terminal.h>
#include <ntdef.h>
/* ── IRQ handler table ──────────────────────────────────────────────────── */
static KIRQ_HANDLER g_IrqHandlers[16];

VOID KeRegisterIrqHandler(BYTE Irq, KIRQ_HANDLER Handler) {
    if (Irq < 16) g_IrqHandlers[Irq] = Handler;
}

/* ── Exception name table ───────────────────────────────────────────────── */
static const CHAR* g_ExceptionNames[32] = {
    "Divide Error",             /* 0  */
    "Debug",                    /* 1  */
    "NMI",                      /* 2  */
    "Breakpoint",               /* 3  */
    "Overflow",                 /* 4  */
    "Bound Range Exceeded",     /* 5  */
    "Invalid Opcode",           /* 6  */
    "Device Not Available",     /* 7  */
    "Double Fault",             /* 8  */
    "Coprocessor Segment",      /* 9  */
    "Invalid TSS",              /* 10 */
    "Segment Not Present",      /* 11 */
    "Stack Fault",              /* 12 */
    "General Protection Fault", /* 13 */
    "Page Fault",               /* 14 */
    "Reserved",                 /* 15 */
    "x87 FPU Error",            /* 16 */
    "Alignment Check",          /* 17 */
    "Machine Check",            /* 18 */
    "SIMD Exception",           /* 19 */
    "Virtualization Exception", /* 20 */
    "Control Protection",       /* 21 */
    "Reserved",                 /* 22 */
    "Reserved",                 /* 23 */
    "Reserved",                 /* 24 */
    "Reserved",                 /* 25 */
    "Reserved",                 /* 26 */
    "Reserved",                 /* 27 */
    "Reserved",                 /* 28 */
    "Reserved",                 /* 29 */
    "Security Exception",       /* 30 */
    "Reserved"                  /* 31 */
};

/* ── KiCommonHandler — called from idt.asm _isr_common ─────────────────── */

VOID KiCommonHandler(PKTRAP_FRAME Frame) {
    QWORD vec = Frame->InterruptNumber;

    if (vec < 32) {
        /* CPU exception */
        KiTrapHandler(Frame);
    } else if (vec < 48) {
        /* Hardware IRQ (PIC remapped to 0x20–0x2F) */
        KiIsrDispatch(Frame);
    }
    /* Vectors 48–255: spurious / unhandled — silently ignore */
}

/* ── KiTrapHandler — CPU exceptions ────────────────────────────────────── */

VOID KiTrapHandler(PKTRAP_FRAME Frame) {
    QWORD vec = Frame->InterruptNumber;

    /* Log the exception before rendering the BSOD */
    KePanicLog("CPU exception #%llu at RIP=%016llx errcode=%016llx",
               vec, Frame->RIP, Frame->ErrorCode);

    switch (vec) {
    case 14:
        KePanicLog("Page fault: CR2 (faulting address) logged in register dump");
        KeBugCheckWithFrame(STOP_PAGE_FAULT_IN_NONPAGED_AREA, Frame);
        break;
    case 8:
        KeBugCheckWithFrame(STOP_UNEXPECTED_KERNEL_MODE_TRAP, Frame);
        break;
    default:
        KeBugCheckWithFrame(STOP_KERNEL_MODE_EXCEPTION, Frame);
        break;
    }

    (VOID)g_ExceptionNames[0];
}

/* ── KiIsrDispatch — hardware IRQs ─────────────────────────────────────── */

VOID KiIsrDispatch(PKTRAP_FRAME Frame) {
    BYTE irq = (BYTE)(Frame->InterruptNumber - IRQ_BASE);

    if (g_IrqHandlers[irq]) {
        g_IrqHandlers[irq]((PVOID)Frame);
    }

    HalPicSendEoi(irq);
}
