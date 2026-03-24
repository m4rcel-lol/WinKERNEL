/* WinKernel NTKRNL-X — RtlPrintf / RtlVprintf
   Supports: %s %d %i %u %x %X %p %c %% with width and zero-padding */

#include <kernel/rtl.h>
#include <ntdef.h>
#include <stdarg.h>

/* ── Internal helpers ───────────────────────────────────────────────────── */

static VOID _AppendChar(PSTR Buf, SIZE_T BufSize, SIZE_T* Pos, CHAR c) {
    if (*Pos < BufSize - 1) {
        Buf[(*Pos)++] = c;
        Buf[*Pos]     = '\0';
    }
}

static VOID _AppendStr(PSTR Buf, SIZE_T BufSize, SIZE_T* Pos, PCSTR s,
                       INT Width, BOOL LeftAlign, CHAR PadChar) {
    if (!s) s = "(null)";
    INT slen = 0;
    while (s[slen]) slen++;

    INT pad = (Width > slen) ? (Width - slen) : 0;

    if (!LeftAlign) {
        for (INT i = 0; i < pad; i++) _AppendChar(Buf, BufSize, Pos, PadChar);
    }
    for (INT i = 0; i < slen; i++) _AppendChar(Buf, BufSize, Pos, s[i]);
    if (LeftAlign) {
        for (INT i = 0; i < pad; i++) _AppendChar(Buf, BufSize, Pos, ' ');
    }
}

static VOID _AppendUlong(PSTR Buf, SIZE_T BufSize, SIZE_T* Pos,
                         ULONG_PTR Value, INT Base, BOOL Upper,
                         INT Width, BOOL LeftAlign, CHAR PadChar) {
    const char* digits = Upper ? "0123456789ABCDEF" : "0123456789abcdef";
    char tmp[65];
    INT  tpos = 0;

    if (Value == 0) {
        tmp[tpos++] = '0';
    } else {
        ULONG_PTR v = Value;
        while (v > 0) {
            tmp[tpos++] = digits[v % (ULONG_PTR)Base];
            v /= (ULONG_PTR)Base;
        }
    }

    /* reverse into number string */
    char numstr[65];
    for (INT i = 0; i < tpos; i++) numstr[i] = tmp[tpos - 1 - i];
    numstr[tpos] = '\0';

    _AppendStr(Buf, BufSize, Pos, numstr, Width, LeftAlign, PadChar);
}

static VOID _AppendLong(PSTR Buf, SIZE_T BufSize, SIZE_T* Pos,
                        LONGLONG Value, INT Width, BOOL LeftAlign, CHAR PadChar) {
    char tmp[22];
    INT  tpos = 0;
    BOOL neg  = FALSE;

    if (Value < 0) { neg = TRUE; Value = -Value; }
    if (Value == 0) {
        tmp[tpos++] = '0';
    } else {
        ULONGLONG v = (ULONGLONG)Value;
        while (v > 0) {
            tmp[tpos++] = (CHAR)('0' + (v % 10));
            v /= 10;
        }
    }
    if (neg) tmp[tpos++] = '-';

    char numstr[22];
    for (INT i = 0; i < tpos; i++) numstr[i] = tmp[tpos - 1 - i];
    numstr[tpos] = '\0';

    _AppendStr(Buf, BufSize, Pos, numstr, Width, LeftAlign, PadChar);
}

/* ── RtlVprintf ─────────────────────────────────────────────────────────── */

LONG RtlVprintf(PSTR Buffer, SIZE_T BufSize, PCSTR Format, va_list Args) {
    if (!Buffer || BufSize == 0 || !Format) return -1;

    SIZE_T pos = 0;
    Buffer[0]  = '\0';

    for (SIZE_T i = 0; Format[i]; i++) {
        if (Format[i] != '%') {
            _AppendChar(Buffer, BufSize, &pos, Format[i]);
            continue;
        }

        i++;
        if (!Format[i]) break;

        /* flags */
        BOOL  LeftAlign = FALSE;
        CHAR  PadChar   = ' ';
        BOOL  LongLong  = FALSE;

        if (Format[i] == '-') { LeftAlign = TRUE; i++; }
        if (Format[i] == '0') { PadChar   = '0';  i++; }

        /* width */
        INT Width = 0;
        while (Format[i] >= '0' && Format[i] <= '9') {
            Width = Width * 10 + (Format[i] - '0');
            i++;
        }

        /* length modifier */
        if (Format[i] == 'l') {
            i++;
            if (Format[i] == 'l') { LongLong = TRUE; i++; }
        }

        switch (Format[i]) {
        case 'c':
            _AppendChar(Buffer, BufSize, &pos, (CHAR)va_arg(Args, INT));
            break;

        case 's': {
            PCSTR s = va_arg(Args, PCSTR);
            _AppendStr(Buffer, BufSize, &pos, s, Width, LeftAlign, ' ');
            break;
        }

        case 'd':
        case 'i': {
            LONGLONG v = LongLong ? va_arg(Args, LONGLONG) : (LONGLONG)va_arg(Args, INT);
            _AppendLong(Buffer, BufSize, &pos, v, Width, LeftAlign, PadChar);
            break;
        }

        case 'u': {
            ULONG_PTR v = LongLong ? (ULONG_PTR)va_arg(Args, ULONGLONG)
                                   : (ULONG_PTR)va_arg(Args, ULONG);
            _AppendUlong(Buffer, BufSize, &pos, v, 10, FALSE, Width, LeftAlign, PadChar);
            break;
        }

        case 'x': {
            ULONG_PTR v = LongLong ? (ULONG_PTR)va_arg(Args, ULONGLONG)
                                   : (ULONG_PTR)va_arg(Args, ULONG);
            _AppendUlong(Buffer, BufSize, &pos, v, 16, FALSE, Width, LeftAlign, PadChar);
            break;
        }

        case 'X': {
            ULONG_PTR v = LongLong ? (ULONG_PTR)va_arg(Args, ULONGLONG)
                                   : (ULONG_PTR)va_arg(Args, ULONG);
            _AppendUlong(Buffer, BufSize, &pos, v, 16, TRUE, Width, LeftAlign, PadChar);
            break;
        }

        case 'p': {
            ULONG_PTR v = (ULONG_PTR)va_arg(Args, PVOID);
            _AppendStr(Buffer, BufSize, &pos, "0x", 0, FALSE, ' ');
            _AppendUlong(Buffer, BufSize, &pos, v, 16, FALSE, 16, FALSE, '0');
            break;
        }

        case '%':
            _AppendChar(Buffer, BufSize, &pos, '%');
            break;

        default:
            _AppendChar(Buffer, BufSize, &pos, '%');
            _AppendChar(Buffer, BufSize, &pos, Format[i]);
            break;
        }
    }

    return (LONG)pos;
}

/* ── RtlPrintf ──────────────────────────────────────────────────────────── */

LONG RtlPrintf(PSTR Buffer, SIZE_T BufSize, PCSTR Format, ...) {
    va_list args;
    va_start(args, Format);
    LONG result = RtlVprintf(Buffer, BufSize, Format, args);
    va_end(args);
    return result;
}
