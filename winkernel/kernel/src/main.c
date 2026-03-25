/* WinKernel NTKRNL-X — KiSystemStartup */

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

__attribute__((used, section(".limine_requests")))
static volatile QWORD limine_base_revision[3] = {
    0xf9562b2d5c95a6c8, 0x6a7b384944536bdc, 2
};

__attribute__((used, section(".limine_requests")))
static volatile QWORD limine_requests_start_marker[2] = {
    0x9da74a4a6f161536, 0x4b5481a9cb534dcb
};

__attribute__((used, section(".limine_requests")))
static volatile QWORD limine_requests_end_marker[2] = {
    0x1d1f2b14d0cbfac0, 0x4369f2b776361016
};

/* ── _PrintStatus ───────────────────────────────────────────────────────── */
static VOID _PrintStatus(PCSTR Text, BOOL Ok) {
    KdPrintColor(Text, CON_GREY, CON_BLACK);
    SIZE_T len = RtlStringLength(Text);
    for (SIZE_T i = len; i < 50; i++) KdPutChar(' ');
    if (Ok) {
        KdPrintColor("[", CON_GREY,  CON_BLACK);
        KdPrintColor(" DONE ", CON_GREEN, CON_BLACK);
        KdPrintColor("]\n", CON_GREY, CON_BLACK);
        KePanicLog("OK   %s", Text);
    } else {
        KdPrintColor("[", CON_GREY, CON_BLACK);
        KdPrintColor(" FAIL ", CON_RED, CON_BLACK);
        KdPrintColor("]\n", CON_GREY, CON_BLACK);
        KePanicLog("FAIL %s", Text);
    }
}

/* ── _PrintBanner — plain ASCII only, no CP437 ──────────────────────────── */
static VOID _PrintBanner(VOID) {
    KdPrintColor("  +--------------------------------------------------+\n", CON_GREY, CON_BLACK);
    KdPrintColor("  |", CON_GREY, CON_BLACK);
    KdPrintColor("    WinKernel  v0.1.0  [x86_64 UEFI]              ", CON_WHITE, CON_BLACK);
    KdPrintColor("|\n", CON_GREY, CON_BLACK);
    KdPrintColor("  |", CON_GREY, CON_BLACK);
    KdPrintColor("    NTKRNL-X Executive Loading...                  ", CON_GREY, CON_BLACK);
    KdPrintColor("|\n", CON_GREY, CON_BLACK);
    KdPrintColor("  |", CON_GREY, CON_BLACK);
    KdPrintColor("    Copyright (c) WinKernel Project                ", CON_DARK_GREY, CON_BLACK);
    KdPrintColor("|\n", CON_GREY, CON_BLACK);
    KdPrintColor("  +--------------------------------------------------+\n\n", CON_GREY, CON_BLACK);
}

/* ── KiSystemStartup ────────────────────────────────────────────────────── */
void KiSystemStartup(void) {
    WinConsole_Init();
    _PrintBanner();

    _PrintStatus("Initializing HAL...",
        NT_SUCCESS(HalInitialize()));

    _PrintStatus("Loading GDT...",
        NT_SUCCESS(KeInitializeGdt()));

    _PrintStatus("Installing IDT...",
        NT_SUCCESS(KeInitializeIdt()));

    HalRemapPic(0x20, 0x28);
    _PrintStatus("Remapping PIC controllers...", TRUE);

    _PrintStatus("Starting system timer (100 Hz)...",
        NT_SUCCESS(HalInitTimer(100)));

    NTSTATUS st = MmInitializePhysicalMemory();
    _PrintStatus("Initializing physical memory manager...", NT_SUCCESS(st));
    KdPrintf("  Available: %llu MB\n", MmGetAvailableBytes() / (1024 * 1024));

    _PrintStatus("Enabling virtual memory / paging...",
        NT_SUCCESS(MmInitializePaging()));

    _PrintStatus("Allocating kernel pool (8 MB)...",
        NT_SUCCESS(ExInitializeHeap()));

    _PrintStatus("Starting Object Manager...",
        NT_SUCCESS(ObInitializeObjectManager()));

    _PrintStatus("Starting I/O Manager...",
        NT_SUCCESS(IoInitialize()));

    _PrintStatus("Connecting PS/2 keyboard...",
        NT_SUCCESS(IoConnectKeyboard()));

    _PrintStatus("Opening serial console (COM1 115200)...",
        NT_SUCCESS(IoConnectSerial()));

    _PrintStatus("Starting Process Manager...",
        NT_SUCCESS(PsInitializeProcessManager()));

    _PrintStatus("Scanning PCI bus...",
        NT_SUCCESS(PciInitialize()));

    _PrintStatus("Initializing network (RTL8139)...",
        NT_SUCCESS(NetInitialize()));

    _PrintStatus("Initializing graphics subsystem...",
        NT_SUCCESS(GfxInitialize()));

    __asm__ volatile ("sti");
    _PrintStatus("Enabling interrupts...", TRUE);

    KdPrint("\n");
    KdPrintColor("[ OK ] WinKernel Executive initialized.\n", CON_GREEN, CON_BLACK);
    KdPrintColor("Type 'help' for available commands.\n\n", CON_GREY, CON_BLACK);

    Shell_Run();

    KeBugCheckEx(STOP_CRITICAL_PROCESS_DIED, 0, 0, 0, 0);
}
