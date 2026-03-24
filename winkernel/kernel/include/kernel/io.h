#pragma once

#include <ntdef.h>
#include <ntstatus.h>

/* ── Key event ──────────────────────────────────────────────────────────── */
typedef struct _KEY_EVENT {
    BYTE    Scancode;
    CHAR    Ascii;
    BOOL    Released;
} KEY_EVENT, *PKEY_EVENT;

/* ── Keyboard queue size ────────────────────────────────────────────────── */
#define KB_QUEUE_SIZE   256

/* ── Io interface ───────────────────────────────────────────────────────── */
NTSTATUS    IoInitialize(VOID);
NTSTATUS    IoConnectKeyboard(VOID);

/* ── Keyboard API ───────────────────────────────────────────────────────── */
BOOL        IoKeyboardReadEvent(PKEY_EVENT Event);
CHAR        IoKeyboardGetChar(VOID);    /* blocking: spins until key pressed */
BOOL        IoKeyboardHasData(VOID);
