#pragma once

#include <ntdef.h>
#include <ntstatus.h>

/* ── Object type IDs ────────────────────────────────────────────────────── */
#define OB_TYPE_PROCESS     1
#define OB_TYPE_THREAD      2
#define OB_TYPE_FILE        3
#define OB_TYPE_MAX         4

/* ── Object header ──────────────────────────────────────────────────────── */
typedef struct _OBJECT_HEADER {
    DWORD   Type;
    DWORD   RefCount;
    CHAR    Name[64];
    SIZE_T  BodySize;
    DWORD   Magic;
} OBJECT_HEADER, *POBJECT_HEADER;

#define OBJECT_HEADER_MAGIC     0x4F424A54  /* 'OBJT' */

/* ── Object type descriptor ─────────────────────────────────────────────── */
typedef VOID (*OB_DELETE_PROC)(PVOID Body);

typedef struct _OBJECT_TYPE {
    CHAR            Name[32];
    DWORD           TypeId;
    OB_DELETE_PROC  DeleteProcedure;
} OBJECT_TYPE, *POBJECT_TYPE;

/* ── Handle table ───────────────────────────────────────────────────────── */
#define OB_HANDLE_TABLE_SIZE    1024

/* ── Ob interface ───────────────────────────────────────────────────────── */
NTSTATUS    ObInitializeObjectManager(VOID);
NTSTATUS    ObRegisterType(DWORD TypeId, PCSTR Name, OB_DELETE_PROC DeleteProc);
HANDLE      ObCreateObject(DWORD Type, PCSTR Name, SIZE_T BodySize);
PVOID       ObReferenceObject(HANDLE Handle);
VOID        ObDereferenceObject(HANDLE Handle);
NTSTATUS    ObDestroyObject(HANDLE Handle);
POBJECT_HEADER ObGetHeader(HANDLE Handle);
