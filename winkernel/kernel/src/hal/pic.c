/* WinKernel NTKRNL-X — 8259 PIC driver */

#include <kernel/hal.h>
#include <ntdef.h>

/* ── 8259 PIC port addresses ────────────────────────────────────────────── */
#define PIC1_CMD    0x20
#define PIC1_DATA   0x21
#define PIC2_CMD    0xA0
#define PIC2_DATA   0xA1

/* ── ICW / OCW constants ────────────────────────────────────────────────── */
#define PIC_EOI     0x20    /* End-of-interrupt command */
#define ICW1_ICW4   0x01    /* ICW4 needed              */
#define ICW1_INIT   0x10    /* Initialization           */
#define ICW4_8086   0x01    /* 8086/88 mode             */

/* ── HalRemapPic ────────────────────────────────────────────────────────── */

VOID HalRemapPic(BYTE MasterOffset, BYTE SlaveOffset) {
    /* Save current masks */
    BYTE mask1 = HalReadPortByte(PIC1_DATA);
    BYTE mask2 = HalReadPortByte(PIC2_DATA);

    /* Start initialization sequence (cascade mode) */
    HalWritePortByte(PIC1_CMD,  ICW1_INIT | ICW1_ICW4);
    HalIoDelay();
    HalWritePortByte(PIC2_CMD,  ICW1_INIT | ICW1_ICW4);
    HalIoDelay();

    /* ICW2: vector offsets */
    HalWritePortByte(PIC1_DATA, MasterOffset);
    HalIoDelay();
    HalWritePortByte(PIC2_DATA, SlaveOffset);
    HalIoDelay();

    /* ICW3: cascade wiring */
    HalWritePortByte(PIC1_DATA, 0x04);   /* slave on IRQ2 */
    HalIoDelay();
    HalWritePortByte(PIC2_DATA, 0x02);   /* slave cascade identity */
    HalIoDelay();

    /* ICW4: 8086 mode */
    HalWritePortByte(PIC1_DATA, ICW4_8086);
    HalIoDelay();
    HalWritePortByte(PIC2_DATA, ICW4_8086);
    HalIoDelay();

    /* Mask all IRQs — drivers unmask their own lines individually */
    HalWritePortByte(PIC1_DATA, 0xFF);
    HalWritePortByte(PIC2_DATA, 0xFF);
}

/* ── HalPicSendEoi ──────────────────────────────────────────────────────── */

VOID HalPicSendEoi(BYTE Irq) {
    if (Irq >= 8) HalWritePortByte(PIC2_CMD, PIC_EOI);
    HalWritePortByte(PIC1_CMD, PIC_EOI);
}

/* ── HalPicMaskIrq ──────────────────────────────────────────────────────── */

VOID HalPicMaskIrq(BYTE Irq) {
    WORD port;
    BYTE value;
    if (Irq < 8) {
        port  = PIC1_DATA;
        value = HalReadPortByte(PIC1_DATA) | (BYTE)(1 << Irq);
    } else {
        port  = PIC2_DATA;
        value = HalReadPortByte(PIC2_DATA) | (BYTE)(1 << (Irq - 8));
    }
    HalWritePortByte(port, value);
}

/* ── HalPicUnmaskIrq ────────────────────────────────────────────────────── */

VOID HalPicUnmaskIrq(BYTE Irq) {
    WORD port;
    BYTE value;
    if (Irq < 8) {
        port  = PIC1_DATA;
        value = HalReadPortByte(PIC1_DATA) & (BYTE)~(1 << Irq);
    } else {
        port  = PIC2_DATA;
        value = HalReadPortByte(PIC2_DATA) & (BYTE)~(1 << (Irq - 8));
    }
    HalWritePortByte(port, value);
}
