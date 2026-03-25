/* WinKernel NTKRNL-X - PS/2 keyboard driver (IRQ1) */

#include <kernel/io.h>
#include <kernel/ke.h>
#include <kernel/hal.h>
#include <kernel/bsod.h>
#include <kernel/rtl.h>
#include <ntdef.h>

#define PS2_DATA        0x60
#define PS2_STATUS      0x64
#define PS2_OBF         0x01    /* output buffer full - safe to read */
#define PS2_IBF         0x02    /* input  buffer full - controller busy */

/* ── Scancode set 1, US QWERTY, unshifted ───────────────────────────────── */
static const CHAR g_Map[128] = {
/*00*/  0,    0,   '1', '2', '3', '4', '5', '6', '7', '8',
/*0A*/  '9',  '0', '-', '=', '\b', '\t',
/*10*/  'q',  'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n',
/*1D*/  0,
/*1E*/  'a',  's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`',
/*2A*/  0,    '\\', 'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/', 0,
/*37*/  '*',  0,   ' ', 0,
/*3B*/  0,0,0,0,0,0,0,0,0,0,  /* F1-F10 */
/*45*/  0,0,                   /* NumLock, ScrollLock */
/*47*/  0,0,0,                 /* Home, Up, PgUp */
/*4A*/  '-',
/*4B*/  0,0,0,                 /* Left, 5, Right */
/*4E*/  '+',
/*4F*/  0,0,0,                 /* End, Down, PgDn */
/*52*/  0,0,                   /* Ins, Del */
/*54*/  0,0,0,0,0,             /* padding to 0x58 */
/*59*/  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0
};

/* ── Scancode set 1, US QWERTY, shifted ─────────────────────────────────── */
static const CHAR g_MapShift[128] = {
/*00*/  0,    0,   '!', '@', '#', '$', '%', '^', '&', '*',
/*0A*/  '(',  ')', '_', '+', '\b', '\t',
/*10*/  'Q',  'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', '{', '}', '\n',
/*1D*/  0,
/*1E*/  'A',  'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L', ':', '"', '~',
/*2A*/  0,    '|', 'Z', 'X', 'C', 'V', 'B', 'N', 'M', '<', '>', '?', 0,
/*37*/  '*',  0,   ' ', 0,
/*3B*/  0,0,0,0,0,0,0,0,0,0,
/*45*/  0,0,
/*47*/  0,0,0,
/*4A*/  '-',
/*4B*/  0,0,0,
/*4E*/  '+',
/*4F*/  0,0,0,
/*52*/  0,0,
/*54*/  0,0,0,0,0,
/*59*/  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0
};

/* ── Ring buffer ────────────────────────────────────────────────────────── */
static KEY_EVENT      g_Queue[KB_QUEUE_SIZE];
static volatile DWORD g_Head = 0;
static volatile DWORD g_Tail = 0;

/* ── State ──────────────────────────────────────────────────────────────── */
static volatile BOOL g_Shift  = FALSE;
static volatile BOOL g_Caps   = FALSE;
static BOOL          g_Ps2Ok  = FALSE;  /* TRUE = real i8042 found */

#define SC_LSHIFT  0x2A
#define SC_RSHIFT  0x36
#define SC_CAPS    0x3A
#define SC_BREAK   0x80  /* bit 7 = key-release flag */

/* ── _Enqueue ───────────────────────────────────────────────────────────── */
static VOID _Enqueue(BYTE sc, CHAR ascii, BOOL released) {
    DWORD next = (g_Tail + 1) % KB_QUEUE_SIZE;
    if (next == g_Head) return;  /* full - drop */
    g_Queue[g_Tail].Scancode = sc;
    g_Queue[g_Tail].Ascii    = ascii;
    g_Queue[g_Tail].Released = released;
    g_Tail = next;
}

/* ── _Process ───────────────────────────────────────────────────────────── */
static VOID _Process(BYTE sc) {
    /* 0x00 = PS/2 overrun, 0xFF = no controller / line error - discard both */
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
    /* Guard: only read when OBF is set - prevents reading 0xFF garbage
       on spurious IRQs or USB-only machines where the port is floating */
    if (HalReadPortByte(PS2_STATUS) & PS2_OBF)
        _Process(HalReadPortByte(PS2_DATA));
}

/* IoConnectKeyboard -------------------------------------------------------- */
NTSTATUS IoConnectKeyboard(VOID) {
    g_Head  = 0;
    g_Tail  = 0;
    g_Shift = FALSE;
    g_Caps  = FALSE;
    g_Ps2Ok = FALSE;

    /* Single non-blocking status read. 0xFF = no i8042 (USB-only machine). */
    BYTE status = HalReadPortByte(PS2_STATUS);
    if (status == 0xFF) {
        KePanicLog("PS/2: absent (status=0xFF) - USB-only machine");
        KeRegisterIrqHandler(1, _IrqHandler);
        return STATUS_SUCCESS;
    }

    /* Drain stale bytes left by firmware (non-blocking, max 16 reads). */
    for (DWORD i = 0; i < 16 && (HalReadPortByte(PS2_STATUS) & PS2_OBF); i++)
        HalReadPortByte(PS2_DATA);

    /* UEFI already enables the keyboard before handoff.
       Do NOT send 0xAE / 0xF4 here — those require waiting for ACK
       which freezes when interrupts are not yet enabled.
       Just register the IRQ and unmask. */
    g_Ps2Ok = TRUE;
    KePanicLog("PS/2: i8042 present (status=0x%02x)", (DWORD)status);

    KeRegisterIrqHandler(1, _IrqHandler);
    HalPicUnmaskIrq(1);
    HalPicUnmaskIrq(2);  /* cascade - slave PIC */

    return STATUS_SUCCESS;
}

/* ── IoKeyboardReadEvent ────────────────────────────────────────────────── */
BOOL IoKeyboardReadEvent(PKEY_EVENT Event) {
    if (!Event) return FALSE;

    /* Polled fallback: only touch the data port when a real i8042 is present
       AND the OBF bit confirms data is waiting. Without both guards, USB-only
       machines return 0xFF on every read and flood the queue with garbage.   */
    if (g_Ps2Ok && (g_Head == g_Tail)) {
        if (HalReadPortByte(PS2_STATUS) & PS2_OBF)
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

/* ── IoKeyboardGetChar - blocking, yields CPU via hlt ───────────────────── */
CHAR IoKeyboardGetChar(VOID) {
    KEY_EVENT ev;
    for (;;) {
        /* Use IoConsoleReadEvent so serial input also works */
        while (!IoConsoleReadEvent(&ev))
            __asm__ volatile ("hlt");
        if (ev.Released) continue;
        if (ev.Ascii == '\b' || ev.Ascii == '\n' || ev.Ascii == '\r') return ev.Ascii;
        if (ev.Ascii >= 32 && (BYTE)ev.Ascii <= 126) return ev.Ascii;
    }
}
