/* WinKernel NTKRNL-X — Graphics subsystem (Limine framebuffer) */

#include <kernel/gfx.h>
#include <kernel/terminal.h>
#include <kernel/rtl.h>
#include <ntdef.h>
#include <limine.h>

/* ── Limine framebuffer request (shared with terminal.c via separate req) ─ */
/* We issue our own request; Limine deduplicates identical request IDs and
   returns the same response to both. */
__attribute__((used, section(".limine_requests")))
static volatile struct limine_framebuffer_request gfx_fb_request = {
    .id       = LIMINE_FRAMEBUFFER_REQUEST,
    .revision = 0,
    .response = NULL
};

/* ── Driver state ───────────────────────────────────────────────────────── */
static GFX_MODE g_Mode;
static BOOL     g_Ready = FALSE;

/* ── GfxInitialize ──────────────────────────────────────────────────────── */
NTSTATUS GfxInitialize(VOID) {
    g_Ready = FALSE;

    if (!gfx_fb_request.response ||
        gfx_fb_request.response->framebuffer_count == 0) {
        return STATUS_DEVICE_NOT_CONNECTED;
    }

    struct limine_framebuffer* fb = gfx_fb_request.response->framebuffers[0];
    g_Mode.Framebuffer = (DWORD*)fb->address;
    g_Mode.Width       = (DWORD)fb->width;
    g_Mode.Height      = (DWORD)fb->height;
    g_Mode.Pitch       = (DWORD)fb->pitch;
    g_Mode.Bpp         = (DWORD)fb->bpp;
    g_Ready            = TRUE;

    return STATUS_SUCCESS;
}

/* ── GfxGetMode ─────────────────────────────────────────────────────────── */
VOID GfxGetMode(PGFX_MODE Mode) {
    if (Mode) *Mode = g_Mode;
}

/* ── GfxDrawPixel ───────────────────────────────────────────────────────── */
VOID GfxDrawPixel(DWORD X, DWORD Y, DWORD Color) {
    if (!g_Ready || X >= g_Mode.Width || Y >= g_Mode.Height) return;
    g_Mode.Framebuffer[Y * (g_Mode.Pitch / 4) + X] = Color;
}

/* ── GfxFillRect ────────────────────────────────────────────────────────── */
VOID GfxFillRect(DWORD X, DWORD Y, DWORD W, DWORD H, DWORD Color) {
    if (!g_Ready) return;
    DWORD x2 = X + W; if (x2 > g_Mode.Width)  x2 = g_Mode.Width;
    DWORD y2 = Y + H; if (y2 > g_Mode.Height) y2 = g_Mode.Height;
    DWORD stride = g_Mode.Pitch / 4;
    for (DWORD y = Y; y < y2; y++) {
        DWORD* row = g_Mode.Framebuffer + y * stride + X;
        for (DWORD x = 0; x < (x2 - X); x++) row[x] = Color;
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
    DWORD stride = g_Mode.Pitch / 4;
    DWORD y2 = Y + Len; if (y2 > g_Mode.Height) y2 = g_Mode.Height;
    if (X >= g_Mode.Width) return;
    for (DWORD y = Y; y < y2; y++)
        g_Mode.Framebuffer[y * stride + X] = Color;
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
    DWORD stride = g_Mode.Pitch / 4;
    for (DWORD row = 0; row < H; row++) {
        DWORD dy = Y + row;
        if (dy >= g_Mode.Height) break;
        for (DWORD col = 0; col < W; col++) {
            DWORD dx = X + col;
            if (dx >= g_Mode.Width) break;
            g_Mode.Framebuffer[dy * stride + dx] = Pixels[row * W + col];
        }
    }
}

/* ── GfxScrollUp ────────────────────────────────────────────────────────── */
VOID GfxScrollUp(DWORD Lines, DWORD BgColor) {
    if (!g_Ready || Lines == 0) return;
    DWORD stride = g_Mode.Pitch / 4;
    if (Lines >= g_Mode.Height) {
        GfxFillScreen(BgColor);
        return;
    }
    /* Move rows up */
    DWORD move_rows = g_Mode.Height - Lines;
    RtlCopyMemory(g_Mode.Framebuffer,
                  g_Mode.Framebuffer + Lines * stride,
                  move_rows * stride * 4);
    /* Clear bottom */
    GfxFillRect(0, move_rows, g_Mode.Width, Lines, BgColor);
}
