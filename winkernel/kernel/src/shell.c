/* WinKernel NTKRNL-X — cmd.exe-style shell */

#include <kernel/shell.h>
#include <kernel/terminal.h>
#include <kernel/io.h>
#include <kernel/mm.h>
#include <kernel/hal.h>
#include <kernel/rtl.h>
#include <kernel/bsod.h>
#include <kernel/ps.h>
#include <ntdef.h>
#include <ntstatus.h>

/* ── Command names for TAB completion ───────────────────────────────────── */
static const CHAR* g_Commands[] = {
    "help", "cls", "echo", "ver", "mem", "sysinfo",
    "time", "set", "dir", "crash", "reboot", "shutdown", "color",
    NULL
};

/* ── History ────────────────────────────────────────────────────────────── */
static CHAR g_History[SHELL_HISTORY_MAX][SHELL_INPUT_MAX];
static INT  g_HistoryCount = 0;
static INT  g_HistoryPos   = -1;

/* ── Environment variables ──────────────────────────────────────────────── */
static SHELL_ENV_VAR g_Env[SHELL_ENV_MAX];

/* ── Input buffer ───────────────────────────────────────────────────────── */
static CHAR g_InputBuf[SHELL_INPUT_MAX];
static INT  g_InputLen  = 0;
static INT  g_CursorPos = 0;

/* ── Forward declarations ───────────────────────────────────────────────── */
static VOID _ShellExecute(PCSTR Line);
static VOID _ShellRedrawLine(VOID);

/* ── Shell_PrintBanner ──────────────────────────────────────────────────── */
VOID Shell_PrintBanner(VOID) {
    KdPrintColor("WinKernel Version 0.1.0 [x86_64]\n", CON_WHITE, CON_BLACK);
    KdPrintColor("(c) WinKernel Project. All rights reserved.\n\n", CON_GREY, CON_BLACK);
}

/* ── _PrintPrompt ───────────────────────────────────────────────────────── */
static VOID _PrintPrompt(VOID) {
    KdPrintColor("C:\\> ", CON_WHITE, CON_BLACK);
}

/* ── _HistoryAdd ────────────────────────────────────────────────────────── */
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

/* ── _TabComplete ───────────────────────────────────────────────────────── */
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

/* ── Shell_Run ──────────────────────────────────────────────────────────── */
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
            /* hlt until an IRQ wakes us — much better than spinning */
            while (!IoKeyboardReadEvent(&ev)) {
                __asm__ volatile ("hlt");
            }
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
    KdPrintColor("─────────────────────────────────────\n", CON_GREY, CON_BLACK);
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
    KdPrintColor("─────────────────────────────────────────\n", CON_GREY, CON_BLACK);
    KdPrintf("Total Physical Memory:     %llu MB\n", total_mb);
    KdPrintf("Available Physical Memory: %llu MB\n", avail_mb);
    KdPrintf("Kernel Pool Used:          %llu KB\n", used_kb);
    KdPrintf("Kernel Pool Free:          %llu KB\n", free_kb);
    KdPrint("\n");
}

static VOID _CmdSysinfo(VOID) {
    KdPrintColor("System Information\n", CON_WHITE, CON_BLACK);
    KdPrintColor("─────────────────────────────────────────\n", CON_GREY, CON_BLACK);
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

/* ── _ShellExecute ──────────────────────────────────────────────────────── */
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
    } else {
        KdPrintf("'%s' is not recognized as an internal command.\n", cmd);
        KdPrint("Type 'help' for a list of commands.\n");
    }
}
