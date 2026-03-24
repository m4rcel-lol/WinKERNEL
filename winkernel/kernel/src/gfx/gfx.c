/* WinKernel NTKRNL-X — Graphics subsystem (Limine framebuffer) */

#include <kernel/gfx.h>
#include <kernel/terminal.h>
#include <kernel/rtl.h>
#include <kernel/io.h>
#include <ntdef.h>
#include <ntstatus.h>
#include <kernel/limine_fb.h>

/* ── Driver state ───────────────────────────────────────────────────────── */
static GFX_MODE g_Mode;
static BOOL     g_Ready = FALSE;
static BYTE*    g_FbBytes = NULL;

static VOID _GfxPutPixel(DWORD X, DWORD Y, DWORD Rgb) {
    if (!g_Ready || !g_FbBytes || X >= g_Mode.Width || Y >= g_Mode.Height) return;
    BYTE* row = g_FbBytes + Y * g_Mode.Pitch;
    if (g_Mode.Bpp == 32) {
        ((DWORD*)row)[X] = Rgb;
    } else if (g_Mode.Bpp == 24) {
        BYTE* p = row + X * 3;
        p[0] = (BYTE)(Rgb & 0xFF);
        p[1] = (BYTE)((Rgb >> 8) & 0xFF);
        p[2] = (BYTE)((Rgb >> 16) & 0xFF);
    }
}

/* ── GfxInitialize ──────────────────────────────────────────────────────── */
NTSTATUS GfxInitialize(VOID) {
    g_Ready = FALSE;
    g_FbBytes = NULL;

    if (!g_LimineFramebufferRequest.response ||
        g_LimineFramebufferRequest.response->framebuffer_count == 0) {
        return STATUS_DEVICE_NOT_CONNECTED;
    }

    struct limine_framebuffer* fb =
        g_LimineFramebufferRequest.response->framebuffers[0];
    g_Mode.Bpp = (DWORD)fb->bpp;
    if (g_Mode.Bpp != 32 && g_Mode.Bpp != 24) {
        return STATUS_NOT_IMPLEMENTED;
    }

    g_FbBytes          = (BYTE*)fb->address;
    g_Mode.Framebuffer = (DWORD*)fb->address;
    g_Mode.Width        = (DWORD)fb->width;
    g_Mode.Height       = (DWORD)fb->height;
    g_Mode.Pitch        = (DWORD)fb->pitch;
    g_Ready             = TRUE;

    (VOID)IoRegisterLoadedDriver("Basic Framebuffer Display");

    return STATUS_SUCCESS;
}

/* ── GfxGetMode ─────────────────────────────────────────────────────────── */
VOID GfxGetMode(PGFX_MODE Mode) {
    if (Mode) *Mode = g_Mode;
}

/* ── GfxDrawPixel ───────────────────────────────────────────────────────── */
VOID GfxDrawPixel(DWORD X, DWORD Y, DWORD Color) {
    _GfxPutPixel(X, Y, Color);
}

/* ── GfxFillRect ────────────────────────────────────────────────────────── */
VOID GfxFillRect(DWORD X, DWORD Y, DWORD W, DWORD H, DWORD Color) {
    if (!g_Ready) return;
    DWORD x2 = X + W; if (x2 > g_Mode.Width)  x2 = g_Mode.Width;
    DWORD y2 = Y + H; if (y2 > g_Mode.Height) y2 = g_Mode.Height;
    for (DWORD y = Y; y < y2; y++) {
        for (DWORD x = X; x < x2; x++) _GfxPutPixel(x, y, Color);
    }
}

/* ── GfxFillScreen ──────────────────────────────────────────────────────── */
VOID GfxFillScreen(DWORD Color) {
    GfxFillRect(0, 0, g_Mode.Width, g_Mode.Height, Color);
}

/* ── GfxDrawHLine ───────────────────────────────────────────────────────── */
VOID GfxDrawHLine(DWORD X, DWORD Y, DWORD Len, DWORD Color) {
    GfxFillRect(X, Y, Len, 1, Color);
}

/* ── GfxDrawVLine ───────────────────────────────────────────────────────── */
VOID GfxDrawVLine(DWORD X, DWORD Y, DWORD Len, DWORD Color) {
    if (!g_Ready) return;
    DWORD y2 = Y + Len; if (y2 > g_Mode.Height) y2 = g_Mode.Height;
    if (X >= g_Mode.Width) return;
    for (DWORD y = Y; y < y2; y++) _GfxPutPixel(X, y, Color);
}

/* ── GfxDrawRect ────────────────────────────────────────────────────────── */
VOID GfxDrawRect(DWORD X, DWORD Y, DWORD W, DWORD H, DWORD Color) {
    GfxDrawHLine(X,         Y,         W, Color);
    GfxDrawHLine(X,         Y + H - 1, W, Color);
    GfxDrawVLine(X,         Y,         H, Color);
    GfxDrawVLine(X + W - 1, Y,         H, Color);
}

/* ── GfxBlit ────────────────────────────────────────────────────────────── */
VOID GfxBlit(DWORD X, DWORD Y, DWORD W, DWORD H, const DWORD* Pixels) {
    if (!g_Ready || !Pixels) return;
    for (DWORD row = 0; row < H; row++) {
        DWORD dy = Y + row;
        if (dy >= g_Mode.Height) break;
        for (DWORD col = 0; col < W; col++) {
            DWORD dx = X + col;
            if (dx >= g_Mode.Width) break;
            _GfxPutPixel(dx, dy, Pixels[row * W + col]);
        }
    }
}

/* ── GfxScrollUp ────────────────────────────────────────────────────────── */
VOID GfxScrollUp(DWORD Lines, DWORD BgColor) {
    if (!g_Ready || Lines == 0 || !g_FbBytes) return;
    if (Lines >= g_Mode.Height) {
        GfxFillScreen(BgColor);
        return;
    }
    DWORD move_rows = g_Mode.Height - Lines;
    SIZE_T span = (SIZE_T)move_rows * g_Mode.Pitch;
    RtlCopyMemory(g_FbBytes, g_FbBytes + (SIZE_T)Lines * g_Mode.Pitch, span);
    GfxFillRect(0, move_rows, g_Mode.Width, Lines, BgColor);
}
