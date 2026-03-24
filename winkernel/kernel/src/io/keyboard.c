/* WinKernel NTKRNL-X — PS/2 keyboard driver (IRQ1) */

#include <kernel/io.h>
#include <kernel/ke.h>
#include <kernel/hal.h>
#include <kernel/rtl.h>
#include <ntdef.h>

/* ── PS/2 ports ─────────────────────────────────────────────────────────── */
#define PS2_DATA    0x60
#define PS2_STATUS  0x64

/* ── Scancode → ASCII table (US QWERTY, set 1, unshifted) ──────────────── */
static const CHAR g_ScancodeMap[128] = {
    0,    0,   '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', '\b',
    '\t', 'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n',
    0,    'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`',
    0,    '\\','z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/', 0,
    '*',  0,   ' ', 0,
    /* F1–F10, num lock, scroll lock, home, up, pgup, -, left, 5, right, +,
       end, down, pgdn, ins, del — all 0 for now */
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0
};

/* ── Shifted scancode map ───────────────────────────────────────────────── */
static const CHAR g_ScancodeMapShift[128] = {
    0,    0,   '!', '@', '#', '$', '%', '^', '&', '*', '(', ')', '_', '+', '\b',
    '\t', 'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', '{', '}', '\n',
    0,    'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L', ':', '"', '~',
    0,    '|', 'Z', 'X', 'C', 'V', 'B', 'N', 'M', '<', '>', '?', 0,
    '*',  0,   ' ', 0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0
};

/* ── Key event ring buffer ──────────────────────────────────────────────── */
static KEY_EVENT g_KeyQueue[KB_QUEUE_SIZE];
static volatile DWORD g_QueueHead = 0;
static volatile DWORD g_QueueTail = 0;

/* ── Modifier state ─────────────────────────────────────────────────────── */
static volatile BOOL g_ShiftHeld  = FALSE;
static volatile BOOL g_CapsLock   = FALSE;

/* ── Special scancodes ──────────────────────────────────────────────────── */
#define SC_LSHIFT   0x2A
#define SC_RSHIFT   0x36
#define SC_CAPS     0x3A
#define SC_RELEASE  0x80    /* bit 7 set = key release */

/* ── _KbEnqueue ─────────────────────────────────────────────────────────── */
static VOID _KbEnqueue(BYTE Scancode, CHAR Ascii, BOOL Released) {
    DWORD next = (g_QueueTail + 1) % KB_QUEUE_SIZE;
    if (next == g_QueueHead) return;    /* queue full, drop */

    g_KeyQueue[g_QueueTail].Scancode = Scancode;
    g_KeyQueue[g_QueueTail].Ascii    = Ascii;
    g_KeyQueue[g_QueueTail].Released = Released;
    g_QueueTail = next;
}

static VOID _KbProcessScancode(BYTE sc) {
    BOOL released = (sc & SC_RELEASE) ? TRUE : FALSE;
    BYTE raw = sc & 0x7F;

    /* Handle modifiers */
    if (raw == SC_LSHIFT || raw == SC_RSHIFT) {
        g_ShiftHeld = !released;
        return;
    }
    if (raw == SC_CAPS && !released) {
        g_CapsLock = !g_CapsLock;
        return;
    }

    if (released) {
        _KbEnqueue(raw, 0, TRUE);
        return;
    }

    /* Translate scancode to ASCII */
    CHAR ascii = 0;
    if (raw < 128) {
        if (g_ShiftHeld) {
            ascii = g_ScancodeMapShift[raw];
        } else {
            ascii = g_ScancodeMap[raw];
            /* Apply caps lock to letters */
            if (g_CapsLock && ascii >= 'a' && ascii <= 'z')
                ascii = (CHAR)(ascii - 32);
        }
    }

    _KbEnqueue(raw, ascii, FALSE);
}

/* ── _KbIrqHandler — called from KiIsrDispatch for IRQ1 ────────────────── */
static VOID _KbIrqHandler(PVOID Frame) {
    (VOID)Frame;
    _KbProcessScancode(HalReadPortByte(PS2_DATA));
}

/* ── IoConnectKeyboard ──────────────────────────────────────────────────── */

NTSTATUS IoConnectKeyboard(VOID) {
    g_QueueHead = 0;
    g_QueueTail = 0;
    g_ShiftHeld = FALSE;
    g_CapsLock  = FALSE;

    /* Flush any stale byte sitting in the PS/2 buffer */
    while (HalReadPortByte(PS2_STATUS) & 0x01)
        HalReadPortByte(PS2_DATA);

    KeRegisterIrqHandler(1, _KbIrqHandler);

    /* Unmask IRQ1 (keyboard) and IRQ2 (cascade — required for slave PIC) */
    HalPicUnmaskIrq(1);
    HalPicUnmaskIrq(2);

    (VOID)IoRegisterLoadedDriver("PS/2 Keyboard (i8042prt)");

    return STATUS_SUCCESS;
}

/* ── IoKeyboardReadEvent ────────────────────────────────────────────────── */

BOOL IoKeyboardReadEvent(PKEY_EVENT Event) {
    if (!Event) return FALSE;

    /* Fallback polling path: if IRQ delivery is broken, still parse queued
       bytes from i8042 data port so shell input does not hard-freeze. */
    if (g_QueueHead == g_QueueTail && (HalReadPortByte(PS2_STATUS) & 0x01)) {
        _KbProcessScancode(HalReadPortByte(PS2_DATA));
    }

    if (g_QueueHead == g_QueueTail) return FALSE;

    *Event = g_KeyQueue[g_QueueHead];
    g_QueueHead = (g_QueueHead + 1) % KB_QUEUE_SIZE;
    return TRUE;
}

/* ── IoKeyboardHasData ──────────────────────────────────────────────────── */

BOOL IoKeyboardHasData(VOID) {
    return g_QueueHead != g_QueueTail;
}

/* ── IoKeyboardGetChar — blocking spin until printable key ──────────────── */

CHAR IoKeyboardGetChar(VOID) {
    KEY_EVENT ev;
    while (TRUE) {
        while (!IoKeyboardReadEvent(&ev)) {
            __asm__ volatile ("pause");
        }
        if (!ev.Released && ev.Ascii != 0) return ev.Ascii;
        if (!ev.Released && (ev.Ascii == '\b' || ev.Ascii == '\n')) return ev.Ascii;
    }
}
