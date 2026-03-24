/* WinKernel NTKRNL-X — Blue Screen of Death + Kernel Panic Log */

#include <kernel/bsod.h>
#include <kernel/terminal.h>
#include <kernel/rtl.h>
#include <ntdef.h>
#include <ntstatus.h>
#include <stdarg.h>

/* ════════════════════════════════════════════════════════════════════════════
   KERNEL PANIC LOG
   A simple ring buffer written during boot/runtime. Shown verbatim on BSOD.
   ════════════════════════════════════════════════════════════════════════════ */

static CHAR  g_PanicLog[KPANIC_LOG_ENTRIES][KPANIC_LOG_MSG_MAX];
static DWORD g_PanicLogCount = 0;

VOID KePanicLog(PCSTR Fmt, ...) {
    if (g_PanicLogCount >= KPANIC_LOG_ENTRIES) {
        /* Shift entries up to make room */
        for (DWORD i = 0; i < KPANIC_LOG_ENTRIES - 1; i++)
            RtlCopyMemory(g_PanicLog[i], g_PanicLog[i + 1], KPANIC_LOG_MSG_MAX);
        g_PanicLogCount = KPANIC_LOG_ENTRIES - 1;
    }
    va_list args;
    va_start(args, Fmt);
    RtlVprintf(g_PanicLog[g_PanicLogCount], KPANIC_LOG_MSG_MAX, Fmt, args);
    va_end(args);
    g_PanicLogCount++;
}

/* ── Stop code name table ───────────────────────────────────────────────── */
static const STOP_CODE_ENTRY g_StopCodes[] = {
    { STOP_IRQL_NOT_LESS_OR_EQUAL,         "IRQL_NOT_LESS_OR_EQUAL"         },
    { STOP_PAGE_FAULT_IN_NONPAGED_AREA,    "PAGE_FAULT_IN_NONPAGED_AREA"    },
    { STOP_KERNEL_MODE_EXCEPTION,          "KERNEL_MODE_EXCEPTION"          },
    { STOP_UNEXPECTED_KERNEL_MODE_TRAP,    "UNEXPECTED_KERNEL_MODE_TRAP"    },
    { STOP_MANUALLY_INITIATED_CRASH,       "MANUALLY_INITIATED_CRASH"       },
    { STOP_HEAP_CORRUPTION,                "HEAP_CORRUPTION"                },
    { STOP_CRITICAL_PROCESS_DIED,          "CRITICAL_PROCESS_DIED"          },
    { 0xFFFFFFFF,                          "UNKNOWN_STOP_CODE"              },
};

static PCSTR _LookupStopCode(DWORD Code) {
    for (DWORD i = 0; g_StopCodes[i].Code != 0xFFFFFFFF; i++)
        if (g_StopCodes[i].Code == Code) return g_StopCodes[i].Name;
    return "UNKNOWN_STOP_CODE";
}

/* ── BSOD helpers ───────────────────────────────────────────────────────── */
static VOID _BsodFillScreen(VOID) {
    KdSetColor(CON_WHITE, CON_DARK_BLUE);
    KdClearScreen();
}

static VOID _BsodPrint(PCSTR s) {
    KdPrintColor(s, CON_WHITE, CON_DARK_BLUE);
}

__attribute__((format(printf, 1, 2)))
static VOID _BsodPrintf(PCSTR fmt, ...) {
    char buf[256];
    va_list args;
    va_start(args, fmt);
    RtlVprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    _BsodPrint(buf);
}

/* ── _BsodRenderRegisters ───────────────────────────────────────────────── */
static VOID _BsodRenderRegisters(PKTRAP_FRAME Frame) {
    if (!Frame) { _BsodPrint("  (no trap frame available)\n"); return; }

    _BsodPrintf("  RAX=%016llx  RBX=%016llx  RCX=%016llx  RDX=%016llx\n",
                Frame->RAX, Frame->RBX, Frame->RCX, Frame->RDX);
    _BsodPrintf("  RSI=%016llx  RDI=%016llx  RSP=%016llx  RBP=%016llx\n",
                Frame->RSI, Frame->RDI, Frame->RSP, Frame->RBP);
    _BsodPrintf("  R8 =%016llx  R9 =%016llx  R10=%016llx  R11=%016llx\n",
                Frame->R8,  Frame->R9,  Frame->R10, Frame->R11);
    _BsodPrintf("  R12=%016llx  R13=%016llx  R14=%016llx  R15=%016llx\n",
                Frame->R12, Frame->R13, Frame->R14, Frame->R15);
    _BsodPrintf("  RIP=%016llx  RFLAGS=%016llx\n", Frame->RIP, Frame->RFLAGS);

    ULONG_PTR cr0, cr2, cr3, cr4;
    __asm__ volatile ("mov %%cr0, %0" : "=r"(cr0));
    __asm__ volatile ("mov %%cr2, %0" : "=r"(cr2));
    __asm__ volatile ("mov %%cr3, %0" : "=r"(cr3));
    __asm__ volatile ("mov %%cr4, %0" : "=r"(cr4));
    _BsodPrintf("  CR0=%016llx  CR2=%016llx  CR3=%016llx  CR4=%016llx\n",
                cr0, cr2, cr3, cr4);
}

/* ── _BsodRenderPanicLog ────────────────────────────────────────────────── */
static VOID _BsodRenderPanicLog(VOID) {
    _BsodPrint("\nKernel event log (most recent last):\n");
    _BsodPrint("--------------------------------------\n");
    if (g_PanicLogCount == 0) {
        _BsodPrint("  (empty)\n");
        return;
    }
    /* Show last 16 entries so they fit on screen */
    DWORD start = (g_PanicLogCount > 16) ? (g_PanicLogCount - 16) : 0;
    for (DWORD i = start; i < g_PanicLogCount; i++) {
        _BsodPrintf("  [%02u] %s\n", i, g_PanicLog[i]);
    }
}

/* ── _BsodCommon ────────────────────────────────────────────────────────── */
static __attribute__((noreturn)) VOID
_BsodCommon(DWORD StopCode, ULONG_PTR P1, ULONG_PTR P2,
            ULONG_PTR P3, ULONG_PTR P4, PKTRAP_FRAME Frame) {
    __asm__ volatile ("cli");
    _BsodFillScreen();

    _BsodPrint("\n  *** STOP: WinKernel has encountered a fatal error ***\n\n");
    _BsodPrintf("  Stop code : 0x%08x  %s\n\n", StopCode, _LookupStopCode(StopCode));
    _BsodPrintf("  Parameters: 0x%016llx  0x%016llx\n", P1, P2);
    _BsodPrintf("              0x%016llx  0x%016llx\n\n", P3, P4);

    _BsodPrint("  Kernel: NTKRNL-X v0.1.0  [x86_64]\n\n");

    if (Frame) {
        _BsodPrint("Register dump:\n");
        _BsodRenderRegisters(Frame);
    }

    _BsodRenderPanicLog();

    _BsodPrint("\nSystem halted. You may power off your machine.\n");

    __asm__ volatile ("1: cli\n hlt\n jmp 1b\n");
    __builtin_unreachable();
}

/* ── KeBugCheckEx ───────────────────────────────────────────────────────── */
__attribute__((noreturn))
VOID KeBugCheckEx(DWORD StopCode, ULONG_PTR P1, ULONG_PTR P2,
                  ULONG_PTR P3, ULONG_PTR P4) {
    _BsodCommon(StopCode, P1, P2, P3, P4, NULL);
}

/* ── KeBugCheckWithFrame ────────────────────────────────────────────────── */
__attribute__((noreturn))
VOID KeBugCheckWithFrame(DWORD StopCode, PKTRAP_FRAME Frame) {
    ULONG_PTR p1 = Frame ? Frame->ErrorCode : 0;
    ULONG_PTR p2 = Frame ? Frame->RIP       : 0;
    _BsodCommon(StopCode, p1, p2, 0, 0, Frame);
}
