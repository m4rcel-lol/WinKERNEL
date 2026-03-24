/* WinKernel NTKRNL-X — cmd.exe-style shell */

#include <kernel/shell.h>
#include <kernel/terminal.h>
#include <kernel/io.h>
#include <kernel/mm.h>
#include <kernel/hal.h>
#include <kernel/rtl.h>
#include <kernel/bsod.h>
#include <kernel/ps.h>
#include <kernel/net.h>
#include <kernel/gfx.h>
#include <ntdef.h>
#include <ntstatus.h>

/* ── Command names for TAB completion ------------------------------------- */
static const CHAR* g_Commands[] = {
    "help", "cls", "echo", "ver", "mem", "sysinfo",
    "time", "set", "dir", "crash", "reboot", "shutdown", "color",
    "net", "netping", "gfxtest", "drivers", "fetch",
    NULL
};

/* ── History -----------------------------------------───────────────────── */
static CHAR g_History[SHELL_HISTORY_MAX][SHELL_INPUT_MAX];
static INT  g_HistoryCount = 0;
static INT  g_HistoryPos   = -1;

/* ── Environment variables -----------------------------------------─────── */
static SHELL_ENV_VAR g_Env[SHELL_ENV_MAX];

/* ── Input buffer -----------------------------------------──────────────── */
static CHAR g_InputBuf[SHELL_INPUT_MAX];
static INT  g_InputLen  = 0;
static INT  g_CursorPos = 0;

/* ── Forward declarations -----------------------------------------──────── */
static VOID _ShellExecute(PCSTR Line);
static VOID _ShellRedrawLine(VOID);

/* ── Shell_PrintBanner -----------------------------------------─────────── */
VOID Shell_PrintBanner(VOID) {
    KdPrintColor("WinKernel Version 0.1.0 [x86_64]\n", CON_WHITE, CON_BLACK);
    KdPrintColor("(c) WinKernel Project. All rights reserved.\n\n", CON_GREY, CON_BLACK);
}

/* ── _PrintPrompt -----------------------------------------──────────────── */
static VOID _PrintPrompt(VOID) {
    KdPrintColor("C:\\> ", CON_WHITE, CON_BLACK);
}

/* ── _HistoryAdd -----------------------------------------───────────────── */
static VOID _HistoryAdd(PCSTR Line) {
    if (RtlStringLength(Line) == 0) return;
    /* Shift history up */
    if (g_HistoryCount == SHELL_HISTORY_MAX) {
        for (INT i = 0; i < SHELL_HISTORY_MAX - 1; i++)
            RtlCopyString(g_History[i], g_History[i+1], SHELL_INPUT_MAX);
        g_HistoryCount--;
    }
    RtlCopyString(g_History[g_HistoryCount], Line, SHELL_INPUT_MAX);
    g_HistoryCount++;
    g_HistoryPos = g_HistoryCount;
}

/* ── _ShellRedrawLine — reprint input line from cursor ──────────────────── */
static VOID _ShellRedrawLine(VOID) {
    DWORD col, row;
    KdGetCursor(&col, &row);
    DWORD save_col = col;
    /* Print remaining chars from cursor position in buffer */
    for (INT i = g_CursorPos; i < g_InputLen; i++) KdPutChar(g_InputBuf[i]);
    /* Erase any leftover characters */
    KdPutChar(' ');
    /* Move cursor back to saved position */
    KdMoveCursor(save_col, row);
}

/* ── _TabComplete -----------------------------------------──────────────── */
static VOID _TabComplete(VOID) {
    /* Find matching command */
    PCSTR match = NULL;
    INT   matches = 0;
    for (INT i = 0; g_Commands[i]; i++) {
        if (RtlStringStartsWith(g_Commands[i], g_InputBuf)) {
            match = g_Commands[i];
            matches++;
        }
    }
    if (matches == 1 && match) {
        /* Complete the command */
        RtlCopyString(g_InputBuf, match, SHELL_INPUT_MAX);
        g_InputLen  = (INT)RtlStringLength(g_InputBuf);
        g_CursorPos = g_InputLen;
        /* Redraw */
        DWORD col, row;
        KdGetCursor(&col, &row);
        /* Go back to start of line */
        DWORD prompt_len = 5; /* "C:\> " */
        KdMoveCursor(prompt_len, row);
        DWORD cols = KdGetCols();
        for (DWORD i = prompt_len; i < cols; i++) KdPutChar(' ');
        KdMoveCursor(prompt_len, row);
        KdPrint(g_InputBuf);
    }
}

/* ── Shell_Run -----------------------------------------─────────────────── */
VOID Shell_Run(VOID) {
    Shell_PrintBanner();

    while (TRUE) {
        _PrintPrompt();
        g_InputLen  = 0;
        g_CursorPos = 0;
        g_HistoryPos = g_HistoryCount;
        RtlZeroMemory(g_InputBuf, sizeof(g_InputBuf));

        while (TRUE) {
            KEY_EVENT ev;
            while (!IoConsoleReadEvent(&ev))
                __asm__ volatile ("hlt");   /* sleep until next IRQ wakes us */
            if (ev.Released) continue;

            CHAR c = ev.Ascii;

            /* Enter */
            if (c == '\n' || c == '\r') {
                KdPutChar('\n');
                g_InputBuf[g_InputLen] = '\0';
                _HistoryAdd(g_InputBuf);
                _ShellExecute(g_InputBuf);
                break;
            }

            /* Backspace */
            if (c == '\b') {
                if (g_CursorPos > 0) {
                    /* Shift buffer left */
                    for (INT i = g_CursorPos - 1; i < g_InputLen - 1; i++)
                        g_InputBuf[i] = g_InputBuf[i+1];
                    g_InputLen--;
                    g_CursorPos--;
                    g_InputBuf[g_InputLen] = '\0';
                    KdBackspace();
                    _ShellRedrawLine();
                }
                continue;
            }

            /* Tab completion */
            if (c == '\t') {
                _TabComplete();
                continue;
            }

            /* Arrow keys come as scancode with ascii=0 */
            if (c == 0) {
                /* Up arrow = scancode 0x48 */
                if (ev.Scancode == 0x48) {
                    if (g_HistoryPos > 0) {
                        g_HistoryPos--;
                        RtlCopyString(g_InputBuf, g_History[g_HistoryPos], SHELL_INPUT_MAX);
                        g_InputLen  = (INT)RtlStringLength(g_InputBuf);
                        g_CursorPos = g_InputLen;
                        /* Redraw */
                        DWORD col, row;
                        KdGetCursor(&col, &row);
                        KdMoveCursor(5, row);
                        DWORD cols = KdGetCols();
                        for (DWORD i = 5; i < cols; i++) KdPutChar(' ');
                        KdMoveCursor(5, row);
                        KdPrint(g_InputBuf);
                    }
                }
                /* Down arrow = scancode 0x50 */
                else if (ev.Scancode == 0x50) {
                    if (g_HistoryPos < g_HistoryCount - 1) {
                        g_HistoryPos++;
                        RtlCopyString(g_InputBuf, g_History[g_HistoryPos], SHELL_INPUT_MAX);
                    } else {
                        g_HistoryPos = g_HistoryCount;
                        RtlZeroMemory(g_InputBuf, sizeof(g_InputBuf));
                    }
                    g_InputLen  = (INT)RtlStringLength(g_InputBuf);
                    g_CursorPos = g_InputLen;
                    DWORD col, row;
                    KdGetCursor(&col, &row);
                    KdMoveCursor(5, row);
                    DWORD cols = KdGetCols();
                    for (DWORD i = 5; i < cols; i++) KdPutChar(' ');
                    KdMoveCursor(5, row);
                    KdPrint(g_InputBuf);
                }
                /* Left arrow = scancode 0x4B */
                else if (ev.Scancode == 0x4B) {
                    if (g_CursorPos > 0) {
                        g_CursorPos--;
                        DWORD col, row;
                        KdGetCursor(&col, &row);
                        if (col > 5) KdMoveCursor(col - 1, row);
                    }
                }
                /* Right arrow = scancode 0x4D */
                else if (ev.Scancode == 0x4D) {
                    if (g_CursorPos < g_InputLen) {
                        g_CursorPos++;
                        DWORD col, row;
                        KdGetCursor(&col, &row);
                        KdMoveCursor(col + 1, row);
                    }
                }
                continue;
            }

            /* Regular printable character */
            if (g_InputLen < SHELL_INPUT_MAX - 1) {
                /* Insert at cursor position */
                for (INT i = g_InputLen; i > g_CursorPos; i--)
                    g_InputBuf[i] = g_InputBuf[i-1];
                g_InputBuf[g_CursorPos] = c;
                g_InputLen++;
                g_CursorPos++;
                g_InputBuf[g_InputLen] = '\0';
                KdPutChar(c);
                _ShellRedrawLine();
            }
        }
    }
}

/* ════════════════════════════════════════════════════════════════════════════
   COMMAND IMPLEMENTATIONS
   ════════════════════════════════════════════════════════════════════════════ */

static VOID _CmdHelp(VOID) {
    KdPrintColor("WinKernel NTKRNL-X Command Reference\n", CON_WHITE, CON_BLACK);
    KdPrintColor("-------------------------------------\n", CON_GREY, CON_BLACK);
    KdPrint("help              Displays this help information.\n");
    KdPrint("cls               Clears the screen.\n");
    KdPrint("echo [text]       Displays text. 'echo.' prints a blank line.\n");
    KdPrint("ver               Displays the WinKernel version.\n");
    KdPrint("mem               Displays memory usage statistics.\n");
    KdPrint("sysinfo           Displays CPU and system information.\n");
    KdPrint("time              Displays system uptime.\n");
    KdPrint("set [var] [val]   Sets an environment variable.\n");
    KdPrint("dir               Displays volume information.\n");
    KdPrint("crash             Triggers a manual kernel crash (BSOD).\n");
    KdPrint("reboot            Restarts the system.\n");
    KdPrint("shutdown          Powers off the system.\n");
    KdPrint("color [attr]      Changes console colors (e.g. color 0A).\n");
    KdPrint("net               Displays network adapter status and MAC address.\n");
    KdPrint("netping           Sends a raw ARP probe to test the Tx path.\n");
    KdPrint("gfxtest           Displays color bars to test the framebuffer.\n");
    KdPrint("drivers           Lists boot-loaded kernel drivers.\n");
    KdPrint("fetch             Prints system summary (fastfetch-style).\n");
    KdPrint("\n");
}

static VOID _CmdCls(VOID) {
    KdClearScreen();
}

static VOID _CmdEcho(PCSTR Args) {
    if (!Args || *Args == '\0' || (*Args == '.' && *(Args+1) == '\0')) {
        KdPutChar('\n');
    } else {
        KdPrint(Args);
        KdPutChar('\n');
    }
}

static VOID _CmdVer(VOID) {
    KdPrintColor("WinKernel Version 0.1.0 [x86_64]\n", CON_WHITE, CON_BLACK);
}

static VOID _CmdMem(VOID) {
    QWORD total_bytes = MmGetTotalBytes();
    QWORD avail_bytes = MmGetAvailableBytes();
    QWORD pool_used   = ExGetPoolUsed();
    QWORD pool_free   = ExGetPoolFree();

    QWORD total_mb = total_bytes / (1024 * 1024);
    QWORD avail_mb = avail_bytes / (1024 * 1024);
    QWORD used_kb  = pool_used   / 1024;
    QWORD free_kb  = pool_free   / 1024;

    KdPrintColor("Memory Status\n", CON_WHITE, CON_BLACK);
    KdPrintColor("-----------------------------------------\n", CON_GREY, CON_BLACK);
    KdPrintf("Total Physical Memory:     %llu MB\n", total_mb);
    KdPrintf("Available Physical Memory: %llu MB\n", avail_mb);
    KdPrintf("Kernel Pool Used:          %llu KB\n", used_kb);
    KdPrintf("Kernel Pool Free:          %llu KB\n", free_kb);
    KdPrint("\n");
}

static VOID _CmdSysinfo(VOID) {
    KdPrintColor("System Information\n", CON_WHITE, CON_BLACK);
    KdPrintColor("-----------------------------------------\n", CON_GREY, CON_BLACK);
    KdPrintf("CPU Vendor:    %s\n", HalCpuInfo.VendorString);
    KdPrintf("CPU Brand:     %s\n", HalCpuInfo.BrandString);
    KdPrintf("Family:        %u  Model: %u  Stepping: %u\n",
             HalCpuInfo.Family, HalCpuInfo.Model, HalCpuInfo.Stepping);
    KdPrintf("Logical CPUs:  %u\n", HalCpuInfo.LogicalCpuCount);
    KdPrintf("Features:      SSE=%s SSE2=%s SSE3=%s AVX=%s AVX2=%s\n",
             HalCpuInfo.HasSSE  ? "Yes" : "No",
             HalCpuInfo.HasSSE2 ? "Yes" : "No",
             HalCpuInfo.HasSSE3 ? "Yes" : "No",
             HalCpuInfo.HasAVX  ? "Yes" : "No",
             HalCpuInfo.HasAVX2 ? "Yes" : "No");
    KdPrintf("Architecture:  x86_64 (64-bit)\n");
    KdPrintf("Kernel:        NTKRNL-X v0.1.0\n");
    KdPrint("\n");
}

static VOID _CmdTime(VOID) {
    QWORD uptime = HalGetUptimeSeconds();
    QWORD hours  = uptime / 3600;
    QWORD mins   = (uptime % 3600) / 60;
    QWORD secs   = uptime % 60;
    KdPrintf("System uptime: %llu:%02llu:%02llu (%llu seconds)\n",
             hours, mins, secs, uptime);
}

static VOID _CmdSet(PCSTR Args) {
    if (!Args || *Args == '\0') {
        /* Print all variables */
        BOOL any = FALSE;
        for (INT i = 0; i < SHELL_ENV_MAX; i++) {
            if (g_Env[i].Used) {
                KdPrintf("%s=%s\n", g_Env[i].Name, g_Env[i].Value);
                any = TRUE;
            }
        }
        if (!any) KdPrint("No environment variables set.\n");
        return;
    }

    /* Parse "var val" */
    CHAR name[SHELL_ENV_NAME_MAX];
    CHAR val[SHELL_ENV_VAL_MAX];
    INT  ni = 0, vi = 0;

    while (*Args && *Args != ' ' && ni < SHELL_ENV_NAME_MAX - 1)
        name[ni++] = *Args++;
    name[ni] = '\0';

    while (*Args == ' ') Args++;

    while (*Args && vi < SHELL_ENV_VAL_MAX - 1)
        val[vi++] = *Args++;
    val[vi] = '\0';

    /* Find existing or free slot */
    INT slot = -1;
    for (INT i = 0; i < SHELL_ENV_MAX; i++) {
        if (g_Env[i].Used && RtlCompareString(g_Env[i].Name, name) == 0) {
            slot = i; break;
        }
    }
    if (slot == -1) {
        for (INT i = 0; i < SHELL_ENV_MAX; i++) {
            if (!g_Env[i].Used) { slot = i; break; }
        }
    }
    if (slot == -1) {
        KdPrint("Environment table full.\n");
        return;
    }

    RtlCopyString(g_Env[slot].Name,  name, SHELL_ENV_NAME_MAX);
    RtlCopyString(g_Env[slot].Value, val,  SHELL_ENV_VAL_MAX);
    g_Env[slot].Used = TRUE;
}

static VOID _CmdDir(VOID) {
    KdPrint(" Volume in drive C has no label.\n");
    KdPrint(" Volume Serial Number is DEAD-BEEF\n\n");
    KdPrint(" Directory of C:\\\n\n");
    KdPrint("No filesystem mounted.\n\n");
    KdPrint("               0 File(s)              0 bytes\n");
    KdPrint("               0 Dir(s)               0 bytes free\n\n");
}

static VOID _CmdCrash(VOID) {
    KeBugCheckEx(STOP_MANUALLY_INITIATED_CRASH, 0, 0, 0, 0);
}

static VOID _CmdReboot(VOID) {
    KdPrint("Rebooting...\n");
    /* Pulse 8042 reset line */
    BYTE val;
    do {
        val = HalReadPortByte(0x64);
    } while (val & 0x02);
    HalWritePortByte(0x64, 0xFE);

    /* If that didn't work, triple fault via invalid IDTR load */
    __asm__ volatile (
        "lidt (%0)\n"
        "int $0\n"
        :: "r"((QWORD[]){0, 0})
    );
    __builtin_unreachable();
}

static VOID _CmdShutdown(VOID) {
    KdPrint("Shutting down...\n");
    /* ACPI S5 via QEMU power port */
    HalWritePortWord(0x604, 0x2000);
    /* Fallback: halt */
    __asm__ volatile ("cli; hlt");
    __builtin_unreachable();
}

static VOID _CmdColor(PCSTR Args) {
    if (!Args || RtlStringLength(Args) < 2) {
        KdPrint("Usage: color [attr]\n");
        KdPrint("  attr is a 2-digit hex value: first digit = background, second = foreground\n");
        KdPrint("  Example: color 0A  (black background, green text)\n");
        return;
    }

    BYTE bg_nibble = 0, fg_nibble = 0;

    CHAR c0 = Args[0];
    CHAR c1 = Args[1];

    if (c0 >= '0' && c0 <= '9') bg_nibble = (BYTE)(c0 - '0');
    else if (c0 >= 'a' && c0 <= 'f') bg_nibble = (BYTE)(c0 - 'a' + 10);
    else if (c0 >= 'A' && c0 <= 'F') bg_nibble = (BYTE)(c0 - 'A' + 10);

    if (c1 >= '0' && c1 <= '9') fg_nibble = (BYTE)(c1 - '0');
    else if (c1 >= 'a' && c1 <= 'f') fg_nibble = (BYTE)(c1 - 'a' + 10);
    else if (c1 >= 'A' && c1 <= 'F') fg_nibble = (BYTE)(c1 - 'A' + 10);

    if (bg_nibble == fg_nibble) {
        KdPrint("Invalid color: foreground and background cannot be the same.\n");
        return;
    }

    KdSetColor(fg_nibble, bg_nibble);
    KdClearScreen();
}

/* ── _CmdNet -----------------------------------------───────────────────── */
static VOID _CmdNet(VOID) {
    NetPrintStatus();
}

/* ── _CmdNetping -----------------------------------------───────────────── */
static VOID _CmdNetping(VOID) {
    if (!NetIsAvailable()) {
        KdPrintColor("No network adapter available.\n", CON_RED, CON_BLACK);
        return;
    }

    /* Build a minimal ARP request (28 bytes) inside an Ethernet frame */
    BYTE frame[60];
    RtlZeroMemory(frame, sizeof(frame));

    MAC_ADDR src;
    NetGetMac(&src);

    /* Ethernet header: broadcast dst, our src, EtherType ARP */
    for (BYTE i = 0; i < ETH_ALEN; i++) frame[i] = 0xFF;           /* dst: broadcast */
    for (BYTE i = 0; i < ETH_ALEN; i++) frame[6 + i] = src.b[i];  /* src: our MAC   */
    frame[12] = 0x08; frame[13] = 0x06;                             /* EtherType: ARP */

    /* ARP payload (28 bytes): IPv4 over Ethernet request */
    BYTE* arp = frame + 14;
    arp[0] = 0x00; arp[1] = 0x01;  /* HTYPE: Ethernet */
    arp[2] = 0x08; arp[3] = 0x00;  /* PTYPE: IPv4     */
    arp[4] = 6;                     /* HLEN            */
    arp[5] = 4;                     /* PLEN            */
    arp[6] = 0x00; arp[7] = 0x01;  /* OPER: request   */
    for (BYTE i = 0; i < ETH_ALEN; i++) arp[8 + i] = src.b[i];    /* SHA */
    arp[14] = 0; arp[15] = 0; arp[16] = 0; arp[17] = 0;          /* SPA: 0.0.0.0 */
    for (BYTE i = 0; i < ETH_ALEN; i++) arp[18 + i] = 0;         /* THA */
    arp[24] = 192; arp[25] = 168; arp[26] = 1; arp[27] = 1;      /* TPA */

    NTSTATUS st = NetSendFrame(frame, 60);
    if (NT_SUCCESS(st)) {
        KdPrintColor("ARP probe sent successfully.\n", CON_GREEN, CON_BLACK);
    } else {
        KdPrintColor("Failed to send ARP probe.\n", CON_RED, CON_BLACK);
    }
}

/* ── _CmdGfxtest -----------------------------------------───────────────── */
static VOID _CmdGfxtest(VOID) {
    GFX_MODE mode;
    GfxGetMode(&mode);
    if (!mode.Framebuffer) {
        KdPrintColor("No framebuffer available.\n", CON_RED, CON_BLACK);
        return;
    }

    /* Draw 8 vertical color bars */
    static const DWORD colors[8] = {
        GFX_WHITE, GFX_YELLOW, GFX_CYAN, GFX_GREEN,
        GFX_MAGENTA, GFX_RED, GFX_BLUE, GFX_BLACK
    };

    DWORD bar_w = mode.Width / 8;
    for (DWORD i = 0; i < 8; i++) {
        GfxFillRect(i * bar_w, 0, bar_w, mode.Height, colors[i]);
    }

    /* Wait for a keypress then restore terminal */
    KdPrint("\nPress any key to return...");
    IoKeyboardGetChar();

    /* Restore black screen and reprint prompt area */
    GfxFillScreen(GFX_BLACK);
    KdClearScreen();
}

/* ── _CmdDrivers -----------------------------------------───────────────── */
static VOID _CmdDrivers(VOID) {
    DWORD n = IoGetLoadedDriverCount();
    KdPrintColor("Loaded drivers\n", CON_WHITE, CON_BLACK);
    KdPrintColor("-----------------------------------------\n", CON_GREY, CON_BLACK);
    if (n == 0) {
        KdPrint("  (none registered)\n\n");
        return;
    }
    CHAR name[IO_DRIVER_NAME_MAX];
    for (DWORD i = 0; i < n; i++) {
        if (IoGetLoadedDriverName(i, name, sizeof(name)))
            KdPrintf("  [%u] %s\n", i + 1, name);
    }
    KdPrint("\n");
}

/* ── _CmdFetch -----------------------------------------─────────────────── */
static VOID _CmdFetch(VOID) {
    QWORD total_mb = MmGetTotalBytes() / (1024 * 1024);
    QWORD avail_mb = MmGetAvailableBytes() / (1024 * 1024);
    QWORD uptime   = HalGetUptimeSeconds();
    QWORD h = uptime / 3600;
    QWORD m = (uptime % 3600) / 60;
    QWORD s = uptime % 60;

    CHAR net_line[64];
    if (NetIsAvailable()) {
        RtlCopyString(net_line, "RTL8139 (up)", sizeof(net_line));
    } else {
        RtlCopyString(net_line, "Offline (no supported NIC)", sizeof(net_line));
    }

    KdPrintColor("      _       _  __           _\n", CON_CYAN, CON_BLACK);
    KdPrintColor(" __ _| | ___ (_)/ /___ _ __  | |_\n", CON_CYAN, CON_BLACK);
    KdPrintColor("/ _` | |/ _ \\| | '_/ _ \\ '__| | __|\n", CON_CYAN, CON_BLACK);
    KdPrintColor("| (_| | | (_) | | . \\  __/ |    | |_\n", CON_CYAN, CON_BLACK);
    KdPrintColor(" \\__,_|_|\\___/|_|_|\\_\\___|_|     \\__|\n\n", CON_CYAN, CON_BLACK);

    KdPrintfColor(CON_WHITE, CON_BLACK, " OS      : WinKernel NTKRNL-X v0.1.0\n");
    KdPrintfColor(CON_WHITE, CON_BLACK, " Host    : x86_64 UEFI\n");
    KdPrintfColor(CON_WHITE, CON_BLACK, " CPU     : %s\n", HalCpuInfo.BrandString);
    KdPrintfColor(CON_WHITE, CON_BLACK, " Cores   : %u logical\n", HalCpuInfo.LogicalCpuCount);
    KdPrintfColor(CON_WHITE, CON_BLACK, " Memory  : %llu MB / %llu MB free\n", avail_mb, total_mb);
    KdPrintfColor(CON_WHITE, CON_BLACK, " Uptime  : %llu:%02llu:%02llu\n", h, m, s);
    KdPrintfColor(CON_WHITE, CON_BLACK, " Network : %s\n", net_line);
    KdPrintfColor(CON_WHITE, CON_BLACK, " Drivers : %u loaded\n\n", IoGetLoadedDriverCount());
}

/* ── _ShellExecute -----------------------------------------─────────────── */
static VOID _ShellExecute(PCSTR Line) {
    /* Skip leading spaces */
    while (*Line == ' ') Line++;
    if (*Line == '\0') return;

    /* Extract command token */
    CHAR cmd[64];
    INT  ci = 0;
    while (*Line && *Line != ' ' && ci < 63) cmd[ci++] = *Line++;
    cmd[ci] = '\0';

    /* Skip space between command and args */
    while (*Line == ' ') Line++;
    PCSTR args = Line;

    /* Dispatch */
    if (RtlCompareString(cmd, "help") == 0) {
        _CmdHelp();
    } else if (RtlCompareString(cmd, "cls") == 0) {
        _CmdCls();
    } else if (RtlCompareString(cmd, "echo") == 0) {
        _CmdEcho(args);
    } else if (RtlCompareString(cmd, "ver") == 0) {
        _CmdVer();
    } else if (RtlCompareString(cmd, "mem") == 0) {
        _CmdMem();
    } else if (RtlCompareString(cmd, "sysinfo") == 0) {
        _CmdSysinfo();
    } else if (RtlCompareString(cmd, "time") == 0) {
        _CmdTime();
    } else if (RtlCompareString(cmd, "set") == 0) {
        _CmdSet(args);
    } else if (RtlCompareString(cmd, "dir") == 0) {
        _CmdDir();
    } else if (RtlCompareString(cmd, "crash") == 0) {
        _CmdCrash();
    } else if (RtlCompareString(cmd, "reboot") == 0) {
        _CmdReboot();
    } else if (RtlCompareString(cmd, "shutdown") == 0) {
        _CmdShutdown();
    } else if (RtlCompareString(cmd, "color") == 0) {
        _CmdColor(args);
    } else if (RtlCompareString(cmd, "net") == 0) {
        _CmdNet();
    } else if (RtlCompareString(cmd, "netping") == 0) {
        _CmdNetping();
    } else if (RtlCompareString(cmd, "gfxtest") == 0) {
        _CmdGfxtest();
    } else if (RtlCompareString(cmd, "drivers") == 0) {
        _CmdDrivers();
    } else if (RtlCompareString(cmd, "fetch") == 0) {
        _CmdFetch();
    } else {
        KdPrintf("'%s' is not recognized as an internal command.\n", cmd);
        KdPrint("Type 'help' for a list of commands.\n");
    }
}
