/* WinKernel NTKRNL-X — KiSystemStartup: NT-style executive initialization */

#include <kernel/terminal.h>
#include <kernel/hal.h>
#include <kernel/ke.h>
#include <kernel/mm.h>
#include <kernel/ob.h>
#include <kernel/io.h>
#include <kernel/ps.h>
#include <kernel/bsod.h>
#include <kernel/shell.h>
#include <kernel/rtl.h>
#include <kernel/pci.h>
#include <kernel/net.h>
#include <kernel/gfx.h>
#include <kernel/serial.h>
#include <ntdef.h>
#include <ntstatus.h>
#include <limine.h>
/* ── Limine base revision (required) ───────────────────────────────────── */
__attribute__((used, section(".limine_requests")))
static volatile QWORD limine_base_revision[3] = { 0xf9562b2d5c95a6c8, 0x6a7b384944536bdc, 2 };

/* ── Limine requests anchor ─────────────────────────────────────────────── */
__attribute__((used, section(".limine_requests")))
static volatile QWORD limine_requests_start_marker[2] = {
    0x9da74a4a6f161536, 0x4b5481a9cb534dcb
};

__attribute__((used, section(".limine_requests")))
static volatile QWORD limine_requests_end_marker[2] = {
    0x1d1f2b14d0cbfac0, 0x4369f2b776361016
};

/* ── _PrintStatus — print "text ... [ DONE ]" ───────────────────────────── */
static VOID _PrintStatus(PCSTR Text, BOOL Ok) {
    KdPrintColor(Text, CON_GREY, CON_BLACK);
    SIZE_T len = RtlStringLength(Text);
    for (SIZE_T i = len; i < 50; i++) KdPutChar(' ');
    if (Ok) {
        KdPrintColor("[ ", CON_GREY,  CON_BLACK);
        KdPrintColor("DONE", CON_GREEN, CON_BLACK);
        KdPrintColor(" ]\n", CON_GREY, CON_BLACK);
        KePanicLog("OK   %s", Text);
    } else {
        KdPrintColor("[ ", CON_GREY, CON_BLACK);
        KdPrintColor("FAIL", CON_RED, CON_BLACK);
        KdPrintColor(" ]\n", CON_GREY, CON_BLACK);
        KePanicLog("FAIL %s", Text);
    }
}

/* ── _PrintBanner ───────────────────────────────────────────────────────── */
static VOID _PrintBanner(VOID) {
    KdPrintColor("  +-----------------------------------------------+\n", CON_GREY,  CON_BLACK);
    KdPrintColor("  |", CON_GREY, CON_BLACK);
    KdPrintColor("      WinKernel  v0.1.0  [x86_64 UEFI]          ", CON_WHITE, CON_BLACK);
    KdPrintColor("|\n", CON_GREY, CON_BLACK);
    KdPrintColor("  |", CON_GREY, CON_BLACK);
    KdPrintColor("      NTKRNL-X Executive Loading...              ", CON_GREY,  CON_BLACK);
    KdPrintColor("|\n", CON_GREY, CON_BLACK);
    KdPrintColor("  |", CON_GREY, CON_BLACK);
    KdPrintColor("      Copyright (c) WinKernel Project            ", CON_DARK_GREY, CON_BLACK);
    KdPrintColor("|\n", CON_GREY, CON_BLACK);
    KdPrintColor("  +-----------------------------------------------+\n\n", CON_GREY, CON_BLACK);
}

/* ── KiSystemStartup ────────────────────────────────────────────────────── */

void KiSystemStartup(void) {
    /* a. Initialize framebuffer console */
    WinConsole_Init();

    /* b. Print boot banner */
    _PrintBanner();

    /* c. HAL */
    _PrintStatus("Initializing HAL...", NT_SUCCESS(HalInitialize()));

    /* d. GDT */
    _PrintStatus("Loading GDT descriptor tables...", NT_SUCCESS(KeInitializeGdt()));

    /* e. IDT */
    _PrintStatus("Installing interrupt descriptor table...", NT_SUCCESS(KeInitializeIdt()));

    /* f. PIC remap */
    HalRemapPic(0x20, 0x28);
    _PrintStatus("Remapping PIC controllers...", TRUE);

    /* g. PIT timer at 100 Hz */
    _PrintStatus("Starting system timer (100Hz)...", NT_SUCCESS(HalInitTimer(100)));

    /* h. PMM */
    NTSTATUS st = MmInitializePhysicalMemory();
    _PrintStatus("Initializing physical memory manager...", NT_SUCCESS(st));

    QWORD avail_mb = MmGetAvailableBytes() / (1024 * 1024);
    KdPrintf("  Available memory: %llu MB\n", avail_mb);

    /* i. VMM / paging */
    _PrintStatus("Enabling virtual memory / paging...", NT_SUCCESS(MmInitializePaging()));

    /* j. Kernel heap */
    _PrintStatus("Allocating kernel pool (8MB)...", NT_SUCCESS(ExInitializeHeap()));

    /* k. Object Manager */
    _PrintStatus("Starting Object Manager...", NT_SUCCESS(ObInitializeObjectManager()));

    /* l. I/O Manager */
    _PrintStatus("Starting I/O Manager...", NT_SUCCESS(IoInitialize()));

    /* m. PS/2 keyboard */
    {
        NTSTATUS kb_st = IoConnectKeyboard();
        _PrintStatus("Connecting PS/2 keyboard...", NT_SUCCESS(kb_st));
        if (!NT_SUCCESS(kb_st))
            KePanicLog("Keyboard: no i8042 detected (USB-only machine)");
        else
            KePanicLog("Keyboard: i8042 PS/2 controller found and enabled");
    }

    /* m2. COM1 serial (polled) */
    _PrintStatus("Opening serial console (COM1 115200)...", NT_SUCCESS(IoConnectSerial()));

    /* n. Process Manager */
    _PrintStatus("Starting Process Manager...", NT_SUCCESS(PsInitializeProcessManager()));

    /* o. PCI bus */
    _PrintStatus("Scanning PCI bus...", NT_SUCCESS(PciInitialize()));

    /* p. Network (RTL8139) */
    NTSTATUS net_st = NetInitialize();
    _PrintStatus("Initializing network adapter (RTL8139)...", NT_SUCCESS(net_st));

    /* q. Graphics subsystem */
    _PrintStatus("Initializing graphics subsystem...", NT_SUCCESS(GfxInitialize()));

    /* r. Enable interrupts */
    __asm__ volatile ("sti");
    _PrintStatus("Enabling interrupts...", TRUE);

    KdPrint("\n");
    KdPrintColor("[  OK  ] WinKernel Executive initialized.\n", CON_GREEN, CON_BLACK);
    KdPrintColor("Type 'help' for available commands.\n\n", CON_GREY, CON_BLACK);

    /* p. Enter shell — never returns */
    Shell_Run();

    /* Should never reach here */
    KeBugCheckEx(STOP_CRITICAL_PROCESS_DIED, 0, 0, 0, 0);
}
