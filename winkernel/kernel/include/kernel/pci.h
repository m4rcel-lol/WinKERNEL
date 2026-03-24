#pragma once
#include <ntdef.h>
#include <ntstatus.h>

/* ── PCI config space ports ─────────────────────────────────────────────── */
#define PCI_CONFIG_ADDR     0xCF8
#define PCI_CONFIG_DATA     0xCFC

/* ── PCI device descriptor ──────────────────────────────────────────────── */
typedef struct _PCI_DEVICE {
    BYTE    Bus;
    BYTE    Slot;
    BYTE    Func;
    WORD    VendorId;
    WORD    DeviceId;
    BYTE    Class;
    BYTE    Subclass;
    BYTE    ProgIf;
    BYTE    InterruptLine;
    DWORD   BAR[6];
    BOOL    Valid;
} PCI_DEVICE, *PPCI_DEVICE;

/* ── PCI interface ──────────────────────────────────────────────────────── */
NTSTATUS    PciInitialize(VOID);
DWORD       PciReadDword(BYTE Bus, BYTE Slot, BYTE Func, BYTE Offset);
VOID        PciWriteDword(BYTE Bus, BYTE Slot, BYTE Func, BYTE Offset, DWORD Val);
WORD        PciReadWord(BYTE Bus, BYTE Slot, BYTE Func, BYTE Offset);
BOOL        PciFindDevice(WORD VendorId, WORD DeviceId, PPCI_DEVICE Out);
VOID        PciEnableBusMaster(PPCI_DEVICE Dev);
