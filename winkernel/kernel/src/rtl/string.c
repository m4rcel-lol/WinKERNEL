/* WinKernel NTKRNL-X — RTL string and memory operations */

#include <kernel/rtl.h>
#include <string.h>
#include <ntdef.h>

/* ── Memory ─────────────────────────────────────────────────────────────── */

VOID RtlCopyMemory(PVOID Dst, const VOID* Src, SIZE_T Len) {
    BYTE*       d = (BYTE*)Dst;
    const BYTE* s = (const BYTE*)Src;
    for (SIZE_T i = 0; i < Len; i++) d[i] = s[i];
}

VOID RtlZeroMemory(PVOID Dst, SIZE_T Len) {
    BYTE* d = (BYTE*)Dst;
    for (SIZE_T i = 0; i < Len; i++) d[i] = 0;
}

VOID RtlFillMemory(PVOID Dst, SIZE_T Len, BYTE Value) {
    BYTE* d = (BYTE*)Dst;
    for (SIZE_T i = 0; i < Len; i++) d[i] = Value;
}

LONG RtlCompareMemory(const VOID* A, const VOID* B, SIZE_T Len) {
    const BYTE* a = (const BYTE*)A;
    const BYTE* b = (const BYTE*)B;
    for (SIZE_T i = 0; i < Len; i++) {
        if (a[i] < b[i]) return -1;
        if (a[i] > b[i]) return  1;
    }
    return 0;
}

/* ── String ─────────────────────────────────────────────────────────────── */

SIZE_T RtlStringLength(PCSTR Str) {
    SIZE_T n = 0;
    if (!Str) return 0;
    while (Str[n]) n++;
    return n;
}

VOID RtlCopyString(PSTR Dst, PCSTR Src, SIZE_T MaxLen) {
    if (!Dst || !Src || MaxLen == 0) return;
    SIZE_T i = 0;
    while (i < MaxLen - 1 && Src[i]) {
        Dst[i] = Src[i];
        i++;
    }
    Dst[i] = '\0';
}

LONG RtlCompareString(PCSTR A, PCSTR B) {
    if (!A && !B) return 0;
    if (!A) return -1;
    if (!B) return  1;
    while (*A && *B && *A == *B) { A++; B++; }
    return (LONG)(unsigned char)*A - (LONG)(unsigned char)*B;
}

LONG RtlCompareStringN(PCSTR A, PCSTR B, SIZE_T N) {
    if (!A && !B) return 0;
    if (!A) return -1;
    if (!B) return  1;
    SIZE_T i = 0;
    while (i < N && A[i] && B[i] && A[i] == B[i]) i++;
    if (i == N) return 0;
    return (LONG)(unsigned char)A[i] - (LONG)(unsigned char)B[i];
}

VOID RtlAppendString(PSTR Dst, PCSTR Src, SIZE_T MaxLen) {
    if (!Dst || !Src || MaxLen == 0) return;
    SIZE_T dlen = RtlStringLength(Dst);
    SIZE_T i = 0;
    while (dlen + i < MaxLen - 1 && Src[i]) {
        Dst[dlen + i] = Src[i];
        i++;
    }
    Dst[dlen + i] = '\0';
}

BOOL RtlStringStartsWith(PCSTR Str, PCSTR Prefix) {
    if (!Str || !Prefix) return FALSE;
    while (*Prefix) {
        if (*Str != *Prefix) return FALSE;
        Str++; Prefix++;
    }
    return TRUE;
}

/* ── Number conversion ──────────────────────────────────────────────────── */

VOID RtlUlongToHexString(ULONG_PTR Value, PSTR Buffer, BOOL UpperCase) {
    const char* digits = UpperCase ? "0123456789ABCDEF" : "0123456789abcdef";
    char tmp[17];
    INT  pos = 0;

    if (Value == 0) {
        Buffer[0] = '0';
        Buffer[1] = '\0';
        return;
    }

    while (Value > 0) {
        tmp[pos++] = digits[Value & 0xF];
        Value >>= 4;
    }

    /* reverse */
    for (INT i = 0; i < pos; i++)
        Buffer[i] = tmp[pos - 1 - i];
    Buffer[pos] = '\0';
}

VOID RtlUlongToDecString(ULONG_PTR Value, PSTR Buffer) {
    char tmp[21];
    INT  pos = 0;

    if (Value == 0) {
        Buffer[0] = '0';
        Buffer[1] = '\0';
        return;
    }

    while (Value > 0) {
        tmp[pos++] = (CHAR)('0' + (Value % 10));
        Value /= 10;
    }

    for (INT i = 0; i < pos; i++)
        Buffer[i] = tmp[pos - 1 - i];
    Buffer[pos] = '\0';
}

LONG RtlStringToLong(PCSTR Str) {
    if (!Str) return 0;
    LONG  result = 0;
    BOOL  neg    = FALSE;
    if (*Str == '-') { neg = TRUE; Str++; }
    while (*Str >= '0' && *Str <= '9') {
        result = result * 10 + (*Str - '0');
        Str++;
    }
    return neg ? -result : result;
}

/* ── C standard wrappers (required by compiler builtins) ────────────────── */

void* memcpy(void* dst, const void* src, size_t n) {
    RtlCopyMemory(dst, src, n);
    return dst;
}

void* memset(void* dst, int c, size_t n) {
    RtlFillMemory(dst, n, (BYTE)c);
    return dst;
}

void* memmove(void* dst, const void* src, size_t n) {
    BYTE*       d = (BYTE*)dst;
    const BYTE* s = (const BYTE*)src;
    if (d < s) {
        for (size_t i = 0; i < n; i++) d[i] = s[i];
    } else {
        for (size_t i = n; i > 0; i--) d[i-1] = s[i-1];
    }
    return dst;
}

int memcmp(const void* a, const void* b, size_t n) {
    return (int)RtlCompareMemory(a, b, n);
}

size_t strlen(const char* s) {
    return RtlStringLength(s);
}

char* strcpy(char* dst, const char* src) {
    RtlCopyString(dst, src, RtlStringLength(src) + 1);
    return dst;
}

char* strncpy(char* dst, const char* src, size_t n) {
    RtlCopyString(dst, src, n);
    return dst;
}

int strcmp(const char* a, const char* b) {
    return (int)RtlCompareString(a, b);
}

int strncmp(const char* a, const char* b, size_t n) {
    return (int)RtlCompareStringN(a, b, n);
}

char* strcat(char* dst, const char* src) {
    RtlAppendString(dst, src, RtlStringLength(dst) + RtlStringLength(src) + 1);
    return dst;
}

char* strchr(const char* s, int c) {
    while (*s) {
        if (*s == (char)c) return (char*)s;
        s++;
    }
    return (c == '\0') ? (char*)s : NULL;
}

char* strrchr(const char* s, int c) {
    const char* last = NULL;
    while (*s) {
        if (*s == (char)c) last = s;
        s++;
    }
    if (c == '\0') return (char*)s;
    return (char*)last;
}
