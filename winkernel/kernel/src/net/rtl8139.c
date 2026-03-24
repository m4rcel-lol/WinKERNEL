/* WinKernel NTKRNL-X — RTL8139 NIC driver */

#include <kernel/net.h>
#include <kernel/pci.h>
#include <kernel/hal.h>
#include <kernel/ke.h>
#include <kernel/terminal.h>
#include <kernel/rtl.h>
#include <kernel/mm.h>
#include <kernel/io.h>
#include <ntdef.h>
#include <ntstatus.h>

/* ── RTL8139 register offsets (I/O base relative) ──────────────────────── */
#define RTL_MAC0        0x00    /* MAC address bytes 0-5 */
#define RTL_MAR0        0x08    /* Multicast filter */
#define RTL_TSD0        0x10    /* Tx status descriptor 0-3 (4 x DWORD) */
#define RTL_TSAD0       0x20    /* Tx start address 0-3 (4 x DWORD) */
#define RTL_RBSTART     0x30    /* Rx buffer start address */
#define RTL_CMD         0x37    /* Command register */
#define RTL_CAPR        0x38    /* Current address of packet read */
#define RTL_CBR         0x3A    /* Current buffer address */
#define RTL_IMR         0x3C    /* Interrupt mask register */
#define RTL_ISR         0x3E    /* Interrupt status register */
#define RTL_TCR         0x40    /* Tx config register */
#define RTL_RCR         0x44    /* Rx config register */
#define RTL_CONFIG1     0x52    /* Config register 1 */

/* ── CMD bits ───────────────────────────────────────────────────────────── */
#define CMD_RST         0x10
#define CMD_RE          0x08    /* Rx enable */
#define CMD_TE          0x04    /* Tx enable */

/* ── ISR/IMR bits ───────────────────────────────────────────────────────── */
#define INT_ROK         0x0001  /* Rx OK */
#define INT_TOK         0x0004  /* Tx OK */
#define INT_TER         0x0008  /* Tx error */
#define INT_RER         0x0002  /* Rx error */

/* ── RCR bits ───────────────────────────────────────────────────────────── */
#define RCR_AAP         (1 << 0)    /* Accept all packets */
#define RCR_APM         (1 << 1)    /* Accept physical match */
#define RCR_AM          (1 << 2)    /* Accept multicast */
#define RCR_AB          (1 << 3)    /* Accept broadcast */
#define RCR_WRAP        (1 << 7)    /* Wrap Rx buffer */
#define RCR_RBLEN_8K    (0 << 11)   /* 8KB Rx buffer */
#define RCR_MXDMA_UNLIM (7 << 8)    /* Unlimited DMA burst */

/* ── Tx descriptor count and Rx buffer size ─────────────────────────────── */
#define TX_DESC_COUNT   4
#define RX_BUF_SIZE     (8192 + 16 + 1500)  /* 8K + header + overflow guard */

/* ── Static DMA buffers (must be physically contiguous, identity-mapped) ── */
static BYTE  g_RxBuf[RX_BUF_SIZE]          __attribute__((aligned(4)));
static BYTE  g_TxBuf[TX_DESC_COUNT][1536]  __attribute__((aligned(4)));

/* ── Driver state ───────────────────────────────────────────────────────── */
static struct {
    BOOL        Available;
    WORD        IoBase;
    BYTE        Irq;
    MAC_ADDR    Mac;
    DWORD       TxCurrent;      /* next Tx descriptor to use */
    DWORD       RxOffset;       /* current read offset in Rx ring */
} g_Nic;

/* ── Rx ring buffer ─────────────────────────────────────────────────────── */
/* Receive queue for upper layer */
#define RX_QUEUE_SIZE   8
static BYTE  g_RxQueue[RX_QUEUE_SIZE][ETH_FRAME_MAX];
static WORD  g_RxQueueLen[RX_QUEUE_SIZE];
static volatile DWORD g_RxHead = 0;
static volatile DWORD g_RxTail = 0;

/* ── _NicRead8 / _NicRead16 / _NicRead32 ───────────────────────────────── */
static inline BYTE  _NicRead8 (BYTE reg) { return HalReadPortByte ((WORD)(g_Nic.IoBase + reg)); }
static inline WORD  _NicRead16(BYTE reg) { return HalReadPortWord ((WORD)(g_Nic.IoBase + reg)); }
static inline DWORD _NicRead32(BYTE reg) { return HalReadPortDword((WORD)(g_Nic.IoBase + reg)); }
static inline VOID  _NicWrite8 (BYTE reg, BYTE  v) { HalWritePortByte ((WORD)(g_Nic.IoBase + reg), v); }
static inline VOID  _NicWrite16(BYTE reg, WORD  v) { HalWritePortWord ((WORD)(g_Nic.IoBase + reg), v); }
static inline VOID  _NicWrite32(BYTE reg, DWORD v) { HalWritePortDword((WORD)(g_Nic.IoBase + reg), v); }

/* RTL8139 DMA registers hold *physical* addresses (32-bit bus master). */
static NTSTATUS _NicSetDmaPhys(PVOID Virt, DWORD* OutLow32) {
    ULONG_PTR phys = MmGetPhysicalAddress((ULONG_PTR)Virt);
    if (phys == 0) return STATUS_DEVICE_CONFIGURATION_ERROR;
    if (phys >= 0x100000000ULL) return STATUS_INSUFFICIENT_RESOURCES;
    *OutLow32 = (DWORD)phys;
    return STATUS_SUCCESS;
}

/* ── IRQ handler ────────────────────────────────────────────────────────── */
static VOID _NicIrqHandler(PVOID Frame) {
    (VOID)Frame;
    WORD isr = _NicRead16(RTL_ISR);
    _NicWrite16(RTL_ISR, isr);   /* ACK all */

    if (isr & INT_ROK) {
        /* Drain Rx ring */
        while (!(_NicRead8(RTL_CMD) & 0x01)) {   /* while buffer not empty */
            DWORD offset = g_Nic.RxOffset % (8192 + 16);
            BYTE* hdr    = g_RxBuf + offset;

            /* RTL8139 Rx packet header: status(2) + length(2) */
            WORD pkt_len = (WORD)((hdr[3] << 8) | hdr[2]);
            if (pkt_len < 4 || pkt_len > ETH_FRAME_MAX + 4) {
                /* Bad packet — reset offset */
                g_Nic.RxOffset = (DWORD)_NicRead16(RTL_CBR);
                _NicWrite16(RTL_CAPR, (WORD)(g_Nic.RxOffset - 16));
                break;
            }

            WORD data_len = (WORD)(pkt_len - 4);  /* strip CRC */
            BYTE* data    = hdr + 4;

            /* Enqueue if space available */
            DWORD next = (g_RxTail + 1) % RX_QUEUE_SIZE;
            if (next != g_RxHead && data_len <= ETH_FRAME_MAX) {
                /* Handle wrap-around in ring buffer */
                DWORD avail = (8192 + 16) - (offset + 4);
                if (avail >= (DWORD)data_len) {
                    RtlCopyMemory(g_RxQueue[g_RxTail], data, data_len);
                } else {
                    RtlCopyMemory(g_RxQueue[g_RxTail], data, avail);
                    RtlCopyMemory(g_RxQueue[g_RxTail] + avail, g_RxBuf, (SIZE_T)(data_len - (WORD)avail));
                }
                g_RxQueueLen[g_RxTail] = data_len;
                g_RxTail = next;
            }

            /* Advance CAPR — must be DWORD aligned, subtract 16 per spec */
            g_Nic.RxOffset = (g_Nic.RxOffset + 4 + pkt_len + 3) & ~3u;
            _NicWrite16(RTL_CAPR, (WORD)((g_Nic.RxOffset - 16) & 0xFFFF));
        }
    }
}

/* ── NetInitialize ──────────────────────────────────────────────────────── */
NTSTATUS NetInitialize(VOID) {
    NTSTATUS dma_st;
    RtlZeroMemory(&g_Nic, sizeof(g_Nic));
    g_Nic.Available = FALSE;

    /* Find RTL8139 on PCI bus */
    PCI_DEVICE dev;
    if (!PciFindDevice(RTL8139_VENDOR, RTL8139_DEVICE, &dev)) {
        return STATUS_SUCCESS; /* Optional device: continue boot without NIC */
    }

    /* Get I/O BAR0 (bit 0 = I/O space indicator, mask it off) */
    DWORD bar0 = dev.BAR[0];
    if (!(bar0 & 0x01)) {
        /* Not an I/O BAR — unsupported config */
        return STATUS_DEVICE_CONFIGURATION_ERROR;
    }
    g_Nic.IoBase = (WORD)(bar0 & 0xFFFC);
    g_Nic.Irq    = dev.InterruptLine;

    /* Enable bus mastering + I/O space */
    PciEnableBusMaster(&dev);

    /* Power on */
    _NicWrite8(RTL_CONFIG1, 0x00);

    /* Software reset */
    _NicWrite8(RTL_CMD, CMD_RST);
    DWORD timeout = 1000000;
    while ((_NicRead8(RTL_CMD) & CMD_RST) && --timeout) {
        __asm__ volatile ("pause");
    }
    if (timeout == 0) return STATUS_TIMEOUT;

    /* Read MAC address */
    for (BYTE i = 0; i < ETH_ALEN; i++)
        g_Nic.Mac.b[i] = _NicRead8(i);

    /* Set Rx / Tx DMA buffers (must be below 4G and identity-reachable by bus) */
    DWORD phys_rx;
    dma_st = _NicSetDmaPhys(g_RxBuf, &phys_rx);
    if (!NT_SUCCESS(dma_st)) return dma_st;
    _NicWrite32(RTL_RBSTART, phys_rx);

    for (BYTE i = 0; i < TX_DESC_COUNT; i++) {
        DWORD phys_tx;
        dma_st = _NicSetDmaPhys(g_TxBuf[i], &phys_tx);
        if (!NT_SUCCESS(dma_st)) return dma_st;
        _NicWrite32((BYTE)(RTL_TSAD0 + i * 4), phys_tx);
    }

    /* Enable Rx + Tx */
    _NicWrite8(RTL_CMD, CMD_RE | CMD_TE);

    /* Configure Rx: accept broadcast + physical match, 8K buffer, no wrap */
    _NicWrite32(RTL_RCR, RCR_AB | RCR_APM | RCR_AM | RCR_MXDMA_UNLIM | RCR_RBLEN_8K);

    /* Configure Tx: max DMA burst 2048, no loopback */
    _NicWrite32(RTL_TCR, (6 << 8));

    /* Enable Rx OK + Tx OK interrupts */
    _NicWrite16(RTL_IMR, INT_ROK | INT_TOK | INT_RER | INT_TER);

    /* Register IRQ handler only for legacy PIC-routable IRQs (0..15). */
    if (g_Nic.Irq < 16) {
        KeRegisterIrqHandler(g_Nic.Irq, _NicIrqHandler);
        HalPicUnmaskIrq(g_Nic.Irq);
        if (g_Nic.Irq >= 8) HalPicUnmaskIrq(2); /* slave PIC cascade */
    }

    g_Nic.RxOffset  = 0;
    g_Nic.TxCurrent = 0;
    g_Nic.Available = TRUE;

    (VOID)IoRegisterLoadedDriver("RTL8139 NDIS Miniport");

    return STATUS_SUCCESS;
}

/* ── NetIsAvailable ─────────────────────────────────────────────────────── */
BOOL NetIsAvailable(VOID) {
    return g_Nic.Available;
}

/* ── NetSendFrame ───────────────────────────────────────────────────────── */
NTSTATUS NetSendFrame(const BYTE* Data, WORD Len) {
    if (!g_Nic.Available) return STATUS_DEVICE_NOT_CONNECTED;
    if (Len > 1536)       return STATUS_INVALID_PARAMETER;

    DWORD desc = g_Nic.TxCurrent;

    /* Copy into Tx buffer */
    RtlCopyMemory(g_TxBuf[desc], Data, Len);

    /* Write TSD: clear OWN bit (bit 13), set size */
    BYTE tsd_reg = (BYTE)(RTL_TSD0 + desc * 4);
    _NicWrite32(tsd_reg, (DWORD)Len);

    /* Poll for completion (TOK or TUN) */
    DWORD timeout = 1000000;
    while (!(_NicRead32(tsd_reg) & 0x8000) && --timeout) {
        __asm__ volatile ("pause");
    }

    g_Nic.TxCurrent = (desc + 1) % TX_DESC_COUNT;

    return (timeout > 0) ? STATUS_SUCCESS : STATUS_TIMEOUT;
}

/* ── NetReceiveFrame ────────────────────────────────────────────────────── */
BOOL NetReceiveFrame(BYTE* Buffer, WORD* Len) {
    if (g_RxHead == g_RxTail) return FALSE;

    *Len = g_RxQueueLen[g_RxHead];
    RtlCopyMemory(Buffer, g_RxQueue[g_RxHead], *Len);
    g_RxHead = (g_RxHead + 1) % RX_QUEUE_SIZE;
    return TRUE;
}

/* ── NetGetMac ──────────────────────────────────────────────────────────── */
VOID NetGetMac(MAC_ADDR* Out) {
    if (Out) *Out = g_Nic.Mac;
}

/* ── NetPrintStatus ─────────────────────────────────────────────────────── */
VOID NetPrintStatus(VOID) {
    if (!g_Nic.Available) {
        KdPrintColor("Network: No RTL8139 adapter found.\n", CON_RED, CON_BLACK);
        return;
    }

    MAC_ADDR mac = g_Nic.Mac;
    KdPrintColor("Network Adapter: RTL8139\n", CON_WHITE, CON_BLACK);
    KdPrintf("  I/O Base:  0x%04X\n", (DWORD)g_Nic.IoBase);
    KdPrintf("  IRQ:       %u\n",     (DWORD)g_Nic.Irq);
    KdPrintf("  MAC:       %02X-%02X-%02X-%02X-%02X-%02X\n",
             (DWORD)mac.b[0], (DWORD)mac.b[1], (DWORD)mac.b[2],
             (DWORD)mac.b[3], (DWORD)mac.b[4], (DWORD)mac.b[5]);

    /* Link status: bit 2 of CMD = Tx enabled means link is up */
    BYTE cmd = _NicRead8(RTL_CMD);
    KdPrintf("  Link:      %s\n", (cmd & CMD_TE) ? "Up" : "Down");
}
