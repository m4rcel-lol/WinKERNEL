#pragma once

#include <ntdef.h>

/* ── Success codes ──────────────────────────────────────────────────────── */
#define STATUS_SUCCESS                      ((NTSTATUS)0x00000000)
#define STATUS_PENDING                      ((NTSTATUS)0x00000103)

/* ── Informational codes ────────────────────────────────────────────────── */
#define STATUS_NO_MORE_ENTRIES              ((NTSTATUS)0x8000001A)

/* ── Error codes ────────────────────────────────────────────────────────── */
#define STATUS_FAILURE                      ((NTSTATUS)0xC0000001)
#define STATUS_NOT_IMPLEMENTED              ((NTSTATUS)0xC0000002)
#define STATUS_INVALID_INFO_CLASS           ((NTSTATUS)0xC0000003)
#define STATUS_ACCESS_VIOLATION             ((NTSTATUS)0xC0000005)
#define STATUS_IN_PAGE_ERROR                ((NTSTATUS)0xC0000006)
#define STATUS_INVALID_HANDLE               ((NTSTATUS)0xC0000008)
#define STATUS_NO_MEMORY                    ((NTSTATUS)0xC0000017)
#define STATUS_ACCESS_DENIED                ((NTSTATUS)0xC0000022)
#define STATUS_BUFFER_TOO_SMALL             ((NTSTATUS)0xC0000023)
#define STATUS_OBJECT_NAME_NOT_FOUND        ((NTSTATUS)0xC0000034)
#define STATUS_OBJECT_NAME_COLLISION        ((NTSTATUS)0xC0000035)
#define STATUS_INSUFFICIENT_RESOURCES       ((NTSTATUS)0xC000009A)
#define STATUS_INVALID_PARAMETER            ((NTSTATUS)0xC000000D)
#define STATUS_NOT_FOUND                    ((NTSTATUS)0xC0000225)
#define STATUS_ALREADY_EXISTS               ((NTSTATUS)0xC0000035)
#define STATUS_END_OF_FILE                  ((NTSTATUS)0xC0000011)
#define STATUS_HEAP_CORRUPTION              ((NTSTATUS)0xC0000374)

/* ── NT_SUCCESS macro ───────────────────────────────────────────────────── */
#define NT_SUCCESS(status)      (((NTSTATUS)(status)) < 0x80000000)
#define NT_ERROR(status)        (((NTSTATUS)(status)) >= 0xC0000000)

/* ── BSOD stop codes ────────────────────────────────────────────────────── */
#define STOP_IRQL_NOT_LESS_OR_EQUAL         0x0000000A
#define STOP_PAGE_FAULT_IN_NONPAGED_AREA    0x00000050
#define STOP_KERNEL_MODE_EXCEPTION          0x0000008E
#define STOP_UNEXPECTED_KERNEL_MODE_TRAP    0x0000007F
#define STOP_MANUALLY_INITIATED_CRASH       0x000000E2
#define STOP_HEAP_CORRUPTION                0x00000139
#define STOP_CRITICAL_PROCESS_DIED          0x000000EF
