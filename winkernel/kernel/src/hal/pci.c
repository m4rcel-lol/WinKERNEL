/* WinKernel NTKRNL-X — PCI bus driver */

#include <kernel/pci.h>
#include <kernel/hal.h>
#include <kernel/terminal.h>
#include <ntdef.h>

/* ── PciReadDword ───────────────────────────────────────────────────────── */
DWORD PciReadDword(BYTE Bus, BYTE Slot, BYTE Func, BYTE Offset) {
    DWORD addr = (DWORD)(1u << 31)
               | ((DWORD)Bus   << 16)
               | ((DWORD)Slot  << 11)
               | ((DWORD)Func  <<  8)
               | ((DWORD)(Offset & 0xFC));
    HalWritePortDword(PCI_CONFIG_ADDR, addr);
    return HalReadPortDword(PCI_CONFIG_DATA);
}

/* ── PciWriteDword ──────────────────────────────────────────────────────── */
VOID PciWriteDword(BYTE Bus, BYTE Slot, BYTE Func, BYTE Offset, DWORD Val) {
    DWORD addr = (DWORD)(1u << 31)
               | ((DWORD)Bus   << 16)
               | ((DWORD)Slot  << 11)
               | ((DWORD)Func  <<  8)
               | ((DWORD)(Offset & 0xFC));
    HalWritePortDword(PCI_CONFIG_ADDR, addr);
    HalWritePortDword(PCI_CONFIG_DATA, Val);
}

/* ── PciReadWord ────────────────────────────────────────────────────────── */
WORD PciReadWord(BYTE Bus, BYTE Slot, BYTE Func, BYTE Offset) {
    DWORD dw = PciReadDword(Bus, Slot, Func, Offset & 0xFC);
    return (WORD)((dw >> ((Offset & 2) * 8)) & 0xFFFF);
}

/* ── PciFindDevice ──────────────────────────────────────────────────────── */
BOOL PciFindDevice(WORD VendorId, WORD DeviceId, PPCI_DEVICE Out) {
    for (DWORD bus = 0; bus < 256; bus++) {
        for (DWORD slot = 0; slot < 32; slot++) {
            DWORD id = PciReadDword((BYTE)bus, (BYTE)slot, 0, 0);
            if ((id & 0xFFFF) == 0xFFFF) continue;

            WORD vid = (WORD)(id & 0xFFFF);
            WORD did = (WORD)(id >> 16);

            if (vid == VendorId && did == DeviceId) {
                Out->Bus      = (BYTE)bus;
                Out->Slot     = (BYTE)slot;
                Out->Func     = 0;
                Out->VendorId = vid;
                Out->DeviceId = did;

                DWORD cc = PciReadDword((BYTE)bus, (BYTE)slot, 0, 0x08);
                Out->Class    = (BYTE)(cc >> 24);
                Out->Subclass = (BYTE)(cc >> 16);
                Out->ProgIf   = (BYTE)(cc >>  8);

                Out->InterruptLine = (BYTE)(PciReadDword((BYTE)bus, (BYTE)slot, 0, 0x3C) & 0xFF);

                for (BYTE b = 0; b < 6; b++)
                    Out->BAR[b] = PciReadDword((BYTE)bus, (BYTE)slot, 0, (BYTE)(0x10 + b * 4));

                Out->Valid = TRUE;
                return TRUE;
            }
        }
    }
    Out->Valid = FALSE;
    return FALSE;
}

/* ── PciEnableBusMaster ─────────────────────────────────────────────────── */
VOID PciEnableBusMaster(PPCI_DEVICE Dev) {
    DWORD cmd = PciReadDword(Dev->Bus, Dev->Slot, Dev->Func, 0x04);
    cmd |= (1 << 2) | (1 << 0);   /* Bus Master + I/O Space enable */
    PciWriteDword(Dev->Bus, Dev->Slot, Dev->Func, 0x04, cmd);
}

/* ── PciInitialize ──────────────────────────────────────────────────────── */
NTSTATUS PciInitialize(VOID) {
    return STATUS_SUCCESS;
}
