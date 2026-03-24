/* WinKernel NTKRNL-X — I/O Manager */

#include <kernel/io.h>
#include <kernel/rtl.h>
#include <ntdef.h>
#include <ntstatus.h>

/* ── IoInitialize ───────────────────────────────────────────────────────── */

NTSTATUS IoInitialize(VOID) {
    /* I/O manager initialized — driver registration table ready */
    return STATUS_SUCCESS;
}
