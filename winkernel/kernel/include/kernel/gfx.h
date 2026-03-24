#pragma once
#include <ntdef.h>
#include <ntstatus.h>

/* ── Pixel format ───────────────────────────────────────────────────────── */
#define GFX_RGB(r,g,b)  (((DWORD)(r)<<16)|((DWORD)(g)<<8)|(DWORD)(b))
#define GFX_BLACK       0x000000
#define GFX_WHITE       0xFFFFFF
#define GFX_RED         0xFF0000
#define GFX_GREEN       0x00FF00
#define GFX_BLUE        0x0000FF
#define GFX_CYAN        0x00FFFF
#define GFX_YELLOW      0xFFFF00
#define GFX_GREY        0xC0C0C0
#define GFX_DARK_GREY   0x808080
#define GFX_NT_BLUE     0x0000AA

/* ── Framebuffer info ───────────────────────────────────────────────────── */
typedef struct _GFX_MODE {
    DWORD*  Framebuffer;
    DWORD   Width;
    DWORD   Height;
    DWORD   Pitch;      /* bytes per row */
    DWORD   Bpp;
} GFX_MODE, *PGFX_MODE;

/* ── Gfx interface ──────────────────────────────────────────────────────── */
NTSTATUS    GfxInitialize(VOID);
VOID        GfxGetMode(PGFX_MODE Mode);
VOID        GfxFillScreen(DWORD Color);
VOID        GfxFillRect(DWORD X, DWORD Y, DWORD W, DWORD H, DWORD Color);
VOID        GfxDrawPixel(DWORD X, DWORD Y, DWORD Color);
VOID        GfxDrawHLine(DWORD X, DWORD Y, DWORD Len, DWORD Color);
VOID        GfxDrawVLine(DWORD X, DWORD Y, DWORD Len, DWORD Color);
VOID        GfxDrawRect(DWORD X, DWORD Y, DWORD W, DWORD H, DWORD Color);
VOID        GfxBlit(DWORD X, DWORD Y, DWORD W, DWORD H, const DWORD* Pixels);
VOID        GfxScrollUp(DWORD Lines, DWORD Color);
