/* WinKernel NTKRNL-X — Object Manager */

#include <kernel/ob.h>
#include <kernel/mm.h>
#include <kernel/rtl.h>
#include <ntdef.h>
#include <ntstatus.h>

/* ── Object type registry ───────────────────────────────────────────────── */
static OBJECT_TYPE g_ObjectTypes[OB_TYPE_MAX];

/* ── Handle table ───────────────────────────────────────────────────────── */
static PVOID g_HandleTable[OB_HANDLE_TABLE_SIZE];

/* ── ObInitializeObjectManager ──────────────────────────────────────────── */

NTSTATUS ObInitializeObjectManager(VOID) {
    RtlZeroMemory(g_ObjectTypes, sizeof(g_ObjectTypes));
    RtlZeroMemory(g_HandleTable, sizeof(g_HandleTable));

    /* Register built-in object types */
    ObRegisterType(OB_TYPE_PROCESS, "Process", NULL);
    ObRegisterType(OB_TYPE_THREAD,  "Thread",  NULL);
    ObRegisterType(OB_TYPE_FILE,    "File",    NULL);

    return STATUS_SUCCESS;
}

/* ── ObRegisterType ─────────────────────────────────────────────────────── */

NTSTATUS ObRegisterType(DWORD TypeId, PCSTR Name, OB_DELETE_PROC DeleteProc) {
    if (TypeId == 0 || TypeId >= OB_TYPE_MAX) return STATUS_INVALID_PARAMETER;

    OBJECT_TYPE* t = &g_ObjectTypes[TypeId];
    RtlCopyString(t->Name, Name, sizeof(t->Name));
    t->TypeId          = TypeId;
    t->DeleteProcedure = DeleteProc;

    return STATUS_SUCCESS;
}

/* ── ObCreateObject ─────────────────────────────────────────────────────── */

HANDLE ObCreateObject(DWORD Type, PCSTR Name, SIZE_T BodySize) {
    if (Type == 0 || Type >= OB_TYPE_MAX) return INVALID_HANDLE;

    /* Find a free handle slot */
    HANDLE h = INVALID_HANDLE;
    for (DWORD i = 1; i < OB_HANDLE_TABLE_SIZE; i++) {
        if (!g_HandleTable[i]) { h = (HANDLE)i; break; }
    }
    if (h == INVALID_HANDLE) return INVALID_HANDLE;

    /* Allocate header + body */
    SIZE_T total = sizeof(OBJECT_HEADER) + BodySize;
    OBJECT_HEADER* hdr = (OBJECT_HEADER*)ExAllocatePoolZero(total);
    if (!hdr) return INVALID_HANDLE;

    hdr->Type     = Type;
    hdr->RefCount = 1;
    hdr->BodySize = BodySize;
    hdr->Magic    = OBJECT_HEADER_MAGIC;
    if (Name) RtlCopyString(hdr->Name, Name, sizeof(hdr->Name));

    g_HandleTable[h] = hdr;
    return h;
}

/* ── ObReferenceObject ──────────────────────────────────────────────────── */

PVOID ObReferenceObject(HANDLE Handle) {
    if (Handle == INVALID_HANDLE || Handle == 0 || Handle >= OB_HANDLE_TABLE_SIZE)
        return NULL;

    OBJECT_HEADER* hdr = (OBJECT_HEADER*)g_HandleTable[Handle];
    if (!hdr || hdr->Magic != OBJECT_HEADER_MAGIC) return NULL;

    hdr->RefCount++;
    return (PVOID)((BYTE*)hdr + sizeof(OBJECT_HEADER));
}

/* ── ObDereferenceObject ────────────────────────────────────────────────── */

VOID ObDereferenceObject(HANDLE Handle) {
    if (Handle == INVALID_HANDLE || Handle == 0 || Handle >= OB_HANDLE_TABLE_SIZE)
        return;

    OBJECT_HEADER* hdr = (OBJECT_HEADER*)g_HandleTable[Handle];
    if (!hdr || hdr->Magic != OBJECT_HEADER_MAGIC) return;

    if (hdr->RefCount > 0) hdr->RefCount--;

    if (hdr->RefCount == 0) {
        ObDestroyObject(Handle);
    }
}

/* ── ObDestroyObject ────────────────────────────────────────────────────── */

NTSTATUS ObDestroyObject(HANDLE Handle) {
    if (Handle == INVALID_HANDLE || Handle == 0 || Handle >= OB_HANDLE_TABLE_SIZE)
        return STATUS_INVALID_HANDLE;

    OBJECT_HEADER* hdr = (OBJECT_HEADER*)g_HandleTable[Handle];
    if (!hdr) return STATUS_INVALID_HANDLE;

    /* Call type-specific destructor if registered */
    DWORD type = hdr->Type;
    if (type < OB_TYPE_MAX && g_ObjectTypes[type].DeleteProcedure) {
        PVOID body = (PVOID)((BYTE*)hdr + sizeof(OBJECT_HEADER));
        g_ObjectTypes[type].DeleteProcedure(body);
    }

    hdr->Magic = 0;
    ExFreePool(hdr);
    g_HandleTable[Handle] = NULL;

    return STATUS_SUCCESS;
}

/* ── ObGetHeader ────────────────────────────────────────────────────────── */

POBJECT_HEADER ObGetHeader(HANDLE Handle) {
    if (Handle == INVALID_HANDLE || Handle == 0 || Handle >= OB_HANDLE_TABLE_SIZE)
        return NULL;
    return (POBJECT_HEADER)g_HandleTable[Handle];
}
