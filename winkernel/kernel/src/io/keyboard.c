/* WinKernel NTKRNL-X — PS/2 keyboard driver (IRQ1) */

#include <kernel/io.h>
#include <kernel/ke.h>
#include <kernel/hal.h>
#include <kernel/bsod.h>
#include <kernel/rtl.h>
#include <ntdef.h>

/* ── PS/2 ports ─────────────────────────────────────────────────────────── */
#define PS2_DATA        0x60
#define PS2_STATUS      0x64
#define PS2_STAT_OBF    0x01    /* output buffer full — data ready */
#define PS2_STAT_IBF    0x02    /* input  buffer full — controller busy */

/* ── Scancode set 1, US QWERTY ──────────────────────────────────────────── */
static const CHAR g_Map[128] = {
    0,    0,   '1', '2', '3', '4', '5', '6', '7', '8',
    '9',  '0', '-', '=', '\b', '\t',
    'q',  'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n',
    0,
    'a',  's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`',
    0,    '\\', 'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/', 0,
    '*',  0,   ' ', 0,
    0,0,0,0,0,0,0,0,0,0,  /* F1-F10 */
    0,0,                   /* num lock, scroll lock */
    0,0,0,                 /* home, up, pgup */
    '-',
    0,0,0,                 /* left, 5, right */
    '+',
    0,0,0,                 /* end, down, pgdn */
    0,0,                   /* ins, del */
    0,0,0,0,0,             /* padding */
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0
};

static const CHAR g_MapShift[128] = {
    0,    0,   '!', '@', '#', '$', '%', '^', '&', '*',
    '(',  ')', '_', '+', '\b', '\t',
    'Q',  'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', '{', '}', '\n',
    0,
    'A',  'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L', ':', '"', '~',
    0,    '|', 'Z', 'X', 'C', 'V', 'B', 'N', 'M', '<', '>', '?', 0,
    '*',  0,   ' ', 0,
    0,0,0,0,0,0,0,0,0,0,
    0,0,
    0,0,0,
    '-',
    0,0,0,
    '+',
    0,0,0,
    0,0,
    0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0
};

/* ── Ring buffer ────────────────────────────────────────────────────────── */
static KEY_EVENT      g_Queue[KB_QUEUE_SIZE];
static volatile DWORD g_Head = 0;
static volatile DWORD g_Tail = 0;

/* ── Modifier state ─────────────────────────────────────────────────────── */
static volatile BOOL g_Shift   = FALSE;
static volatile BOOL g_Caps    = FALSE;

/* ── PS/2 present flag ──────────────────────────────────────────────────── */
static BOOL g_Ps2Ok = FALSE;

#define SC_LSHIFT  0x2A
#define SC_RSHIFT  0x36
#define SC_CAPS    0x3A
#define SC_BREAK   0x80

/* ── _Enqueue ───────────────────────────────────────────────────────────── */
static VOID _Enqueue(BYTE sc, CHAR ascii, BOOL released) {
    DWORD next = (g_Tail + 1) % KB_QUEUE_SIZE;
    if (next == g_Head) return;
    g_Queue[g_Tail].Scancode = sc;
    g_Queue[g_Tail].Ascii    = ascii;
    g_Queue[g_Tail].Released = released;
    g_Tail = next;
}

/* ── _Process ───────────────────────────────────────────────────────────── */
static VOID _Process(BYTE sc) {
    /* 0x00 and 0xFF are PS/2 error/absent codes — discard */
    if (sc == 0x00 || sc == 0xFF) return;

    BOOL released = (sc & SC_BREAK) ? TRUE : FALSE;
    BYTE raw = sc & 0x7F;

    if (raw == SC_LSHIFT || raw == SC_RSHIFT) { g_Shift = !released; return; }
    if (raw == SC_CAPS && !released)           { g_Caps  = !g_Caps;  return; }
    if (released) { _Enqueue(raw, 0, TRUE); return; }

    CHAR ascii = g_Shift ? g_MapShift[raw] : g_Map[raw];
    if (!g_Shift && g_Caps && ascii >= 'a' && ascii <= 'z')
        ascii = (CHAR)(ascii - 32);

    _Enqueue(raw, ascii, FALSE);
}

/* ── IRQ1 handler ───────────────────────────────────────────────────────── */
static VOID _IrqHandler(PVOID Frame) {
    (VOID)Frame;
    if (HalReadPortByte(PS2_STATUS) & PS2_STAT_OBF)
        _Process(HalReadPortByte(PS2_DATA));
}

/* ── _Ps2WaitIn — wait for input buffer empty ───────────────────────────── */
static BOOL _Ps2WaitIn(void) {
    for (DWORD i = 0; i < 65536; i++) {
        if (!(HalReadPortByte(PS2_STATUS) & PS2_STAT_IBF)) return TRUE;
        __asm__ volatile ("pause");
    }
    return FALSE;
}

/* ── _Ps2WaitOut — wait for output buffer full ──────────────────────────── */
static BOOL _Ps2WaitOut(void) {
    for (DWORD i = 0; i < 65536; i++) {
        if (HalReadPortByte(PS2_STATUS) & PS2_STAT_OBF) return TRUE;
        __asm__ volatile ("pause");
    }
    return FALSE;
}

/* ── IoConnectKeyboard ──────────────────────────────────────────────────── */
NTSTATUS IoConnectKeyboard(VOID) {
    g_Head  = 0;
    g_Tail  = 0;
    g_Shift = FALSE;
    g_Caps  = FALSE;
    g_Ps2Ok = FALSE;

    /* Quick sanity check: if status port reads 0xFF the i8042 is absent.
       This is a single read — no timeout loop, no freeze. */
    BYTE status = HalReadPortByte(PS2_STATUS);
    if (status == 0xFF) {
        /* USB-only machine — register IRQ handler (harmless) and return OK.
           Input will arrive via serial console. */
        KeRegisterIrqHandler(1, _IrqHandler);
        KePanicLog("PS/2: status=0xFF, no i8042 (USB-only machine)");
        return STATUS_SUCCESS;
    }

    g_Ps2Ok = TRUE;
    KePanicLog("PS/2: i8042 detected, status=0x%02x", (DWORD)status);

    /* Drain any stale bytes */
    for (DWORD i = 0; i < 16 && (HalReadPortByte(PS2_STATUS) & PS2_STAT_OBF); i++)
        HalReadPortByte(PS2_DATA);

    /* Enable first PS/2 port (0xAE to command port) */
    if (_Ps2WaitIn()) HalWritePortByte(PS2_STATUS, 0xAE);

    /* Send "enable scanning" (0xF4) to keyboard device */
    if (_Ps2WaitIn()) HalWritePortByte(PS2_DATA, 0xF4);

    /* Consume ACK (0xFA) — don't block if it doesn't come */
    if (_Ps2WaitOut()) HalReadPortByte(PS2_DATA);

    KeRegisterIrqHandler(1, _IrqHandler);
    HalPicUnmaskIrq(1);
    HalPicUnmaskIrq(2);  /* cascade — required for slave PIC */

    KePanicLog("PS/2: keyboard enabled, IRQ1 unmasked");
    return STATUS_SUCCESS;
}

/* ── IoKeyboardReadEvent ────────────────────────────────────────────────── */
BOOL IoKeyboardReadEvent(PKEY_EVENT Event) {
    if (!Event) return FALSE;

    /* Polled fallback: only touch the data port when OBF is set.
       Without this guard, USB-only machines return 0xFF on every read
       and flood the queue with garbage. */
    if (g_Ps2Ok && g_Head == g_Tail) {
        if (HalReadPortByte(PS2_STATUS) & PS2_STAT_OBF)
            _Process(HalReadPortByte(PS2_DATA));
    }

    if (g_Head == g_Tail) return FALSE;

    *Event = g_Queue[g_Head];
    g_Head = (g_Head + 1) % KB_QUEUE_SIZE;
    return TRUE;
}

/* ── IoKeyboardHasData ──────────────────────────────────────────────────── */
BOOL IoKeyboardHasData(VOID) {
    return g_Head != g_Tail;
}

/* ── IoKeyboardGetChar — blocking ───────────────────────────────────────── */
CHAR IoKeyboardGetChar(VOID) {
    KEY_EVENT ev;
    for (;;) {
        while (!IoConsoleReadEvent(&ev))
            __asm__ volatile ("hlt");   /* sleep until next IRQ */
        if (ev.Released) continue;
        if (ev.Ascii == '\b' || ev.Ascii == '\n' || ev.Ascii == '\r') return ev.Ascii;
        if (ev.Ascii >= 32 && (BYTE)ev.Ascii <= 126) return ev.Ascii;
    }
}
