/* WinKernel NTKRNL-X — Process Manager (single system process / thread) */

#include <kernel/ps.h>
#include <kernel/mm.h>
#include <kernel/rtl.h>
#include <ntdef.h>
#include <ntstatus.h>

/* ── System process (PID 0 — the kernel itself) ─────────────────────────── */
static KPROCESS g_SystemProcess;
static KTHREAD  g_SystemThread;

/* ── PsInitializeProcessManager ────────────────────────────────────────── */

NTSTATUS PsInitializeProcessManager(VOID) {
    RtlZeroMemory(&g_SystemProcess, sizeof(g_SystemProcess));
    RtlZeroMemory(&g_SystemThread,  sizeof(g_SystemThread));

    /* System process */
    g_SystemProcess.ProcessId   = 0;
    RtlCopyString(g_SystemProcess.ImageName, "System", sizeof(g_SystemProcess.ImageName));
    g_SystemProcess.MainThread  = &g_SystemThread;

    /* Read current CR3 for the system process directory base */
    ULONG_PTR cr3;
    __asm__ volatile ("mov %%cr3, %0" : "=r"(cr3));
    g_SystemProcess.DirectoryBase = cr3 & ~0xFFFULL;

    /* System thread */
    g_SystemThread.ThreadId = 0;
    RtlCopyString(g_SystemThread.Name, "SystemThread", sizeof(g_SystemThread.Name));
    g_SystemThread.State    = ThreadRunning;

    return STATUS_SUCCESS;
}

/* ── PsGetCurrentProcess ────────────────────────────────────────────────── */

PKPROCESS PsGetCurrentProcess(VOID) {
    return &g_SystemProcess;
}

/* ── PsGetCurrentThread ─────────────────────────────────────────────────── */

PKTHREAD PsGetCurrentThread(VOID) {
    return &g_SystemThread;
}
