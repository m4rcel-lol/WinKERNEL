#pragma once

#include <ntdef.h>
#include <ntstatus.h>

/* ── GDT segment selectors ──────────────────────────────────────────────── */
#define GDT_NULL_SEL        0x00
#define GDT_KERNEL_CODE     0x08
#define GDT_KERNEL_DATA     0x10
#define GDT_USER_CODE       0x18
#define GDT_USER_DATA       0x20
#define GDT_TSS_LOW         0x28

/* ── IDT / interrupt constants ──────────────────────────────────────────── */
#define IDT_ENTRIES         256
#define IRQ_BASE            0x20

/* ── Interrupt frame pushed by our stubs ───────────────────────────────── */
/* Field order MUST match the push order in idt.asm _isr_common exactly.    */
/* First field = lowest address = top of stack after all pushes.            */
typedef struct _KTRAP_FRAME {
    /* GPRs — pushed last-to-first so R15 is at lowest address */
    QWORD   R15, R14, R13, R12;
    QWORD   R11, R10, R9,  R8;
    QWORD   RBP, RDI, RSI, RDX;
    QWORD   RCX, RBX, RAX;
    /* Pushed by stub */
    QWORD   InterruptNumber;
    QWORD   ErrorCode;
    /* Pushed by CPU on interrupt entry */
    QWORD   RIP;
    QWORD   CS;
    QWORD   RFLAGS;
    QWORD   RSP;
    QWORD   SS;
} KTRAP_FRAME, *PKTRAP_FRAME;

/* ── GDT descriptor ─────────────────────────────────────────────────────── */
typedef struct __attribute__((packed)) _GDT_ENTRY {
    WORD    LimitLow;
    WORD    BaseLow;
    BYTE    BaseMiddle;
    BYTE    Access;
    BYTE    Granularity;
    BYTE    BaseHigh;
} GDT_ENTRY, *PGDT_ENTRY;

typedef struct __attribute__((packed)) _GDT_POINTER {
    WORD    Limit;
    QWORD   Base;
} GDT_POINTER, *PGDT_POINTER;

/* ── IDT descriptor ─────────────────────────────────────────────────────── */
typedef struct __attribute__((packed)) _IDT_ENTRY {
    WORD    OffsetLow;
    WORD    Selector;
    BYTE    IST;
    BYTE    TypeAttr;
    WORD    OffsetMiddle;
    DWORD   OffsetHigh;
    DWORD   Reserved;
} IDT_ENTRY, *PIDT_ENTRY;

typedef struct __attribute__((packed)) _IDT_POINTER {
    WORD    Limit;
    QWORD   Base;
} IDT_POINTER, *PIDT_POINTER;

/* ── TSS ────────────────────────────────────────────────────────────────── */
typedef struct __attribute__((packed)) _TSS64 {
    DWORD   Reserved0;
    QWORD   RSP0;
    QWORD   RSP1;
    QWORD   RSP2;
    QWORD   Reserved1;
    QWORD   IST[7];
    QWORD   Reserved2;
    WORD    Reserved3;
    WORD    IOPBOffset;
} TSS64, *PTSS64;

/* ── Ke interface ───────────────────────────────────────────────────────── */
NTSTATUS    KeInitializeGdt(VOID);
NTSTATUS    KeInitializeIdt(VOID);
VOID        KiTrapHandler(PKTRAP_FRAME Frame);
VOID        KiIsrDispatch(PKTRAP_FRAME Frame);
VOID        KiCommonHandler(PKTRAP_FRAME Frame);

/* ── IRQ handler registration ───────────────────────────────────────────── */
typedef VOID (*KIRQ_HANDLER)(PVOID Frame);
VOID        KeRegisterIrqHandler(BYTE Irq, KIRQ_HANDLER Handler);
