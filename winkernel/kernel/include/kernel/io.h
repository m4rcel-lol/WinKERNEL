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

#define IO_DRIVER_NAME_MAX   64
#define IO_MAX_LOADED_DRIVERS 16

/* ── Io interface ───────────────────────────────────────────────────────── */
NTSTATUS    IoInitialize(VOID);
NTSTATUS    IoConnectKeyboard(VOID);

/* Loaded miniport / class driver list (registration order) */
NTSTATUS    IoRegisterLoadedDriver(PCSTR Name);
DWORD       IoGetLoadedDriverCount(VOID);
BOOL        IoGetLoadedDriverName(DWORD Index, PSTR Out, SIZE_T OutSize);

/* ── Keyboard API ───────────────────────────────────────────────────────── */
BOOL        IoKeyboardReadEvent(PKEY_EVENT Event);
CHAR        IoKeyboardGetChar(VOID);    /* blocking: spins until key pressed */
BOOL        IoKeyboardHasData(VOID);
