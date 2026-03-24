#pragma once

#include <ntdef.h>
#include <stdarg.h>

/* ── Memory operations ──────────────────────────────────────────────────── */
VOID    RtlCopyMemory(PVOID Dst, const VOID* Src, SIZE_T Len);
VOID    RtlZeroMemory(PVOID Dst, SIZE_T Len);
VOID    RtlFillMemory(PVOID Dst, SIZE_T Len, BYTE Value);
LONG    RtlCompareMemory(const VOID* A, const VOID* B, SIZE_T Len);

/* ── String operations ──────────────────────────────────────────────────── */
SIZE_T  RtlStringLength(PCSTR Str);
VOID    RtlCopyString(PSTR Dst, PCSTR Src, SIZE_T MaxLen);
LONG    RtlCompareString(PCSTR A, PCSTR B);
LONG    RtlCompareStringN(PCSTR A, PCSTR B, SIZE_T N);
VOID    RtlAppendString(PSTR Dst, PCSTR Src, SIZE_T MaxLen);
BOOL    RtlStringStartsWith(PCSTR Str, PCSTR Prefix);

/* ── Formatted output ───────────────────────────────────────────────────── */
LONG    RtlVprintf(PSTR Buffer, SIZE_T BufSize, PCSTR Format, va_list Args);
LONG    RtlPrintf(PSTR Buffer, SIZE_T BufSize, PCSTR Format, ...);

/* ── Number conversion ──────────────────────────────────────────────────── */
VOID    RtlUlongToHexString(ULONG_PTR Value, PSTR Buffer, BOOL UpperCase);
VOID    RtlUlongToDecString(ULONG_PTR Value, PSTR Buffer);
LONG    RtlStringToLong(PCSTR Str);
