/* WinKernel NTKRNL-X — PIT (8253/8254) timer driver, channel 0 at N Hz */

#include <kernel/hal.h>
#include <ntdef.h>

/* ── PIT ports ──────────────────────────────────────────────────────────── */
#define PIT_CHANNEL0    0x40
#define PIT_CMD         0x43

/* ── PIT base frequency ─────────────────────────────────────────────────── */
#define PIT_BASE_HZ     1193182UL

/* ── Tick counter (incremented by IRQ0 handler) ─────────────────────────── */
static volatile QWORD g_TickCount = 0;
static DWORD          g_FrequencyHz = 0;

/* ── Forward: KeRegisterIrqHandler ─────────────────────────────────────── */
extern VOID KeRegisterIrqHandler(BYTE Irq, VOID (*Handler)(PVOID));
extern VOID HalPicUnmaskIrq(BYTE Irq);

/* ── HalTimerIrqHandler — called from KiIsrDispatch for IRQ0 ────────────── */
static VOID _TimerIrqWrapper(PVOID Frame) {
    (VOID)Frame;
    g_TickCount++;
}

/* ── HalInitTimer ───────────────────────────────────────────────────────── */

NTSTATUS HalInitTimer(DWORD FrequencyHz) {
    if (FrequencyHz == 0) return STATUS_INVALID_PARAMETER;

    g_FrequencyHz = FrequencyHz;

    DWORD divisor = (DWORD)(PIT_BASE_HZ / FrequencyHz);
    if (divisor > 0xFFFF) divisor = 0xFFFF;
    if (divisor == 0)     divisor = 1;

    /* Channel 0, lobyte/hibyte, mode 3 (square wave), binary */
    HalWritePortByte(PIT_CMD, 0x36);
    HalWritePortByte(PIT_CHANNEL0, (BYTE)(divisor & 0xFF));
    HalWritePortByte(PIT_CHANNEL0, (BYTE)((divisor >> 8) & 0xFF));

    /* Register IRQ0 handler and unmask */
    KeRegisterIrqHandler(0, _TimerIrqWrapper);
    HalPicUnmaskIrq(0);

    return STATUS_SUCCESS;
}

/* ── HalGetTickCount ────────────────────────────────────────────────────── */

QWORD HalGetTickCount(VOID) {
    return g_TickCount;
}

/* ── HalGetUptimeSeconds ────────────────────────────────────────────────── */

QWORD HalGetUptimeSeconds(VOID) {
    if (g_FrequencyHz == 0) return 0;
    return g_TickCount / g_FrequencyHz;
}
