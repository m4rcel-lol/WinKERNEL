#pragma once
#include <ntdef.h>
#include <ntstatus.h>

/* ── Ethernet frame limits ──────────────────────────────────────────────── */
#define ETH_ALEN        6
#define ETH_MTU         1500
#define ETH_FRAME_MAX   1518

/* ── MAC address ────────────────────────────────────────────────────────── */
typedef struct _MAC_ADDR { BYTE b[ETH_ALEN]; } MAC_ADDR;

/* ── Ethernet frame header ──────────────────────────────────────────────── */
typedef struct __attribute__((packed)) _ETH_HEADER {
    MAC_ADDR    Dst;
    MAC_ADDR    Src;
    WORD        EtherType;
} ETH_HEADER;

#define ETHERTYPE_ARP   0x0806
#define ETHERTYPE_IPV4  0x0800

/* ── RTL8139 PCI IDs ────────────────────────────────────────────────────── */
#define RTL8139_VENDOR  0x10EC
#define RTL8139_DEVICE  0x8139

/* ── Net interface ──────────────────────────────────────────────────────── */
NTSTATUS    NetInitialize(VOID);
BOOL        NetIsAvailable(VOID);
NTSTATUS    NetSendFrame(const BYTE* Data, WORD Len);
BOOL        NetReceiveFrame(BYTE* Buffer, WORD* Len);
VOID        NetGetMac(MAC_ADDR* Out);
VOID        NetPrintStatus(VOID);
