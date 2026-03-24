#pragma once

#include <ntdef.h>
#include <ntstatus.h>
#include <kernel/ke.h>

/* ── Stop code name table entry ─────────────────────────────────────────── */
typedef struct _STOP_CODE_ENTRY {
    DWORD   Code;
    PCSTR   Name;
} STOP_CODE_ENTRY;

/* ── BSOD interface ─────────────────────────────────────────────────────── */
__attribute__((noreturn))
VOID KeBugCheckEx(
    DWORD       StopCode,
    ULONG_PTR   Param1,
    ULONG_PTR   Param2,
    ULONG_PTR   Param3,
    ULONG_PTR   Param4
);

__attribute__((noreturn))
VOID KeBugCheckWithFrame(
    DWORD           StopCode,
    PKTRAP_FRAME    Frame
);
