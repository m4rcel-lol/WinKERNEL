#pragma once

#include <ntdef.h>
#include <ntstatus.h>

/* ── Process/thread state ───────────────────────────────────────────────── */
typedef enum _KTHREAD_STATE {
    ThreadReady     = 0,
    ThreadRunning   = 1,
    ThreadWaiting   = 2,
    ThreadTerminated= 3
} KTHREAD_STATE;

/* ── Kernel thread structure ────────────────────────────────────────────── */
typedef struct _KTHREAD {
    QWORD           Rsp;
    QWORD           Rip;
    KTHREAD_STATE   State;
    DWORD           ThreadId;
    CHAR            Name[32];
    PVOID           KernelStack;
    SIZE_T          StackSize;
} KTHREAD, *PKTHREAD;

/* ── Kernel process structure ───────────────────────────────────────────── */
typedef struct _KPROCESS {
    DWORD           ProcessId;
    CHAR            ImageName[64];
    ULONG_PTR       DirectoryBase;     /* CR3 value */
    PKTHREAD        MainThread;
} KPROCESS, *PKPROCESS;

/* ── Ps interface ───────────────────────────────────────────────────────── */
NTSTATUS    PsInitializeProcessManager(VOID);
PKPROCESS   PsGetCurrentProcess(VOID);
PKTHREAD    PsGetCurrentThread(VOID);
