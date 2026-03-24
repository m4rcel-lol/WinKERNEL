/* WinKernel NTKRNL-X — Blue Screen of Death */

#include <kernel/bsod.h>
#include <kernel/terminal.h>
#include <kernel/rtl.h>
#include <ntdef.h>
#include <ntstatus.h>
#include <stdarg.h>

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
    for (DWORD i = 0; g_StopCodes[i].Code != 0xFFFFFFFF; i++) {
        if (g_StopCodes[i].Code == Code) return g_StopCodes[i].Name;
    }
    return "UNKNOWN_STOP_CODE";
}

/* ── _BsodFillScreen — paint entire framebuffer blue ───────────────────── */
static VOID _BsodFillScreen(VOID) {
    /* Force background to blue, foreground to white */
    KdSetColor(CON_WHITE, CON_DARK_BLUE);
    KdClearScreen();
}

/* ── _BsodPrint helpers ─────────────────────────────────────────────────── */
static VOID _BsodPrint(PCSTR s) {
    KdPrintColor(s, CON_WHITE, CON_DARK_BLUE);
}

static VOID _BsodPrintf(PCSTR fmt, ...) {
    CHAR buf[256];
    va_list args;
    va_start(args, fmt);
    RtlVprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    _BsodPrint(buf);
}

/* ── _BsodRenderRegisters ───────────────────────────────────────────────── */
static VOID _BsodRenderRegisters(PKTRAP_FRAME Frame) {
    if (!Frame) {
        _BsodPrint("Register dump: (no frame available)\n");
        return;
    }

    _BsodPrint("Register dump:\n");
    _BsodPrintf("RAX=%016llx  RBX=%016llx  RCX=%016llx  RDX=%016llx\n",
                Frame->RAX, Frame->RBX, Frame->RCX, Frame->RDX);
    _BsodPrintf("RSI=%016llx  RDI=%016llx  RSP=%016llx  RBP=%016llx\n",
                Frame->RSI, Frame->RDI, Frame->RSP, Frame->RBP);
    _BsodPrintf("R8 =%016llx  R9 =%016llx  R10=%016llx  R11=%016llx\n",
                Frame->R8,  Frame->R9,  Frame->R10, Frame->R11);
    _BsodPrintf("R12=%016llx  R13=%016llx  R14=%016llx  R15=%016llx\n",
                Frame->R12, Frame->R13, Frame->R14, Frame->R15);
    _BsodPrintf("RIP=%016llx  RFLAGS=%016llx\n",
                Frame->RIP, Frame->RFLAGS);

    /* Read control registers */
    ULONG_PTR cr0, cr2, cr3, cr4;
    __asm__ volatile ("mov %0, cr0" : "=r"(cr0));
    __asm__ volatile ("mov %0, cr2" : "=r"(cr2));
    __asm__ volatile ("mov %0, cr3" : "=r"(cr3));
    __asm__ volatile ("mov %0, cr4" : "=r"(cr4));

    _BsodPrintf("CR0=%016llx  CR2=%016llx  CR3=%016llx  CR4=%016llx\n",
                cr0, cr2, cr3, cr4);
}

/* ── KeBugCheckEx ───────────────────────────────────────────────────────── */

__attribute__((noreturn))
VOID KeBugCheckEx(DWORD StopCode, ULONG_PTR Param1, ULONG_PTR Param2,
                  ULONG_PTR Param3, ULONG_PTR Param4) {
    __asm__ volatile ("cli");

    _BsodFillScreen();

    _BsodPrint("\n");
    _BsodPrint("A problem has been detected and WinKernel has been shut down\n");
    _BsodPrint("to prevent damage to your computer.\n\n");

    _BsodPrintf("STOP_CODE: %s\n\n", _LookupStopCode(StopCode));

    _BsodPrint("Technical information:\n\n");
    _BsodPrintf("*** STOP: 0x%08x (0x%016llx, 0x%016llx, 0x%016llx, 0x%016llx)\n\n",
                StopCode, Param1, Param2, Param3, Param4);

    _BsodPrint("KRNL: NTKRNL-X  v0.1.0\n\n");

    _BsodPrint("Register dump: (no trap frame)\n\n");

    _BsodPrint("Beginning physical memory dump...\n");
    _BsodPrint("Physical memory dump complete.\n\n");

    _BsodPrint("Press any key to... (just kidding, we're halted)\n");

    __asm__ volatile (
        "1: cli\n"
        "   hlt\n"
        "   jmp 1b\n"
    );
    __builtin_unreachable();
}

/* ── KeBugCheckWithFrame ────────────────────────────────────────────────── */

__attribute__((noreturn))
VOID KeBugCheckWithFrame(DWORD StopCode, PKTRAP_FRAME Frame) {
    __asm__ volatile ("cli");

    _BsodFillScreen();

    _BsodPrint("\n");
    _BsodPrint("A problem has been detected and WinKernel has been shut down\n");
    _BsodPrint("to prevent damage to your computer.\n\n");

    _BsodPrintf("STOP_CODE: %s\n\n", _LookupStopCode(StopCode));

    _BsodPrint("Technical information:\n\n");

    ULONG_PTR p1 = Frame ? Frame->ErrorCode : 0;
    ULONG_PTR p2 = Frame ? Frame->RIP       : 0;
    _BsodPrintf("*** STOP: 0x%08x (0x%016llx, 0x%016llx, 0x0000000000000000, 0x0000000000000000)\n\n",
                StopCode, p1, p2);

    _BsodPrint("KRNL: NTKRNL-X  v0.1.0\n\n");

    _BsodRenderRegisters(Frame);

    _BsodPrint("\nBeginning physical memory dump...\n");
    _BsodPrint("Physical memory dump complete.\n\n");

    __asm__ volatile (
        "1: cli\n"
        "   hlt\n"
        "   jmp 1b\n"
    );
    __builtin_unreachable();
}
