#pragma once

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

/* ── Primitive NT types ─────────────────────────────────────────────────── */
typedef void                VOID;
typedef bool                BOOL;
typedef uint8_t             BYTE;
typedef uint8_t             UCHAR;
typedef char                CHAR;       /* plain char — matches string literals */
typedef uint16_t            WORD;
typedef uint16_t            USHORT;
typedef int16_t             SHORT;
typedef uint32_t            DWORD;
typedef uint32_t            ULONG;
typedef int32_t             LONG;
typedef uint64_t            QWORD;

/* ── Signed int shorthand ───────────────────────────────────────────────── */
typedef int32_t             INT;
typedef uint64_t            ULONG_PTR;
typedef uint64_t            ULONGLONG;
typedef int64_t             LONGLONG;
typedef size_t              SIZE_T;
typedef void*               PVOID;
typedef char*               PSTR;
typedef const char*         PCSTR;
typedef uint8_t*            PUCHAR;
typedef uint16_t*           PUSHORT;
typedef uint32_t*           PULONG;
typedef uint64_t*           PULONG_PTR;

/* ── Handle types ───────────────────────────────────────────────────────── */
typedef uint32_t            HANDLE;
#define INVALID_HANDLE      ((HANDLE)0xFFFFFFFF)
#define NULL_HANDLE         ((HANDLE)0)

/* ── NTSTATUS ───────────────────────────────────────────────────────────── */
typedef uint32_t            NTSTATUS;

/* ── Boolean helpers ────────────────────────────────────────────────────── */
#define TRUE    1
#define FALSE   0

/* ── Utility macros ─────────────────────────────────────────────────────── */
#define ALIGN_UP(x, a)      (((x) + ((a)-1)) & ~((a)-1))
#define ALIGN_DOWN(x, a)    ((x) & ~((a)-1))
#define ARRAY_SIZE(a)       (sizeof(a) / sizeof((a)[0]))
#define MIN(a, b)           ((a) < (b) ? (a) : (b))
#define MAX(a, b)           ((a) > (b) ? (a) : (b))

/* ── NT calling convention (no-op on x86_64 SysV, kept for aesthetics) ─── */
#define NTAPI
#define WINAPI
#define NTKERNELAPI

/* ── Kernel-mode assert ─────────────────────────────────────────────────── */
#define ASSERT(expr) \
    do { if (!(expr)) KeBugCheckEx(0x0000008E, (ULONG_PTR)__FILE__, \
        (ULONG_PTR)__LINE__, 0, 0); } while(0)
