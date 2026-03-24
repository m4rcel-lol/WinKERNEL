/* WinKernel NTKRNL-X — COM1 (0x3F8) polled serial for shell input */

#include <kernel/serial.h>
#include <kernel/hal.h>
#include <kernel/io.h>
#include <ntdef.h>
#include <ntstatus.h>

#define COM1_BASE       0x3F8
#define COM1_DATA       (COM1_BASE + 0)
#define COM1_IER        (COM1_BASE + 1)
#define COM1_IIR        (COM1_BASE + 2)
#define COM1_LCR        (COM1_BASE + 3)
#define COM1_MCR        (COM1_BASE + 4)
#define COM1_LSR        (COM1_BASE + 5)

#define LSR_DATA_READY  0x01
#define LCR_DLAB        0x80

static BOOL g_SerialReady;

static VOID _Com1Delay(VOID) {
    HalIoDelay();
}

NTSTATUS IoConnectSerial(VOID) {
    g_SerialReady = FALSE;

    /* 115200 8N1, divisor 1 @ standard PC UART clock */
    HalWritePortByte(COM1_IER, 0x00);
    HalWritePortByte(COM1_LCR, LCR_DLAB);
    HalWritePortByte(COM1_DATA, 0x01);
    HalWritePortByte(COM1_IER, 0x00);
    HalWritePortByte(COM1_LCR, 0x03);

    HalWritePortByte(COM1_IIR, 0xC7);
    HalWritePortByte(COM1_MCR, 0x03);
    _Com1Delay();

    g_SerialReady = TRUE;
    (VOID)IoRegisterLoadedDriver("Serial console (COM1 115200)");

    return STATUS_SUCCESS;
}

BOOL IoSerialTryRead(CHAR* Out) {
    if (!g_SerialReady || !Out) return FALSE;
    if (!(HalReadPortByte(COM1_LSR) & LSR_DATA_READY)) return FALSE;
    *Out = (CHAR)HalReadPortByte(COM1_DATA);
    return TRUE;
}
