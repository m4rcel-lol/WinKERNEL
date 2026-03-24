#pragma once

#include <ntdef.h>
#include <ntstatus.h>

/* COM1 polled serial (real-hardware fallback when PS/2 is absent). */
NTSTATUS IoConnectSerial(VOID);
BOOL     IoSerialTryRead(CHAR* Out);
