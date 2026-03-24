#pragma once

#include <ntdef.h>
#include <ntstatus.h>

/* ── CPU feature flags ──────────────────────────────────────────────────── */
typedef struct _HAL_CPU_INFO {
    CHAR    VendorString[13];
    CHAR    BrandString[49];
    DWORD   Family;
    DWORD   Model;
    DWORD   Stepping;
    BOOL    HasSSE;
    BOOL    HasSSE2;
    BOOL    HasSSE3;
    BOOL    HasAVX;
    BOOL    HasAVX2;
    DWORD   LogicalCpuCount;
} HAL_CPU_INFO, *PHAL_CPU_INFO;

extern HAL_CPU_INFO HalCpuInfo;

/* ── HAL interface ──────────────────────────────────────────────────────── */
NTSTATUS    HalInitialize(VOID);
VOID        HalGetCpuInfo(PHAL_CPU_INFO Info);

/* ── Port I/O ───────────────────────────────────────────────────────────── */
BYTE        HalReadPortByte(WORD Port);
VOID        HalWritePortByte(WORD Port, BYTE Value);
WORD        HalReadPortWord(WORD Port);
VOID        HalWritePortWord(WORD Port, WORD Value);
DWORD       HalReadPortDword(WORD Port);
VOID        HalWritePortDword(WORD Port, DWORD Value);
VOID        HalIoDelay(VOID);

/* ── PIC ────────────────────────────────────────────────────────────────── */
VOID        HalRemapPic(BYTE MasterOffset, BYTE SlaveOffset);
VOID        HalPicSendEoi(BYTE Irq);
VOID        HalPicMaskIrq(BYTE Irq);
VOID        HalPicUnmaskIrq(BYTE Irq);

/* ── PIT timer ──────────────────────────────────────────────────────────── */
NTSTATUS    HalInitTimer(DWORD FrequencyHz);
QWORD       HalGetTickCount(VOID);
QWORD       HalGetUptimeSeconds(VOID);
