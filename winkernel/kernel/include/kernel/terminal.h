#pragma once

#include <ntdef.h>

/* ── Console colors (Windows color attribute nibbles) ───────────────────── */
#define CON_BLACK           0x0
#define CON_DARK_BLUE       0x1
#define CON_DARK_GREEN      0x2
#define CON_DARK_CYAN       0x3
#define CON_DARK_RED        0x4
#define CON_DARK_MAGENTA    0x5
#define CON_DARK_YELLOW     0x6
#define CON_GREY            0x7
#define CON_DARK_GREY       0x8
#define CON_BLUE            0x9
#define CON_GREEN           0xA
#define CON_CYAN            0xB
#define CON_RED             0xC
#define CON_MAGENTA         0xD
#define CON_YELLOW          0xE
#define CON_WHITE           0xF

/* ── Default console colors ─────────────────────────────────────────────── */
#define CON_DEFAULT_FG      CON_GREY
#define CON_DEFAULT_BG      CON_BLACK

/* ── RGB color packing ──────────────────────────────────────────────────── */
#define RGB(r,g,b)  (((DWORD)(r) << 16) | ((DWORD)(g) << 8) | (DWORD)(b))

/* ── Console interface ──────────────────────────────────────────────────── */
VOID    WinConsole_Init(VOID);
VOID    KdClearScreen(VOID);
VOID    KdPrint(PCSTR Text);
VOID    KdPrintColor(PCSTR Text, BYTE Fg, BYTE Bg);
VOID    KdPrintf(PCSTR Format, ...);
VOID    KdPrintfColor(BYTE Fg, BYTE Bg, PCSTR Format, ...);
VOID    KdScroll(VOID);
VOID    KdSetColor(BYTE Fg, BYTE Bg);
VOID    KdGetColor(BYTE* Fg, BYTE* Bg);
VOID    KdMoveCursor(DWORD Col, DWORD Row);
VOID    KdGetCursor(DWORD* Col, DWORD* Row);
VOID    KdPutChar(CHAR c);
VOID    KdBackspace(VOID);
DWORD   KdGetCols(VOID);
DWORD   KdGetRows(VOID);
