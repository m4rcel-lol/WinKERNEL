/* WinKernel NTKRNL-X — Hardware Abstraction Layer */

#include <kernel/hal.h>
#include <kernel/rtl.h>
#include <ntdef.h>

HAL_CPU_INFO HalCpuInfo;

/* ── Port I/O ───────────────────────────────────────────────────────────── */

BYTE HalReadPortByte(WORD Port) {
    BYTE val;
    __asm__ volatile ("inb %1, %0" : "=a"(val) : "Nd"(Port));
    return val;
}

VOID HalWritePortByte(WORD Port, BYTE Value) {
    __asm__ volatile ("outb %0, %1" :: "a"(Value), "Nd"(Port));
}

WORD HalReadPortWord(WORD Port) {
    WORD val;
    __asm__ volatile ("inw %1, %0" : "=a"(val) : "Nd"(Port));
    return val;
}

VOID HalWritePortWord(WORD Port, WORD Value) {
    __asm__ volatile ("outw %0, %1" :: "a"(Value), "Nd"(Port));
}

DWORD HalReadPortDword(WORD Port) {
    DWORD val;
    __asm__ volatile ("inl %1, %0" : "=a"(val) : "Nd"(Port));
    return val;
}

VOID HalWritePortDword(WORD Port, DWORD Value) {
    __asm__ volatile ("outl %0, %1" :: "a"(Value), "Nd"(Port));
}

VOID HalIoDelay(VOID) {
    /* Write to port 0x80 (POST diagnostic port) — safe I/O delay */
    HalWritePortByte(0x80, 0x00);
}

/* ── CPUID helper ───────────────────────────────────────────────────────── */

static VOID _Cpuid(DWORD Leaf, DWORD* Eax, DWORD* Ebx, DWORD* Ecx, DWORD* Edx) {
    __asm__ volatile (
        "cpuid"
        : "=a"(*Eax), "=b"(*Ebx), "=c"(*Ecx), "=d"(*Edx)
        : "a"(Leaf), "c"(0)
    );
}

/* ── HalGetCpuInfo ──────────────────────────────────────────────────────── */

VOID HalGetCpuInfo(PHAL_CPU_INFO Info) {
    DWORD eax, ebx, ecx, edx;

    /* Vendor string */
    _Cpuid(0, &eax, &ebx, &ecx, &edx);
    DWORD* vp = (DWORD*)Info->VendorString;
    vp[0] = ebx; vp[1] = edx; vp[2] = ecx;
    Info->VendorString[12] = '\0';

    /* Family / model / stepping */
    _Cpuid(1, &eax, &ebx, &ecx, &edx);
    Info->Stepping = eax & 0xF;
    Info->Model    = (eax >> 4) & 0xF;
    Info->Family   = (eax >> 8) & 0xF;

    /* Feature flags */
    Info->HasSSE  = (edx >> 25) & 1;
    Info->HasSSE2 = (edx >> 26) & 1;
    Info->HasSSE3 = (ecx >>  0) & 1;
    Info->HasAVX  = (ecx >> 28) & 1;

    /* AVX2 — leaf 7 */
    _Cpuid(7, &eax, &ebx, &ecx, &edx);
    Info->HasAVX2 = (ebx >> 5) & 1;

    /* Logical CPU count */
    _Cpuid(1, &eax, &ebx, &ecx, &edx);
    Info->LogicalCpuCount = (ebx >> 16) & 0xFF;
    if (Info->LogicalCpuCount == 0) Info->LogicalCpuCount = 1;

    /* Brand string (leaves 0x80000002–0x80000004) */
    DWORD* bp = (DWORD*)Info->BrandString;
    _Cpuid(0x80000002, &bp[0], &bp[1], &bp[2],  &bp[3]);
    _Cpuid(0x80000003, &bp[4], &bp[5], &bp[6],  &bp[7]);
    _Cpuid(0x80000004, &bp[8], &bp[9], &bp[10], &bp[11]);
    Info->BrandString[48] = '\0';
}

/* ── HalInitialize ──────────────────────────────────────────────────────── */

NTSTATUS HalInitialize(VOID) {
    HalGetCpuInfo(&HalCpuInfo);
    return STATUS_SUCCESS;
}
