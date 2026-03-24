/* WinKernel NTKRNL-X — I/O Manager */

#include <kernel/io.h>
#include <kernel/serial.h>
#include <kernel/rtl.h>
#include <ntdef.h>
#include <ntstatus.h>

static CHAR  g_IoDrivers[IO_MAX_LOADED_DRIVERS][IO_DRIVER_NAME_MAX];
static DWORD g_IoDriverCount;

/* ── IoInitialize ───────────────────────────────────────────────────────── */

NTSTATUS IoInitialize(VOID) {
    RtlZeroMemory(g_IoDrivers, sizeof(g_IoDrivers));
    g_IoDriverCount = 0;
    return STATUS_SUCCESS;
}

/* ── IoRegisterLoadedDriver — post-start success registration ───────────── */

NTSTATUS IoRegisterLoadedDriver(PCSTR Name) {
    if (!Name || !Name[0]) return STATUS_INVALID_PARAMETER;
    if (g_IoDriverCount >= IO_MAX_LOADED_DRIVERS) return STATUS_INSUFFICIENT_RESOURCES;

    for (DWORD i = 0; i < g_IoDriverCount; i++) {
        if (RtlCompareString(g_IoDrivers[i], Name) == 0)
            return STATUS_SUCCESS;
    }

    RtlCopyString(g_IoDrivers[g_IoDriverCount], Name, IO_DRIVER_NAME_MAX);
    g_IoDriverCount++;
    return STATUS_SUCCESS;
}

DWORD IoGetLoadedDriverCount(VOID) {
    return g_IoDriverCount;
}

BOOL IoGetLoadedDriverName(DWORD Index, PSTR Out, SIZE_T OutSize) {
    if (!Out || OutSize == 0 || Index >= g_IoDriverCount) return FALSE;
    RtlCopyString(Out, g_IoDrivers[Index], OutSize);
    return TRUE;
}

/* ── IoConsoleReadEvent ─────────────────────────────────────────────────── */

BOOL IoConsoleReadEvent(PKEY_EVENT Event) {
    if (!Event) return FALSE;
    if (IoKeyboardReadEvent(Event)) return TRUE;

    CHAR c;
    if (!IoSerialTryRead(&c)) return FALSE;

    Event->Scancode  = 0;
    Event->Released  = FALSE;
    if (c == '\r')
        Event->Ascii = '\n';
    else if (c == '\x7F' || c == '\b')
        Event->Ascii = '\b';
    else
        Event->Ascii = c;
    return TRUE;
}
